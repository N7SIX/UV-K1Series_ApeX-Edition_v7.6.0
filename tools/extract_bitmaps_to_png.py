import re
import os
from PIL import Image

# Path to the C file
C_FILE = "App/bitmaps.c"
# Output directory for PNGs
OUT_DIR = "./icons_from_c/"
os.makedirs(OUT_DIR, exist_ok=True)

# Regex to match C arrays for bitmaps (uint8_t BITMAP_* or gFont*)
ARRAY_RE = re.compile(r"const uint8_t (BITMAP_[A-Za-z0-9_]+|gFont[A-Za-z0-9_]+)\s*\[([^\]]*)\]\s*=\s*\{([^}]*)\};", re.MULTILINE | re.DOTALL)

# Helper to parse C array data

def parse_c_array(data):
    # Remove comments and whitespace
    data = re.sub(r"//.*", "", data)
    data = re.sub(r"/\*.*?\*/", "", data, flags=re.DOTALL)
    data = data.replace("\n", " ").replace("\r", " ")
    # Split by comma, hex, or binary
    items = re.findall(r"0x[0-9a-fA-F]+|0b[01]+|\d+", data)
    values = []
    for item in items:
        if item.startswith("0x"):
            values.append(int(item, 16))
        elif item.startswith("0b"):
            values.append(int(item, 2))
        else:
            values.append(int(item))
    return values

# Helper to guess width/height from array size

def guess_size(name, dims, values):
    # Try to infer from dimensions in C
    if dims:
        dims = dims.replace(' ', '').split(',')
        dims = [int(d) for d in dims if d]
        if len(dims) == 2:
            return dims[1], dims[0]  # width, height
        elif len(dims) == 1:
            return len(values), dims[0]
    # Fallback: width = len(values), height = 8 or 16
    if len(values) % 8 == 0:
        return 8, len(values) // 8
    elif len(values) % 16 == 0:
        return 16, len(values) // 16
    else:
        return len(values), 1

# Main extraction
with open(C_FILE, "r") as f:
    c_code = f.read()

for match in ARRAY_RE.finditer(c_code):
    name, dims, data = match.groups()
    values = parse_c_array(data)
    width, height = guess_size(name, dims, values)
    # For 1D arrays, treat each value as a column of 8 bits
    if width * height != len(values):
        width = len(values)
        height = 8
    # Create image (1-bit)
    img = Image.new("1", (width, height))
    for x in range(width):
        v = values[x] if x < len(values) else 0
        for y in range(height):
            bit = (v >> y) & 1
            img.putpixel((x, y), 255 * bit)
    img = img.transpose(Image.ROTATE_270).transpose(Image.FLIP_LEFT_RIGHT)
    img.save(os.path.join(OUT_DIR, f"{name}.png"))
    print(f"Saved {name}.png ({width}x{height})")

print("Done. All icons extracted to", OUT_DIR)
