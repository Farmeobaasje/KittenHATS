#!/usr/bin/env python3
"""Verify BMP assets for KittenHATS Phase 3 release."""
import struct, os, sys

files = [
    ('app/res/home_background.bmp', 1280, 720),
    ('app/res/kittenhats_logo.bmp', 500, 180),
    ('app/res/gear_icon_normal.bmp', 128, 128),
    ('app/res/gear_icon_pressed.bmp', 128, 128),
    ('app/res/start_button_normal.bmp', 500, 260),
    ('app/res/start_button_pressed.bmp', 500, 260),
]

all_valid = True
for path, exp_w, exp_h in files:
    if not os.path.exists(path):
        print(f"MISSING: {path}")
        all_valid = False
        continue
    
    size = os.path.getsize(path)
    with open(path, 'rb') as f:
        h = f.read(138)
        bf_type = struct.unpack('<H', h[0:2])[0]
        bf_size = struct.unpack('<I', h[2:6])[0]
        bi_width = struct.unpack('<i', h[18:22])[0]
        bi_height = struct.unpack('<i', h[22:26])[0]
        bi_bit_count = struct.unpack('<H', h[28:30])[0]
        bi_compression = struct.unpack('<I', h[30:34])[0]
        
        valid = (bf_type == 0x4D42)  # 'BM'
        valid = valid and (bi_bit_count == 32)
        valid = valid and (bi_width == exp_w)
        valid = valid and (bi_height == exp_h)
        valid = valid and (bi_compression == 3)  # BI_BITFIELDS
        valid = valid and (size == bf_size)
        
        f.seek(138)
        pixel_data = f.read()
        has_content = any(b != 0 for b in pixel_data[:1000])
    
    status = "OK" if valid else "INVALID"
    content = "has data" if has_content else "EMPTY"
    print(f"{os.path.basename(path):40s} {size:>8d} bytes  {bi_width}x{bi_height}  {bi_bit_count}bpp  [{status}]  [{content}]")
    all_valid = all_valid and valid

print(f"\nAll assets valid: {'YES' if all_valid else 'NO'}")
sys.exit(0 if all_valid else 1)
