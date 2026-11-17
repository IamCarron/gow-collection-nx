# Junta os .spv de uma pasta em shaders.pak (lido pelo spv_read do hle_gxm.cpp).
# Uso: python pack_shaders.py <pasta com .spv> <saida/shaders.pak>
import os, struct, sys
src, out = sys.argv[1], sys.argv[2]
names = sorted(n for n in os.listdir(src) if n.endswith('.spv') and not n.endswith('.pbr.spv') and len(n.encode()) <= 28)
blobs = [open(os.path.join(src, n), 'rb').read() for n in names]
off = 12 + len(names) * 40
idx, data = b'', []
for n, b in zip(names, blobs):
    idx += struct.pack('<28sIQ', n.encode(), len(b), off)
    data.append(b)
    off += len(b)
with open(out, 'wb') as f:
    f.write(b'SPVPAK1\0' + struct.pack('<I', len(names)) + idx)
    for b in data: f.write(b)
print(len(names), 'shaders,', off, 'bytes')
