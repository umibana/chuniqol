"""Read-only PE string/xref/disassembly helper. Never loads the game executable."""
import argparse
import hashlib
import pathlib
import re
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parent
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

parser = argparse.ArgumentParser()
parser.add_argument('--exe', type=pathlib.Path, required=True, help='Local game EXE; never copy it into this repo')
parser.add_argument('--va', type=lambda value: int(value, 0))
parser.add_argument('--size', type=int, default=256)
parser.add_argument('--strings', help='Case-insensitive regex; show text references too')
args = parser.parse_args()
data = args.exe.read_bytes()
u16 = lambda offset: struct.unpack_from('<H', data, offset)[0]
u32 = lambda offset: struct.unpack_from('<I', data, offset)[0]
pe = u32(60)
assert data[:2] == b'MZ' and data[pe:pe+4] == b'PE\0\0'
assert u16(pe+4) == 0x14c
optional = pe+24
base = u32(optional+28)
sections = []
for index in range(u16(pe+6)):
    offset = optional+u16(pe+20)+index*40
    name = data[offset:offset+8].split(b'\0')[0].decode('ascii')
    virtual_size, rva, raw_size, raw = struct.unpack_from('<IIII', data, offset+8)
    sections.append((name, base+rva, raw_size, raw))

def file_offset(address):
    for _, va, size, raw in sections:
        if va <= address < va+size:
            return raw+address-va
    raise ValueError(f'Unmapped virtual address: {address:x}')

def virtual_address(offset):
    for _, va, size, raw in sections:
        if raw <= offset < raw+size:
            return va+offset-raw
    raise ValueError(f'Unmapped file offset: {offset:x}')

print('SHA256:', hashlib.sha256(data).hexdigest(), 'preferred base:', hex(base))
if args.strings:
    pattern = re.compile(args.strings, re.I)
    for match in re.finditer(rb'[ -~]{4,}', data):
        text = match[0].decode('ascii')
        if not pattern.search(text):
            continue
        address = virtual_address(match.start())
        needle = struct.pack('<I', address)
        references = []
        for name, va, size, raw in sections:
            if name != '.text':
                continue
            references.extend(hex(va+m.start()) for m in re.finditer(re.escape(needle), data[raw:raw+size]))
        print(hex(address), text, 'raw operand references:', ', '.join(references))
if args.va is not None:
    va = args.va
    for _ in range(8):
        offset = file_offset(va)
        if data[offset] != 0xe9:
            break
        target = va+5+struct.unpack_from('<i', data, offset+1)[0]
        print('Jump thunk:', hex(va), '->', hex(target))
        va = target
    offset = file_offset(va)
    for instruction in Cs(CS_ARCH_X86, CS_MODE_32).disasm(data[offset:offset+args.size], va):
        print(f'{instruction.address:08x}  {instruction.mnemonic:8s} {instruction.op_str}')
        if instruction.mnemonic == 'ret':
            break
