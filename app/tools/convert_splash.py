#!/usr/bin/env python3
"""
KittenHATS splash converter.
Converts assets/splash.png to a KittenHATS splash binary with header.

Output format:
  [KH_SPLASH_HEADER] [B8G8R8A8 pixel data]

The header contains magic, version, format, dimensions, stride and data_size.
Pixel format is B8G8R8A8 (byte order: B, G, R, A) matching the Tegra X1
display controller's WIN_COLOR_DEPTH_B8G8R8A8 (0xC).

Usage:
  python convert_splash.py <input.png> <output.bin>

The .bin is a complete splash file with header: 20 + 720*1280*4 = 3,686,420 bytes.
"""

import struct
import sys
import os

try:
    from PIL import Image
except ImportError:
    print("ERROR: PIL (Pillow) is required. Install with: pip install Pillow")
    sys.exit(1)

# KittenHATS splash constants (must match splash.h)
KH_SPLASH_MAGIC          = 0x5048534B  # "KHSP" in little-endian
KH_SPLASH_VERSION        = 1
KH_SPLASH_FORMAT_B8G8R8A8 = 1

# Expected dimensions
EXPECTED_WIDTH  = 720
EXPECTED_HEIGHT = 1280
EXPECTED_STRIDE = EXPECTED_WIDTH * 4
EXPECTED_DATA_SIZE = EXPECTED_WIDTH * EXPECTED_HEIGHT * 4
HEADER_SIZE = 20  # sizeof(kh_splash_header_t)


def rgba_to_b8g8r8a8(r, g, b, a=255):
    """
    Convert RGBA byte values to B8G8R8A8 u32 (Tegra X1 native format).
    
    In little-endian memory, this produces:
      byte 0 = B (blue)
      byte 1 = G (green)
      byte 2 = R (red)
      byte 3 = A (alpha)
    
    This matches WIN_COLOR_DEPTH_B8G8R8A8 (0xC) on the Tegra X1 display controller.
    """
    return (b << 0) | (g << 8) | (r << 16) | (a << 24)


def convert_splash(input_path, output_bin):
    if not os.path.exists(input_path):
        print(f"ERROR: Input file not found: {input_path}")
        sys.exit(1)

    img = Image.open(input_path).convert("RGBA")

    width, height = img.size

    # Validate dimensions
    if width != EXPECTED_WIDTH or height != EXPECTED_HEIGHT:
        print(f"ERROR: Expected {EXPECTED_WIDTH}x{EXPECTED_HEIGHT} image, got {width}x{height}")
        sys.exit(1)

    pixels = list(img.getdata())

    # Build the splash binary: header + pixel data
    with open(output_bin, "wb") as f:
        # Write header (20 bytes)
        f.write(struct.pack("<I", KH_SPLASH_MAGIC))       # magic
        f.write(struct.pack("<H", KH_SPLASH_VERSION))     # version
        f.write(struct.pack("<H", KH_SPLASH_FORMAT_B8G8R8A8))  # format
        f.write(struct.pack("<H", width))                 # width
        f.write(struct.pack("<H", height))                # height
        f.write(struct.pack("<I", EXPECTED_STRIDE))       # stride
        f.write(struct.pack("<I", EXPECTED_DATA_SIZE))    # data_size

        # Write pixel data in B8G8R8A8 format
        for r, g, b, a in pixels:
            val = rgba_to_b8g8r8a8(r, g, b, a)
            f.write(struct.pack("<I", val))

    bin_size = os.path.getsize(output_bin)
    expected_total = HEADER_SIZE + EXPECTED_DATA_SIZE

    print(f"Splash source: {input_path}")
    print(f"Splash resolution: {width} x {height}")
    print(f"Splash format: B8G8R8A8 (format_id={KH_SPLASH_FORMAT_B8G8R8A8})")
    print(f"Splash stride: {EXPECTED_STRIDE} bytes")
    print(f"Header: {HEADER_SIZE} bytes")
    print(f"Pixel data: {EXPECTED_DATA_SIZE} bytes ({EXPECTED_DATA_SIZE / 1024:.1f} KB)")
    print(f"Total splash.bin: {bin_size} bytes ({bin_size / 1024:.1f} KB)")

    if bin_size != expected_total:
        print(f"WARNING: Expected {expected_total} bytes, got {bin_size} bytes")
        sys.exit(1)

    print(f"OK: {output_bin}")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input.png> <output.bin>")
        sys.exit(1)

    convert_splash(sys.argv[1], sys.argv[2])
