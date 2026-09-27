"""Read PE headers without loading/executing the DLL; reject runtime dependencies."""
import pathlib
import struct
import sys

path = pathlib.Path(sys.argv[1])
data = path.read_bytes()
u16 = lambda offset: struct.unpack_from('<H', data, offset)[0]
u32 = lambda offset: struct.unpack_from('<I', data, offset)[0]
assert data[:2] == b'MZ', 'Not a PE file'
pe = u32(0x3C)
assert data[pe:pe+4] == b'PE\0\0'
assert u16(pe+4) == 0x14C, 'DLL must target x86'
optional = pe+24
assert u16(optional) == 0x10B, 'Expected PE32'
assert u16(pe+22) & 0x2000, 'Missing DLL characteristic'
sections = []
for index in range(u16(pe+6)):
    start = optional + u16(pe+20) + index*40
    sections.append((u32(start+12), max(u32(start+8), u32(start+16)), u32(start+20)))

def offset(rva):
    for address, size, raw in sections:
        if address <= rva < address+size:
            return raw+rva-address
    raise AssertionError(f'Unmapped RVA {rva:x}')

def string(rva):
    start = offset(rva)
    return data[start:data.index(b'\0', start)].decode('ascii')

imports = []
descriptor = offset(u32(optional+104))
while u32(descriptor+12):
    imports.append(string(u32(descriptor+12)))
    descriptor += 20
allowed = {'kernel32.dll', 'msvcrt.dll', 'user32.dll', 'advapi32.dll', 'ntdll.dll'}
# Zig's Windows target uses the OS Universal CRT API sets (Windows 10/11).
allowed.update('api-ms-win-crt-' + component + '-l1-1-0.dll' for component in
               ('convert', 'locale', 'private', 'runtime', 'stdio', 'string', 'time', 'environment', 'heap'))
assert all(item.lower() in allowed for item in imports), imports
print('PE32 x86 DLL; system imports only:', ', '.join(imports))
