
from pathlib import Path
import base64
import textwrap

# Project paths
project = Path(__file__).resolve().parent
image_path = project / "assets" / "IICPlusNAM_clean_interactive_faceplate.png"
header_path = project / "Source" / "FaceplateImageData.h"

if not image_path.is_file():
    raise FileNotFoundError(f"Faceplate PNG not found:\n{image_path}")

if not header_path.is_file():
    raise FileNotFoundError(f"Faceplate header not found:\n{header_path}")

# Verify this is a PNG
image_bytes = image_path.read_bytes()
if not image_bytes.startswith(b"\x89PNG\r\n\x1a\n"):
    raise ValueError("The selected image is not a valid PNG file.")

# Preserve the existing header as a backup
backup_path = header_path.with_suffix(".h.backup")
if not backup_path.exists():
    backup_path.write_bytes(header_path.read_bytes())

# Encode the PNG as Base64 and wrap it into C++ string literals
encoded = base64.b64encode(image_bytes).decode("ascii")
lines = textwrap.wrap(encoded, 100)

header = [
    "#pragma once",
    "// Embedded Mark IIC+ faceplate artwork. Generated from the local PNG.",
    "namespace IICPlusFaceplateData",
    "{",
    "    inline constexpr const char* base64 =",
]

for i, line in enumerate(lines):
    suffix = ";" if i == len(lines) - 1 else ""
    header.append(f'        "{line}"{suffix}')

header.extend(["}", ""])

header_path.write_text("\n".join(header), encoding="ascii")

print("Faceplate header regenerated successfully.")
print(f"Source image: {image_path}")
print(f"Updated header: {header_path}")
print(f"Image size: {len(image_bytes):,} bytes")
print(f"Base64 length: {len(encoded):,} characters")
print(f"Backup: {backup_path}")
