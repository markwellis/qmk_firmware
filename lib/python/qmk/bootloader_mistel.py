"""Flash applications through the retained MD770 RGB BT updater."""
import json
import os
import struct
import tempfile
import time
import zlib
from pathlib import Path

import hid
from milc import cli

VENDOR_ID = 0x04D9
APPLICATIONS = {0x0419: 0x040450C0, 0x041A: 0x04046080}
UPDATE_SIZE = 0x1CE00
APPLICATION_OFFSET = 0x200
WRITE_SIZE = 52
VERSION_MARKER = 0xB0
XOR_KEY = struct.pack('<13I', 0xE7C29474, 0x79084B10, 0x53D54B0D, 0xFC1E8F32, 0x48E81A9B, 0x773C808E, 0xB7483552, 0xD9CB8C76, 0x2A8C8BC6, 0x0967ADA8, 0xD4520F5C, 0xD0C3279D, 0xEAC091C5)


def find_device(include_application=False):
    """Find one supported vendor interface, excluding QMK Raw HID."""
    products = {product | 0x1000 for product in APPLICATIONS}
    if include_application:
        products.update(APPLICATIONS)
    devices = [device for device in hid.enumerate(VENDOR_ID, 0) if device['product_id'] in products and device.get('usage_page') == 0xFF00]
    if len(devices) > 1:
        raise ValueError('Connect only one MD770 half to host USB while flashing.')
    return devices[0] if devices else None


def _send(device, command, subcommand, data=b''):
    payload = struct.pack('<BBH', command, subcommand, 0) + data
    if len(payload) > 64:
        raise ValueError('Mistel report exceeds 64 bytes.')
    if device.write(b'\0' + payload.ljust(64, b'\0')) != 65:
        raise RuntimeError('Incomplete Mistel USB write.')


def _request(device, command, subcommand, data=b'', expected=None):
    _send(device, command, subcommand, data)
    reply = bytes(device.read(64, timeout=15000))
    expected = expected or (command, subcommand)
    if len(reply) != 64 or reply[:4] != struct.pack('<BBH', *expected, 0):
        raise RuntimeError(f'Mistel command {command:02x}/{subcommand:02x} failed or timed out.')
    return reply[4:]


def _version_page(device):
    return b''.join(_request(device, 0x12, index) for index in range(0x20, 0x28))


def _validate_info(info, product_id):
    signature = APPLICATIONS[product_id]
    if len(info) != 52 or info[8:16] != struct.pack('<HHI', VENDOR_ID, product_id | 0x1000, signature):
        raise ValueError('Mistel boot information does not match the connected half.')
    if info[32:38] != struct.pack('<HHH', APPLICATION_OFFSET, 231, 512):
        raise ValueError('Unsupported Mistel update geometry.')


def _validate_image(image, product_id):
    if len(image) % 4 or not 0x1000 <= len(image) <= UPDATE_SIZE - APPLICATION_OFFSET:
        raise ValueError('Mistel application must be word-aligned and fit below the option page.')
    stack, reset = struct.unpack_from('<II', image)
    if stack % 8 or not 0x20000000 < stack <= 0x20003FE0:
        raise ValueError('Invalid Mistel application stack pointer.')
    if not reset & 1 or not 0x3200 <= (reset & ~1) < 0x3200 + len(image):
        raise ValueError('Invalid Mistel application reset vector.')
    if struct.unpack_from('<III', image, 0x9F8) != (APPLICATIONS[product_id], 0x34000001, 0x00020004):
        raise ValueError('Mistel application is for a different half or bootloader.')


def _read_image(filename):
    image = Path(filename).read_bytes()
    return image + b'\xff' * (-len(image) % 4)


def supports_image(filename):
    """Recognise the application compatibility record before stock-device discovery."""
    image = _read_image(filename)
    for product in APPLICATIONS:
        try:
            _validate_image(image, product)
            return True
        except ValueError:
            pass
    return False


def _validate_version(version, product_id):
    if len(version) != 480 or version[0xB4:] != b'\xff' * (480 - 0xB4):
        raise ValueError('Unsupported Mistel version page.')
    marker = struct.unpack_from('<I', version, VERSION_MARKER)[0]
    count = marker >> 18
    if marker & 0x3FFFF != 0x5AA5 or not 1 <= count <= 14:
        raise ValueError('Invalid Mistel version-page marker.')
    if any(struct.unpack_from('<I', version, 0x78 + index * 4)[0] == 0xFFFFFFFF for index in range(count)):
        raise ValueError('Incomplete Mistel version-page metadata.')
    if version[0x90:0x94] != struct.pack('<HH', VENDOR_ID, product_id):
        raise ValueError('Mistel version page belongs to a different half.')


