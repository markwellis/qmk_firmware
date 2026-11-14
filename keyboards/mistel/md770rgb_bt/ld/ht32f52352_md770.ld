/*
 * Copyright (c) 2013-2016 Fabio UJonathan A. Kollaschtzig, http://fabioutzig.com
 *           (c) 2020 Yaotian Feng (Codetector) <codetector@codetector.cn>
 *           (c) 2025 Michał Kopeć <michal@nozomi.space>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
/* Derived from keyboards/annepro2/ld/HT32F52352_ANNEPRO2_C18.ld. */
__vectors_align__ = 256;
MEMORY {
    flash0 : org = 0x00003200, len = 0x3bf8 - 0x3200
    flash1 : org = 0x00003bf8, len = 12
    flash2 : org = 0x00003c04, len = 0x1fe00 - 0x3c04
    flash3 : org = 0, len = 0
    flash4 : org = 0, len = 0
    flash5 : org = 0, len = 0
    flash6 : org = 0, len = 0
    flash7 : org = 0, len = 0
    ram0 : org = 0x20000000, len = 16k - 32
    ram1 : org = 0, len = 0
    ram2 : org = 0, len = 0
    ram3 : org = 0, len = 0
    ram4 : org = 0, len = 0
    ram5 : org = 0, len = 0
    ram6 : org = 0, len = 0
    ram7 : org = 0, len = 0
}
REGION_ALIAS("VECTORS_FLASH", flash0);
REGION_ALIAS("VECTORS_FLASH_LMA", flash0);
REGION_ALIAS("XTORS_FLASH", flash0);
REGION_ALIAS("XTORS_FLASH_LMA", flash0);
REGION_ALIAS("TEXT_FLASH", flash2);
REGION_ALIAS("TEXT_FLASH_LMA", flash2);
REGION_ALIAS("RODATA_FLASH", flash2);
REGION_ALIAS("RODATA_FLASH_LMA", flash2);
REGION_ALIAS("VARIOUS_FLASH", flash0);
REGION_ALIAS("VARIOUS_FLASH_LMA", flash0);
REGION_ALIAS("RAM_INIT_FLASH_LMA", flash0);
REGION_ALIAS("MAIN_STACK_RAM", ram0);
REGION_ALIAS("PROCESS_STACK_RAM", ram0);
REGION_ALIAS("DATA_RAM", ram0);
REGION_ALIAS("DATA_RAM_LMA", flash0);
REGION_ALIAS("BSS_RAM", ram0);
REGION_ALIAS("HEAP_RAM", ram0);
SECTIONS {
    .md770_app_record : {
        LONG(DEFINED(md770_left) ? 0x04046080 : 0x040450c0)
        LONG(0x34000001) LONG(0x00020004)
    } > flash1
}
INCLUDE rules.ld
ASSERT(ADDR(.vectors) == 0x3200, "MD770 vectors must start at 0x3200")
ASSERT(ADDR(.md770_app_record) == 0x3bf8 && SIZEOF(.md770_app_record) == 12, "MD770 compatibility record must occupy 0x3bf8..0x3c03")
ASSERT(__ram0_end__ == 0x20003fe0, "MD770 bootloader SRAM must remain reserved")
