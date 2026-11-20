import hashlib
import sys

src, dst = sys.argv[1], sys.argv[2]
expected = "48084d9fa53b9bb04358f3bb127b7495dc8f7bb0b3ca1437bd24ef2b6eabdf66"
data = open(src, "rb").read()
digest = hashlib.sha256(data).hexdigest()
if digest != expected:
    sys.exit(f"firmware hash mismatch: {digest}")
with open(dst, "w", newline="\n") as f:
    f.write("#include <stdint.h>\n#include <stddef.h>\n")
    f.write(f"const size_t xbox_dongle_fw_len = {len(data)};\n")
    f.write("const uint8_t xbox_dongle_fw[] = {\n")
    for i in range(0, len(data), 16):
        f.write("  " + ", ".join(f"0x{b:02x}" for b in data[i:i + 16]) + ",\n")
    f.write("};\n")
print(f"wrote {dst}: {len(data)} bytes")
