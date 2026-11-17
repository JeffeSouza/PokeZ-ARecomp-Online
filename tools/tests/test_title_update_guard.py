import struct
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from za_patches import TITLE_UPDATE_OFFSET, za_patches


class TitleUpdateGuardTest(unittest.TestCase):
    def test_only_game_202_gets_the_guard(self):
        build = 'b1f12fd919eae86a'
        addr = TITLE_UPDATE_OFFSET[build]
        gen = SimpleNamespace(base=addr, nso=SimpleNamespace(image=struct.pack('<I', 0x3dc01001)))
        self.assertEqual(za_patches(build)[addr](gen, addr),
                         'c->v[1] = c->x[0] ? LD128(c->x[0] + 0x40ull) : 0;')
        for other in ('7222e13ecf6adb32', '8a1c86c437394b69'):
            self.assertNotIn(addr, za_patches(other))

    def test_rejects_a_changed_instruction(self):
        build = 'b1f12fd919eae86a'
        addr = TITLE_UPDATE_OFFSET[build]
        gen = SimpleNamespace(base=addr, nso=SimpleNamespace(image=struct.pack('<I', 0)))
        with self.assertRaises(ValueError):
            za_patches(build)[addr](gen, addr)

    def test_generated_c_handles_missing_and_present_entries(self):
        compiler = os.environ.get('TITLE_GUARD_CC')
        if not compiler:
            self.skipTest('TITLE_GUARD_CC not set')
        addr = TITLE_UPDATE_OFFSET['b1f12fd919eae86a']
        gen = SimpleNamespace(base=addr, nso=SimpleNamespace(image=struct.pack('<I', 0x3dc01001)))
        instruction = za_patches('b1f12fd919eae86a')[addr](gen, addr)
        program = '''#include <stdint.h>
#include <string.h>
typedef unsigned __int128 u128;
typedef struct { uint64_t x[31]; u128 v[32]; } Cpu;
static u128 read128(uint64_t address) { u128 v; memcpy(&v, (void *)(uintptr_t)address, 16); return v; }
#define LD128 read128
static void guarded(Cpu *c) { ''' + instruction + ''' }
int main(void) {
    Cpu c = {0}; c.v[1] = 123; guarded(&c);
    if (c.v[1] != 0) return 1;
    unsigned char entry[0x50] = {0}; u128 expected = ((u128)42 << 64) | 99;
    memcpy(entry + 0x40, &expected, 16); c.x[0] = (uintptr_t)entry; guarded(&c);
    return c.v[1] != expected;
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            src, exe = Path(tmp) / 'guard.c', Path(tmp) / 'guard.exe'
            src.write_text(program, encoding='utf-8')
            subprocess.run([compiler, str(src), '-o', str(exe)], check=True, capture_output=True)
            subprocess.run([str(exe)], check=True, capture_output=True)


if __name__ == '__main__':
    unittest.main()
