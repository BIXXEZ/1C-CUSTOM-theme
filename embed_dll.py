from pathlib import Path
import sys

root = Path(__file__).resolve().parent
dll = root / "bin" / "Debug" / "DarkModDLL.dll"
out = root / "DarkModUI" / "EmbeddedDLL.h"

if not dll.exists():
    print(f"ERROR: DLL not found: {dll}")
    print("Build DarkModDLL x64 Debug first.")
    sys.exit(1)

data = dll.read_bytes()

with out.open("w", encoding="utf-8", newline="\n") as f:
    f.write("#pragma once\n")
    f.write("#include <cstddef>\n\n")
    f.write("static const unsigned char g_embeddedDll[] = {\n")

    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        f.write("    ")
        f.write(", ".join(f"0x{b:02X}" for b in chunk))
        if i + 16 < len(data):
            f.write(",")
        f.write("\n")

    f.write("};\n")
    f.write(f"static const std::size_t g_embeddedDllSize = {len(data)};\n")

print(f"Embedded {len(data):,} bytes into {out}")
