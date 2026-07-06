#!/usr/bin/env python3
"""
KittenHATS UI asset converter.
Converts PNG assets to 32-bit BGRA BMP files for LVGL on the Tegra X1.

Output format:
  Standard 32-bit BMP (BITMAPV4HEADER or BITMAPINFOHEADER) with:
  - BGRA byte order (B, G, R, A) matching Tegra X1's B8G8R8A8
  - Bottom-up row order (standard BMP)
  - No compression (BI_RGB / BI_BITFIELDS)

Usage:
  python convert_ui_assets.py <input.png> <output.bmp>

The .bmp files are loaded at runtime by bmp_to_lvimg_obj() (Nyx's BMP loader).
"""

import struct
import sys
import os

try:
    from PIL import Image
except ImportError:
    print("ERROR: PIL (Pillow) is required. Install with: pip install Pillow")
    sys.exit(1)


def rgba_to_bgra_le(r, g, b, a=255):
    """
    Convert RGBA byte values to BGRA u32 in little-endian.

    In BMP file memory (little-endian), this produces:
      byte 0 = B (blue)
      byte 1 = G (green)
      byte 2 = R (red)
      byte 3 = A (alpha)

    This matches the Tegra X1's native B8G8R8A8 pixel format
    (WIN_COLOR_DEPTH_B8G8R8A8 = 0xC).
    """
    return (b << 0) | (g << 8) | (r << 16) | (a << 24)


def convert_asset(input_path, output_bmp):
    if not os.path.exists(input_path):
        print(f"ERROR: Input file not found: {input_path}")
        sys.exit(1)

    img = Image.open(input_path).convert("RGBA")
    width, height = img.size
    pixels = list(img.getdata())

    # BMP row stride: each row must be aligned to 4 bytes
    # 32bpp = 4 bytes per pixel, so stride = width * 4 (already aligned)
    row_stride = width * 4
    pixel_data_size = row_stride * height

    # DIB header size: BITMAPV4HEADER (108 bytes) + intent/profile (16 bytes) = 124 bytes
    dib_header_size = 124

    # BMP file header (14 bytes)
    bf_type = 0x4D42  # "BM"
    bf_size = 14 + dib_header_size + pixel_data_size  # header + DIB + pixels
    bf_reserved1 = 0
    bf_reserved2 = 0
    bf_off_bits = 14 + dib_header_size  # offset to pixel data

    # DIB header fields
    bi_size = dib_header_size
    bi_width = width
    bi_height = height
    bi_planes = 1
    bi_bit_count = 32
    bi_compression = 3  # BI_BITFIELDS (for alpha channel support)
    bi_size_image = pixel_data_size
    bi_x_pels_per_meter = 2835  # 72 DPI
    bi_y_pels_per_meter = 2835
    bi_clr_used = 0
    bi_clr_important = 0

    # Color masks for BI_BITFIELDS (BGRA order)
    red_mask   = 0x00FF0000
    green_mask = 0x0000FF00
    blue_mask  = 0x000000FF
    alpha_mask = 0xFF000000

    # Color space (LCS_WINDOWS_COLOR_SPACE = 0x57696E20)
    bv_cstype = 0x57696E20

    # Gamma (unused, set to 0)
    bv_red_gamma   = 0
    bv_green_gamma = 0
    bv_blue_gamma  = 0

    # Rendering intent (LCS_GM_IMAGES = 4)
    bv_intent = 4

    # Profile data (unused)
    bv_profile_data = 0
    bv_profile_size = 0
    bv_reserved = 0

    with open(output_bmp, "wb") as f:
        # --- BMP file header (14 bytes) ---
        f.write(struct.pack("<H", bf_type))       # bfType
        f.write(struct.pack("<I", bf_size))       # bfSize
        f.write(struct.pack("<H", bf_reserved1))  # bfReserved1
        f.write(struct.pack("<H", bf_reserved2))  # bfReserved2
        f.write(struct.pack("<I", bf_off_bits))   # bfOffBits

        # --- DIB header (124 bytes) ---
        f.write(struct.pack("<I", bi_size))                # biSize
        f.write(struct.pack("<i", bi_width))               # biWidth
        f.write(struct.pack("<i", bi_height))              # biHeight
        f.write(struct.pack("<H", bi_planes))              # biPlanes
        f.write(struct.pack("<H", bi_bit_count))           # biBitCount
        f.write(struct.pack("<I", bi_compression))         # biCompression
        f.write(struct.pack("<I", bi_size_image))          # biSizeImage
        f.write(struct.pack("<i", bi_x_pels_per_meter))    # biXPelsPerMeter
        f.write(struct.pack("<i", bi_y_pels_per_meter))    # biYPelsPerMeter
        f.write(struct.pack("<I", bi_clr_used))            # biClrUsed
        f.write(struct.pack("<I", bi_clr_important))       # biClrImportant

        # Color masks (BGRA order)
        f.write(struct.pack("<I", red_mask))               # bV4RedMask
        f.write(struct.pack("<I", green_mask))             # bV4GreenMask
        f.write(struct.pack("<I", blue_mask))              # bV4BlueMask
        f.write(struct.pack("<I", alpha_mask))             # bV4AlphaMask

        # Color space type
        f.write(struct.pack("<I", bv_cstype))              # bV4CSType

        # CIEXYZ endpoints (unused, 9 * 4 = 36 bytes)
        for _ in range(9):
            f.write(struct.pack("<I", 0))

        # Gamma
        f.write(struct.pack("<I", bv_red_gamma))           # bV4RedGamma
        f.write(struct.pack("<I", bv_green_gamma))         # bV4GreenGamma
        f.write(struct.pack("<I", bv_blue_gamma))          # bV4BlueGamma

        # Rendering intent
        f.write(struct.pack("<I", bv_intent))              # bV4Intent

        # Profile data (unused)
        f.write(struct.pack("<I", bv_profile_data))        # bV4ProfileData
        f.write(struct.pack("<I", bv_profile_size))        # bV4ProfileSize
        f.write(struct.pack("<I", bv_reserved))            # bV4Reserved

        # --- Pixel data (bottom-up, BGRA) ---
        # BMP stores rows bottom-to-top, so we iterate from last row to first
        for y in range(height - 1, -1, -1):
            for x in range(width):
                idx = y * width + x
                r, g, b, a = pixels[idx]
                val = rgba_to_bgra_le(r, g, b, a)
                f.write(struct.pack("<I", val))

    bin_size = os.path.getsize(output_bmp)
    expected_total = bf_size

    print(f"Asset source: {input_path}")
    print(f"Asset resolution: {width} x {height}")
    print(f"Asset format: 32-bit BGRA BMP (BITMAPV4HEADER)")
    print(f"Row stride: {row_stride} bytes")
    print(f"Pixel data: {pixel_data_size} bytes ({pixel_data_size / 1024:.1f} KB)")
    print(f"Total BMP: {bin_size} bytes ({bin_size / 1024:.1f} KB)")

    if bin_size != expected_total:
        print(f"WARNING: Expected {expected_total} bytes, got {bin_size} bytes")
        sys.exit(1)

    print(f"OK: {output_bmp}")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input.png> <output.bmp>")
        sys.exit(1)

    convert_asset(sys.argv[1], sys.argv[2])
