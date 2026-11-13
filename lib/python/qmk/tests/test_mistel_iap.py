"""Mistel updater transactions against a synthetic device."""
import json
import struct
import tempfile
import unittest
import zlib
from pathlib import Path
from unittest.mock import patch

from qmk import bootloader_mistel as mistel


def application(product=0x0419):
    image = bytearray(4096)
    struct.pack_into('<II', image, 0, 0x20000400, 0x3C81)
    struct.pack_into('<III', image, 0x9F8, mistel.APPLICATIONS[product], 0x34000001, 0x00020004)
    return bytes(image)


class Updater:
    def __init__(self, product=0x0419, mode=0):
        self.product = product
        self.mode = mode
        info = bytearray(52)
        struct.pack_into('<HHI', info, 8, mistel.VENDOR_ID, product | 0x1000, mistel.APPLICATIONS[product])
        struct.pack_into('<HHH', info, 32, 0x200, 231, 512)
        self.info = bytes(info)
        version = bytearray(b'\xff' * 480)
        struct.pack_into('<I', version, 0x78, 0)
        struct.pack_into('<HH', version, 0x90, mistel.VENDOR_ID, product)
        struct.pack_into('<I', version, 0xB0, (1 << 18) | 0x5AA5)
        self.version = bytes(version)
        self.memory = bytearray(b'\xff' * mistel.UPDATE_SIZE)
        self.memory[:480] = self.version
        self.pointer = 0
        self.reply = None
        self.commands = []
        self.regions = []
        self.started = False
        self.bad_checksum = False
        self.bad_erase = False
        self.bad_version = False
        self.erase_check = lambda: None

    def selected(self):
        return {'path': b'synthetic-device', 'product_id': self.product | (0x1000 if self.mode == 0 else 0)}

    def write(self, packet):
        assert len(packet) == 65 and packet[0] == 0
        command, subcommand, status = struct.unpack_from('<BBH', packet, 1)
        assert status == 0
        data = packet[5:]
        self.commands.append((command, subcommand))
        expected = (command, subcommand)
        payload = b''
        if command == 0x12:
            if subcommand == 0:
                payload = self.info
            elif subcommand == 2:
                payload = bytes([self.mode])
            else:
                start = (subcommand - 0x20) * 60
                payload = bytes(self.memory[start:start + 60])
                if self.bad_version and self.regions and self.regions[-1][0] < mistel.APPLICATION_OFFSET:
                    payload = b'\0' * 60
        elif command == 0x11:
            self.mode = 0 if subcommand == 0 else 1
            self.started = subcommand == 1
            self.reply = None
            return len(packet)
        elif command == 0x1D and subcommand == 0:
            self.erase_check()
            assert struct.unpack_from('<II', data) == (0, mistel.UPDATE_SIZE)
            self.memory[:] = b'\xff' * len(self.memory)
            payload = struct.pack('<II', 0, mistel.UPDATE_SIZE - int(self.bad_erase))
        elif command == 0x1D:
            start, length = struct.unpack_from('<II', data)
            encoded = self.memory[start:start + length]
            value = sum(word[0] for word in struct.iter_unpack('<I', encoded)) & 0xFFFFFFFF if subcommand == 1 else zlib.crc32(encoded)
            payload = struct.pack('<I', value ^ int(self.bad_checksum and subcommand == 2))
        elif command == 0x1E:
            if subcommand == 1:
                self.pointer = struct.unpack_from('<I', data)[0]
            payload = struct.pack('<I', self.pointer)
        elif command == 0x1F:
            assert subcommand <= 52 and subcommand % 4 == 0
            self.regions.append((self.pointer, subcommand))
            self.memory[self.pointer:self.pointer + subcommand] = data[:subcommand]
            self.pointer += subcommand
            expected = (0x1E, 2)
            payload = struct.pack('<I', self.pointer)
        else:
            raise AssertionError((command, subcommand))
        self.reply = (struct.pack('<BBH', *expected, 0) + payload).ljust(64, b'\0')
        return len(packet)

    def read(self, size, timeout):
        assert size == 64 and timeout > 0 and self.reply is not None
        return self.reply

    def close(self):
        pass


class MistelIAPTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.image = Path(self.directory.name) / 'application.bin'
        self.image.write_bytes(application())
        self.backup = self.image.with_suffix('.bin.iap-backup.json')
        self.device = Updater()

    def run_flash(self):
        with patch.object(mistel.hid, 'Device', return_value=self.device), patch.object(mistel.hid, 'enumerate', side_effect=lambda vid, pid: [self.device.selected()]
                                                                                        if self.device.started and pid == self.device.product else []), patch.object(mistel, 'find_device',
                                                                                                                                                                     side_effect=self.device.selected), patch.object(mistel.time, 'sleep'):
            mistel.flash(self.image, self.device.selected())

    def test_backup_precedes_erase_and_validity_marker_follows_verification(self):
        self.device.erase_check = lambda: self.assertEqual(json.loads(self.backup.read_text())['version_page_hex'], self.device.version.hex())
        self.run_flash()
        self.assertTrue(self.device.started)
        self.assertEqual(self.device.memory[:480], self.device.version)
        self.assertEqual(self.device.regions[-1], (0xB0, 4))
        self.assertEqual(self.device.commands[-1], (0x11, 1))

    def test_stock_application_enters_updater_without_waiting_for_reset_reply(self):
        self.device.mode = 1
        self.run_flash()
        self.assertIn((0x11, 0), self.device.commands)
        self.assertTrue(self.device.started)

    def test_left_half_requires_a_left_application(self):
        self.device = Updater(0x041A)
        with self.assertRaisesRegex(ValueError, 'different half'):
            self.run_flash()
        self.assertEqual(self.device.commands, [])
        self.image.write_bytes(application(0x041A))
        self.run_flash()
        self.assertTrue(self.device.started)

    def test_unknown_geometry_never_erases(self):
        self.device.info = self.device.info[:32] + b'\0' * 20
        with self.assertRaisesRegex(ValueError, 'geometry'):
            self.run_flash()
        self.assertNotIn((0x1D, 0), self.device.commands)

    def test_metadata_save_failure_never_erases(self):
        with patch.object(mistel.os, 'replace', side_effect=OSError('disk failure')), self.assertRaises(OSError):
            self.run_flash()
        self.assertNotIn((0x1D, 0), self.device.commands)

    def test_checksum_failure_never_commits_or_boots(self):
        self.device.bad_checksum = True
        with self.assertRaisesRegex(RuntimeError, 'checksum'):
            self.run_flash()
        self.assertTrue(self.backup.exists())
        self.assertTrue(all(offset >= 0x200 for offset, _ in self.device.regions))
        self.assertFalse(self.device.started)

    def test_retry_uses_saved_metadata_after_an_interrupted_erase(self):
        self.device.bad_checksum = True
        with self.assertRaises(RuntimeError):
            self.run_flash()
        self.device.bad_checksum = False
        self.run_flash()
        self.assertEqual(self.device.memory[:480], self.device.version)
        self.assertTrue(self.device.started)

    def test_corrupt_version_readback_never_commits_marker(self):
        self.device.bad_version = True
        with self.assertRaisesRegex(RuntimeError, 'before committing'):
            self.run_flash()
        self.assertNotIn((0xB0, 4), self.device.regions)
        self.assertFalse(self.device.started)

    def test_partial_erase_acknowledgement_never_writes(self):
        self.device.bad_erase = True
        with self.assertRaisesRegex(RuntimeError, 'complete erase'):
            self.run_flash()
        self.assertEqual(self.device.regions, [])
        self.assertFalse(self.device.started)

    def test_invalid_image_cannot_open_a_device(self):
        self.image.write_bytes(b'\0' * 4096)
        with patch.object(mistel.hid, 'Device') as opened, self.assertRaises(ValueError):
            mistel.flash(self.image, self.device.selected())
        opened.assert_not_called()

    def test_stock_discovery_requires_a_compatible_image(self):
        stock = dict(self.device.selected(), product_id=0x0419, usage_page=0xFF00)
        with patch.object(mistel.hid, 'enumerate', return_value=[stock, stock]):
            self.assertIsNone(mistel.find_device())
            with self.assertRaises(ValueError):
                mistel.find_device(include_application=True)
        self.assertTrue(mistel.supports_image(self.image))
        self.image.write_bytes(b'\0' * 4096)
        self.assertFalse(mistel.supports_image(self.image))

    def test_invalid_backup_never_erases(self):
        self.device.memory[:480] = b'\xff' * 480
        self.backup.write_text('{}')
        with self.assertRaisesRegex(ValueError, 'Invalid Mistel metadata backup'):
            self.run_flash()
        self.assertNotIn((0x1D, 0), self.device.commands)

    def test_incomplete_report_or_write_aborts(self):
        with patch.object(self.device, 'write', return_value=1), self.assertRaisesRegex(RuntimeError, 'Incomplete'):
            mistel._request(self.device, 0x12, 0)
        with patch.object(self.device, 'read', return_value=b''), self.assertRaisesRegex(RuntimeError, 'failed or timed out'):
            mistel._request(self.device, 0x12, 0)

    def test_another_application_cannot_satisfy_return_verification(self):
        with patch.object(mistel.hid, 'Device', return_value=self.device), patch.object(mistel.hid, 'enumerate', return_value=[self.device.selected()]):
            with self.assertRaisesRegex(ValueError, 'Disconnect other'):
                mistel.flash(self.image, self.device.selected())
        self.assertNotIn((0x1D, 0), self.device.commands)
        self.assertFalse(self.device.started)

    def test_generic_routing_does_not_select_a_stock_keyboard(self):
        from qmk import flashers
        stock = dict(self.device.selected(), product_id=0x0419, usage_page=0xFF00)
        with patch.object(mistel.hid, 'enumerate', return_value=[stock]), patch.object(flashers, '_find_usb_device', side_effect=lambda vid, pid: object() if pid == 0x6124 else None):
            self.assertEqual(flashers._find_bootloader(), ('md-boot', None))

    def test_generic_flash_enables_stock_discovery_only_for_matching_images(self):
        from qmk import flashers
        with patch.object(flashers, '_find_bootloader', return_value=('mistel-iap', self.device.selected())) as discover, patch.object(flashers, '_flash_mistel', return_value=(False, None)):
            self.assertEqual(flashers.flasher(None, self.image), (False, None))
            discover.assert_called_once_with(mistel_application=True)
        self.image.write_bytes(b'\0' * 4096)
        with patch.object(flashers, '_find_bootloader', return_value=('md-boot', None)) as discover, patch.object(flashers, '_flash_known_bootloader', return_value=(False, None)):
            flashers.flasher(None, self.image)
            discover.assert_called_once_with(mistel_application=False)

    def test_discovery_excludes_qmk_raw_hid_and_rejects_ambiguity(self):
        keyboard = dict(self.device.selected(), usage_page=1)
        via = dict(self.device.selected(), usage_page=0xFF60)
        updater = dict(self.device.selected(), usage_page=0xFF00)
        with patch.object(mistel.hid, 'enumerate', return_value=[keyboard, via, updater]):
            self.assertEqual(mistel.find_device(), updater)
        with patch.object(mistel.hid, 'enumerate', return_value=[updater, updater]), self.assertRaisesRegex(ValueError, 'only one'):
            mistel.find_device()