def _saved_version(device, info, product_id, backup):
    current = _version_page(device)
    try:
        _validate_version(current, product_id)
    except ValueError:
        if not backup.exists():
            raise ValueError(f'Version page is incomplete; restore its metadata backup at {backup}.') from None
        try:
            saved = json.loads(backup.read_text(encoding='utf-8'))
            saved_info = bytes.fromhex(saved['boot_info_hex'])
            version = bytes.fromhex(saved['version_page_hex'])
        except (ValueError, KeyError, TypeError) as error:
            raise ValueError('Invalid Mistel metadata backup.') from error
        if saved_info != info:
            raise ValueError('Mistel metadata backup belongs to a different updater.')
        _validate_version(version, product_id)
        if any(actual not in (expected, 0xFF) for actual, expected in zip(current, version)):
            raise ValueError('Mistel version page differs from its metadata backup.')
        return version
    # Preserve the opaque vendor version page before erasing its update window.
    handle, temporary = tempfile.mkstemp(prefix='.', suffix='.iap-backup.json', dir=backup.parent)
    try:
        with os.fdopen(handle, 'w', encoding='utf-8') as output:
            json.dump({'boot_info_hex': info.hex(), 'version_page_hex': current.hex()}, output)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, backup)
        if os.name == 'posix':
            directory = os.open(backup.parent, os.O_RDONLY)
            try:
                os.fsync(directory)
            finally:
                os.close(directory)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)
    cli.log.info('Updater metadata saved to %s', backup)
    return current


def _write_region(device, offset, data):
    if offset < 0 or offset % 4 or len(data) % 4 or offset + len(data) > UPDATE_SIZE:
        raise ValueError('Invalid Mistel update range.')
    if offset < APPLICATION_OFFSET < offset + len(data):
        raise ValueError('Mistel write crosses the application boundary.')
    for subcommand, pointer in ((1, offset), (0, 0xFFFFFFFF)):
        reply = _request(device, 0x1E, subcommand, struct.pack('<I', pointer))
        if struct.unpack_from('<I', reply)[0] != offset:
            raise RuntimeError('Mistel update pointer did not match.')
    for index in range(0, len(data), WRITE_SIZE):
        chunk = data[index:index + WRITE_SIZE]
        reply = _request(device, 0x1F, len(chunk), chunk, expected=(0x1E, 2))
        if struct.unpack_from('<I', reply)[0] != offset + index + len(chunk):
            raise RuntimeError('Mistel write acknowledgement did not match.')


def _program(device, image, version):
    encoded = bytes(value ^ XOR_KEY[index % WRITE_SIZE] for index, value in enumerate(image))
    cli.log.info('Erasing the Mistel application and version page.')
    reply = _request(device, 0x1D, 0, struct.pack('<II', 0, UPDATE_SIZE))
    if struct.unpack_from('<II', reply) != (0, UPDATE_SIZE):
        raise RuntimeError('Mistel updater did not acknowledge the complete erase.')
    _write_region(device, APPLICATION_OFFSET, encoded)
    arguments = struct.pack('<II', APPLICATION_OFFSET, len(encoded))
    checksum = sum(word[0] for word in struct.iter_unpack('<I', encoded)) & 0xFFFFFFFF
    for subcommand, expected in ((1, checksum), (2, zlib.crc32(encoded))):
        if struct.unpack_from('<I', _request(device, 0x1D, subcommand, arguments))[0] != expected:
            raise RuntimeError('Mistel application checksum verification failed.')
    _write_region(device, 0, version[:VERSION_MARKER])
    if _version_page(device) != version[:VERSION_MARKER] + b'\xff' * (480 - VERSION_MARKER):
        raise RuntimeError('Mistel version page differs before committing its marker.')
    _write_region(device, VERSION_MARKER, version[VERSION_MARKER:VERSION_MARKER + 4])
    if _version_page(device) != version:
        raise RuntimeError('Mistel committed version page verification failed.')


def flash(filename, selected):
    """Validate and install one MD770 application, preserving the stock updater."""
    filename = Path(filename)
    image = _read_image(filename)
    product_id = selected['product_id'] & ~0x1000
    _validate_image(image, product_id)
    device = hid.Device(path=selected['path'])
    try:
        info = _request(device, 0x12, 0)[:52]
        _validate_info(info, product_id)
        mode = _request(device, 0x12, 2)[0]
        if mode != (1 if selected['product_id'] == product_id else 0):
            raise ValueError('Unexpected Mistel updater state.')
        version = _saved_version(device, info, product_id, filename.with_suffix(filename.suffix + '.iap-backup.json'))
        if mode:
            _send(device, 0x11, 0)
            device.close()
            device = None
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                candidate = find_device()
                if candidate and candidate['product_id'] == product_id | 0x1000:
                    device = hid.Device(path=candidate['path'])
                    break
                time.sleep(0.1)
            if device is None:
                raise RuntimeError('Mistel updater did not re-enumerate.')
            if _request(device, 0x12, 0)[:52] != info or _request(device, 0x12, 2)[0] != 0 or _version_page(device) != version:
                raise ValueError('Mistel updater identity or version page changed.')
        if hid.enumerate(VENDOR_ID, product_id):
            raise ValueError('Disconnect other MD770 halves from host USB before flashing.')
        _program(device, image, version)
        _send(device, 0x11, 1)
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            if hid.enumerate(VENDOR_ID, product_id):
                cli.log.info('Mistel application and version page verified; application returned over USB.')
                return
            time.sleep(0.1)
        raise RuntimeError('Mistel application did not return over USB.')
    finally:
        if device is not None:
            device.close()
