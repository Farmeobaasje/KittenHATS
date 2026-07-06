#!/usr/bin/env python3
"""
KittenHATS UI generator — MULTI-THEME
==============================================================================
Genereert complete UI-sets in meerdere thema's. Elke set bevat:

  HOME       start_button (SPELEN) normal/pressed  +  gear_icon normal/pressed
  SETTINGS   titel, TAAL (disabled), PIN, TERUG
  TAAL       titel, taalkeuze-radioknoppen, terug

Alles in dezelfde vormtaal als de originele SPELEN-knop:
gradient body, dikke afgeronde rand met buitengloed, glans bovenaan,
witte tekst met schaduw, icoon links.

Per THEMA verschillen alleen de KLEUREN (en optioneel radius/rand). De vorm,
formaten en bestandsnamen blijven identiek, zodat je een heel thema in een keer
kunt wisselen.

Gebruik:
    python3 generate_themes.py                       # alle thema's, alle talen
    python3 generate_themes.py --theme aqua sunset    # alleen deze thema's
    python3 generate_themes.py --lang nl en           # alleen deze talen
    python3 generate_themes.py --home-only            # alleen SPELEN + tandwiel
    python3 generate_themes.py --no-preview           # geen preview-PNG's

Output: out_themes/<thema>/<taal>/*.bmp  (+ home-assets in <thema>/common/)
Alle BMP's: 32-bit BGRA met alfakanaal (BITMAPV4HEADER), klaar voor LVGL.
==============================================================================
"""

import argparse
import os
import struct
import random
from PIL import Image, ImageDraw, ImageFont, ImageFilter

import platform
if platform.system() == "Windows":
    FONT_PATH = "arial.ttf"
else:
    FONT_PATH = "/usr/share/fonts/truetype/google-fonts/Poppins-Bold.ttf"
SS = 4  # supersampling

# ---------------------------------------------------------------------------
# THEMA'S -- alleen kleuren verschillen. Voeg gerust eigen thema's toe.
# ---------------------------------------------------------------------------
# Elk thema definieert dezelfde sleutels. 'accent' = de originele goud-look e.d.
THEMES = {
    # 1) De originele look (blauw + goud) — als referentie/basis
    "royal": {
        "label": "Royal (blauw + goud)",
        "body_top": (122, 138, 226), "body_bottom": (74, 92, 190),
        "selected_top": (104, 200, 140), "selected_bottom": (58, 150, 96),
        "border_outer": (208, 150, 70), "border_inner": (255, 206, 130),
        "glow": (255, 190, 110),
        "bg": (14, 16, 34),
    },
    # 2) Aqua: teal/cyaan body met een frisse mint rand, koele look
    "aqua": {
        "label": "Aqua (teal + mint)",
        "body_top": (64, 196, 206), "body_bottom": (28, 132, 158),
        "selected_top": (255, 214, 120), "selected_bottom": (232, 168, 60),
        "border_outer": (240, 250, 252), "border_inner": (198, 244, 248),
        "glow": (150, 240, 246),
        "bg": (8, 30, 40),
    },
    # 3) Sunset: warm oranje/roze body met diep-paarse rand
    "sunset": {
        "label": "Sunset (oranje + magenta)",
        "body_top": (255, 158, 92), "body_bottom": (232, 86, 120),
        "selected_top": (130, 220, 150), "selected_bottom": (70, 168, 104),
        "border_outer": (86, 52, 120), "border_inner": (190, 150, 235),
        "glow": (255, 150, 120),
        "bg": (32, 14, 40),
    },
    # 4) Candy: speels roze/paars met knalgele rand — kittens-vibe
    "candy": {
        "label": "Candy (roze + geel)",
        "body_top": (214, 130, 224), "body_bottom": (150, 78, 200),
        "selected_top": (120, 224, 176), "selected_bottom": (60, 172, 128),
        "border_outer": (255, 224, 92), "border_inner": (255, 244, 176),
        "glow": (255, 200, 240),
        "bg": (26, 12, 36),
    },
}

# Kleuren die in ELK thema hetzelfde zijn
COMMON = {
    "disabled_top": (150, 152, 168), "disabled_bottom": (120, 122, 140),
    "border_disabled": (170, 172, 185),
    "text": (255, 255, 255), "text_shadow": (24, 26, 54, 200),
    "text_disabled": (238, 240, 248),
    "radius_frac": 0.46, "border_px": 7, "glow_px": 10, "pressed_shift": True,
}

# ---------------------------------------------------------------------------
# Formaten
# ---------------------------------------------------------------------------
BTN = (420, 160)
TITLE = (500, 120)
START = (500, 260)   # SPELEN-knop
GEAR = (128, 128)    # tandwiel
PIN_TITLE = (400, 60)   # PIN-scherm titel
PIN_BACK  = (160, 96)   # PIN-scherm terugknop (BMP 160x96, LVGL toont 160x60)

# ---------------------------------------------------------------------------
# Teksten per taal (home + settings + taal)
# ---------------------------------------------------------------------------
# soort: button | radio | title | static | start
TRANSLATIONS = {
    "nl": {
        "start_button":   ("SPELEN",      "play",  START, "start"),
        "settings_title": ("INSTELLINGEN", None,   TITLE, "title"),
        "taal_button":    ("TAAL",         None,   BTN, "button"),
        "pin_button":     ("PIN",          "lock", BTN, "button"),
        "settings_back":  ("TERUG",        "arrow", BTN, "button"),
        "language_title": ("Taal / Language", None, TITLE, "title"),
        "nl_button":      ("Nederlands",   "flag", BTN, "radio"),
        "en_button":      ("English",      "flag", BTN, "radio"),
        "de_button":      ("Deutsch",      "flag", BTN, "radio"),
        "es_button":      ("Español",      "flag", BTN, "radio"),
        "fr_button":      ("Français",     "flag", BTN, "radio"),
        "lang_back":      ("TERUG",        "arrow", BTN, "button"),
        "pin_title":      ("PIN-CODE",     None,   PIN_TITLE, "title"),
        "back_button":    ("TERUG",        "arrow", PIN_BACK, "button"),
    },
    "en": {
        "start_button":   ("PLAY",         "play",  START, "start"),
        "settings_title": ("SETTINGS",     None,   TITLE, "title"),
        "taal_button":    ("LANGUAGE",     None,   BTN, "button"),
        "pin_button":     ("PIN",          "lock", BTN, "button"),
        "settings_back":  ("BACK",         "arrow", BTN, "button"),
        "language_title": ("Language",     None,   TITLE, "title"),
        "nl_button":      ("Nederlands",   "flag", BTN, "radio"),
        "en_button":      ("English",      "flag", BTN, "radio"),
        "de_button":      ("Deutsch",      "flag", BTN, "radio"),
        "es_button":      ("Español",      "flag", BTN, "radio"),
        "fr_button":      ("Français",     "flag", BTN, "radio"),
        "lang_back":      ("BACK",         "arrow", BTN, "button"),
        "pin_title":      ("PIN CODE",     None,   PIN_TITLE, "title"),
        "back_button":    ("BACK",         "arrow", PIN_BACK, "button"),
    },
    "de": {
        "start_button":   ("SPIELEN",      "play",  START, "start"),
        "settings_title": ("EINSTELLUNGEN", None,  TITLE, "title"),
        "taal_button":    ("SPRACHE",      None,   BTN, "button"),
        "pin_button":     ("PIN",          "lock", BTN, "button"),
        "settings_back":  ("ZURÜCK",       "arrow", BTN, "button"),
        "language_title": ("Sprache / Language", None, TITLE, "title"),
        "nl_button":      ("Nederlands",   "flag", BTN, "radio"),
        "en_button":      ("English",      "flag", BTN, "radio"),
        "de_button":      ("Deutsch",      "flag", BTN, "radio"),
        "es_button":      ("Español",      "flag", BTN, "radio"),
        "fr_button":      ("Français",     "flag", BTN, "radio"),
        "lang_back":      ("ZURÜCK",       "arrow", BTN, "button"),
        "pin_title":      ("PIN-CODE",     None,   PIN_TITLE, "title"),
        "back_button":    ("ZURÜCK",       "arrow", PIN_BACK, "button"),
    },
    "fr": {
        "start_button":   ("JOUER",        "play",  START, "start"),
        "settings_title": ("PARAMÈTRES",   None,   TITLE, "title"),
        "taal_button":    ("LANGUE",       None,   BTN, "button"),
        "pin_button":     ("PIN",          "lock", BTN, "button"),
        "settings_back":  ("RETOUR",       "arrow", BTN, "button"),
        "language_title": ("Langue / Language", None, TITLE, "title"),
        "nl_button":      ("Nederlands",   "flag", BTN, "radio"),
        "en_button":      ("English",      "flag", BTN, "radio"),
        "de_button":      ("Deutsch",      "flag", BTN, "radio"),
        "es_button":      ("Español",      "flag", BTN, "radio"),
        "fr_button":      ("Français",     "flag", BTN, "radio"),
        "lang_back":      ("RETOUR",       "arrow", BTN, "button"),
        "pin_title":      ("CODE PIN",     None,   PIN_TITLE, "title"),
        "back_button":    ("RETOUR",       "arrow", PIN_BACK, "button"),
    },
    "es": {
        "start_button":   ("JUGAR",        "play",  START, "start"),
        "settings_title": ("AJUSTES",      None,   TITLE, "title"),
        "taal_button":    ("IDIOMA",       None,   BTN, "button"),
        "pin_button":     ("PIN",          "lock", BTN, "button"),
        "settings_back":  ("VOLVER",       "arrow", BTN, "button"),
        "language_title": ("Idioma / Language", None, TITLE, "title"),
        "nl_button":      ("Nederlands",   "flag", BTN, "radio"),
        "en_button":      ("English",      "flag", BTN, "radio"),
        "de_button":      ("Deutsch",      "flag", BTN, "radio"),
        "es_button":      ("Español",      "flag", BTN, "radio"),
        "fr_button":      ("Français",     "flag", BTN, "radio"),
        "lang_back":      ("VOLVER",       "arrow", BTN, "button"),
        "pin_title":      ("CÓDIGO PIN",   None,   PIN_TITLE, "title"),
        "back_button":    ("VOLVER",       "arrow", PIN_BACK, "button"),
    },
}

SETTINGS_KEYS = {"settings_title", "taal_button", "pin_button", "settings_back"}

# ===========================================================================
# Teken-helpers
# ===========================================================================

def _vgrad(size, top, bottom):
    w, h = size
    grad = Image.new("RGB", (1, h))
    for y in range(h):
        t = y / max(1, h - 1)
        grad.putpixel((0, y), tuple(
            int(round(top[i] + (bottom[i] - top[i]) * t)) for i in range(3)))
    return grad.resize((w, h))


def _rounded_mask(size, radius):
    m = Image.new("L", size, 0)
    ImageDraw.Draw(m).rounded_rectangle([0, 0, size[0]-1, size[1]-1],
                                        radius=radius, fill=255)
    return m


def _fit_font(text, max_w, max_h, start):
    size = start
    while size > 10:
        f = ImageFont.truetype(FONT_PATH, size)
        box = f.getbbox(text)
        if (box[2]-box[0]) <= max_w and (box[3]-box[1]) <= max_h:
            return f
        size -= 2
    return ImageFont.truetype(FONT_PATH, 10)


def _draw_icon(draw, kind, cx, cy, s, color, body_col):
    if kind == "lock":
        bw, bh = s*0.9, s*0.7
        draw.rounded_rectangle([cx-bw/2, cy-bh/2+s*0.12, cx+bw/2, cy+bh/2+s*0.12],
                               radius=s*0.12, fill=color)
        draw.arc([cx-bw*0.34, cy-bh*0.75, cx+bw*0.34, cy+bh*0.1],
                 start=180, end=360, fill=color, width=max(3, int(s*0.14)))
        draw.ellipse([cx-s*0.09, cy-s*0.02, cx+s*0.09, cy+s*0.16], fill=body_col)
    elif kind == "arrow":
        w = s*0.5
        draw.polygon([(cx+w*0.2, cy-s*0.5), (cx-w*0.8, cy), (cx+w*0.2, cy+s*0.5)],
                     fill=color)
        draw.rectangle([cx+w*0.0, cy-s*0.16, cx+w*0.9, cy+s*0.16], fill=color)
    elif kind == "flag":
        r = s*0.5
        draw.ellipse([cx-r, cy-r, cx+r, cy+r], fill=color)
        draw.ellipse([cx-r*0.45, cy-r*0.45, cx+r*0.45, cy+r*0.45], fill=body_col)
    elif kind == "play":
        # afgeronde play-driehoek
        w = s*0.7
        pts = [(cx-w*0.5, cy-s*0.62), (cx+w*0.72, cy), (cx-w*0.5, cy+s*0.62)]
        draw.polygon(pts, fill=color)


def _draw_gear(draw, cx, cy, r, color, teeth=8, tooth=0.22):
    """Teken een chunky tandwiel (ronde tanden) op (cx,cy) met straal r."""
    import math
    inner = r * (1 - tooth)          # basis van de tanden
    ring_r = inner * 0.98
    tooth_r = (2 * math.pi * inner) / (teeth * 4.2)  # halve breedte van 1 tand
    for i in range(teeth):
        ang = (i / teeth) * 2 * math.pi
        base_x = cx + inner * math.cos(ang)
        base_y = cy + inner * math.sin(ang)
        top_x = cx + r * math.cos(ang)
        top_y = cy + r * math.sin(ang)
        # capsule-vormige tand: cirkel op top + basis + dikke lijn ertussen
        draw.ellipse([top_x-tooth_r, top_y-tooth_r, top_x+tooth_r, top_y+tooth_r],
                     fill=color)
        draw.line([(base_x, base_y), (top_x, top_y)], fill=color,
                  width=int(tooth_r*2))
    # centrale schijf
    draw.ellipse([cx-ring_r, cy-ring_r, cx+ring_r, cy+ring_r], fill=color)
    return inner


# ===========================================================================
# Kern: button renderen met een gegeven palet
# ===========================================================================

def _palette(theme):
    p = dict(COMMON)
    p.update(theme)
    return p


def render_button(text, icon, size, state, theme):
    p = _palette(theme)
    w, h = size
    W, H = w*SS, h*SS
    glow_px = p["glow_px"]*SS
    pad = glow_px + 2*SS

    canvas = Image.new("RGBA", (W+2*pad, H+2*pad), (0, 0, 0, 0))
    radius = int(H * p["radius_frac"])

    if state == "disabled":
        top, bot = p["disabled_top"], p["disabled_bottom"]
        border_o = border_i = p["border_disabled"]
        txt_col = p["text_disabled"]; do_glow = False
    elif state == "selected":
        top, bot = p["selected_top"], p["selected_bottom"]
        border_o, border_i = p["border_outer"], p["border_inner"]
        txt_col = p["text"]; do_glow = True
    else:
        top, bot = p["body_top"], p["body_bottom"]
        border_o, border_i = p["border_outer"], p["border_inner"]
        txt_col = p["text"]; do_glow = True

    if state == "pressed" and p["pressed_shift"]:
        top = tuple(int(c*0.86) for c in top)
        bot = tuple(int(c*0.86) for c in bot)

    body_box = (pad, pad, pad+W, pad+H)
    body_col = tuple(bot)  # voor icoon-uitsparingen

    if do_glow:
        glow = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
        ImageDraw.Draw(glow).rounded_rectangle(body_box, radius=radius,
                                               fill=p["glow"]+(180,))
        glow = glow.filter(ImageFilter.GaussianBlur(glow_px*0.6))
        canvas = Image.alpha_composite(canvas, glow)

    bpx = p["border_px"]*SS
    bl = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    ImageDraw.Draw(bl).rounded_rectangle(body_box, radius=radius,
                                         fill=border_o+(255,))
    canvas = Image.alpha_composite(canvas, bl)

    inner_box = (body_box[0]+bpx, body_box[1]+bpx, body_box[2]-bpx, body_box[3]-bpx)
    iw, ih = inner_box[2]-inner_box[0], inner_box[3]-inner_box[1]
    inner_radius = max(1, radius-bpx)
    grad = _vgrad((iw, ih), top, bot).convert("RGBA")
    gmask = _rounded_mask((iw, ih), inner_radius)
    body = Image.new("RGBA", (iw, ih), (0, 0, 0, 0))
    body.paste(grad, (0, 0), gmask)

    hl = Image.new("RGBA", (iw, ih), (0, 0, 0, 0))
    ImageDraw.Draw(hl).rounded_rectangle([0, 0, iw, int(ih*0.5)],
                                         radius=inner_radius, fill=(255, 255, 255, 55))
    hl = hl.filter(ImageFilter.GaussianBlur(6*SS))
    hl.putalpha(hl.getchannel("A").point(lambda a: min(a, 70)))
    body = Image.alpha_composite(body, Image.composite(
        hl, Image.new("RGBA", (iw, ih), (0, 0, 0, 0)), gmask))

    ImageDraw.Draw(body).rounded_rectangle([1, 1, iw-2, ih-2], radius=inner_radius,
                                           outline=border_i+(150,), width=max(1, SS))
    canvas.paste(body, (inner_box[0], inner_box[1]), body)

    draw = ImageDraw.Draw(canvas)
    has_icon = icon is not None
    icon_zone = ih*0.9 if has_icon else 0
    txt_max_w = iw - (icon_zone + 40*SS if has_icon else 60*SS)
    txt_max_h = int(ih*0.5)
    font = _fit_font(text, txt_max_w, txt_max_h, start=int(ih*0.5))
    tb = font.getbbox(text); tw, th = tb[2]-tb[0], tb[3]-tb[1]

    gap = 26*SS if has_icon else 0
    icon_s = ih*0.42
    total_w = (icon_s*1.4 + gap if has_icon else 0) + tw
    start_x = inner_box[0] + (iw-total_w)/2
    cy = inner_box[1] + ih/2
    y_off = (3*SS if state == "pressed" and p["pressed_shift"] else 0)

    if has_icon:
        icx = start_x + icon_s*0.7
        _draw_icon(draw, icon, icx, cy+y_off, icon_s, txt_col, body_col)
        tx = start_x + icon_s*1.4 + gap - tb[0]
    else:
        tx = start_x - tb[0]
    ty = cy - th/2 - tb[1] + y_off

    draw.text((tx+2*SS, ty+2*SS), text, font=font, fill=p["text_shadow"])
    draw.text((tx, ty), text, font=font, fill=txt_col+(255,))

    return canvas.resize((canvas.size[0]//SS, canvas.size[1]//SS), Image.LANCZOS)


def render_title(text, size, theme):
    p = _palette(theme)
    w, h = size
    W, H = w*SS, h*SS
    canvas = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    font = _fit_font(text, int(W*0.92), int(H*0.55), start=int(H*0.6))
    tb = font.getbbox(text); tw, th = tb[2]-tb[0], tb[3]-tb[1]
    tx = (W-tw)/2 - tb[0]; ty = (H-th)/2 - tb[1]

    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gcol = tuple(p["border_inner"])
    ImageDraw.Draw(glow).text((tx, ty), text, font=font, fill=gcol+(255,))
    glow = glow.filter(ImageFilter.GaussianBlur(6*SS))
    canvas = Image.alpha_composite(canvas, glow)

    draw = ImageDraw.Draw(canvas)
    draw.text((tx+2*SS, ty+2*SS), text, font=font, fill=p["text_shadow"])
    draw.text((tx, ty), text, font=font, fill=(255, 255, 255, 255))
    return canvas.resize((w, h), Image.LANCZOS)


def render_gear(size, state, theme):
    """Tandwiel-icoon (rond) in themakleuren, met gouden rand-vibe."""
    p = _palette(theme)
    w, h = size
    W, H = w*SS, h*SS
    glow_px = p["glow_px"]*SS
    pad = glow_px + 2*SS
    canvas = Image.new("RGBA", (W+2*pad, H+2*pad), (0, 0, 0, 0))
    cx, cy = canvas.size[0]/2, canvas.size[1]/2
    r = min(W, H)/2

    top, bot = p["body_top"], p["body_bottom"]
    if state == "pressed" and p["pressed_shift"]:
        top = tuple(int(c*0.86) for c in top)
        bot = tuple(int(c*0.86) for c in bot)
    border_o, border_i = p["border_outer"], p["border_inner"]

    # gloed
    glow = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    _draw_gear(ImageDraw.Draw(glow), cx, cy, r, p["glow"]+(180,))
    glow = glow.filter(ImageFilter.GaussianBlur(glow_px*0.6))
    canvas = Image.alpha_composite(canvas, glow)

    # rand-tandwiel (iets groter), dan body-tandwiel erin
    bl = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    _draw_gear(ImageDraw.Draw(bl), cx, cy, r, border_o+(255,))
    canvas = Image.alpha_composite(canvas, bl)

    bpx = p["border_px"]*SS
    body = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    inner_r = _draw_gear(ImageDraw.Draw(body), cx, cy, r-bpx, bot+(255,))
    # verticale gradient over de body via mask
    grad = _vgrad(canvas.size, top, bot).convert("RGBA")
    bmask = body.getchannel("A")
    canvas.paste(grad, (0, 0), bmask)

    # centraal gat: transparant maken via alfamasker
    hole_r = (r - bpx) * 0.26
    hole = Image.new("L", canvas.size, 0)
    ImageDraw.Draw(hole).ellipse([cx-hole_r, cy-hole_r, cx+hole_r, cy+hole_r],
                                 fill=255)
    # waar hole==255 -> alfa 0, elders alfa ongewijzigd
    new_alpha = Image.composite(Image.new("L", canvas.size, 0),
                                canvas.getchannel("A"), hole)
    canvas.putalpha(new_alpha)

    # dunne goud ring rond het gat
    draw = ImageDraw.Draw(canvas)
    draw.ellipse([cx-hole_r-SS*2, cy-hole_r-SS*2, cx+hole_r+SS*2, cy+hole_r+SS*2],
                 outline=border_o+(255,), width=max(2, SS*2))

    # binnen-highlight bovenkant (glans)
    draw.arc([cx-r+bpx, cy-r+bpx, cx+r-bpx, cy+r-bpx], start=200, end=340,
             fill=border_i+(160,), width=max(2, SS*2))

    return canvas.resize((canvas.size[0]//SS, canvas.size[1]//SS), Image.LANCZOS)


# ===========================================================================
# BMP export (32-bit BGRA, BITMAPV4HEADER = 108 bytes)
# ===========================================================================

def save_bgra_bmp(img, path):
    img = img.convert("RGBA")
    w, h = img.size
    px = img.load()
    rows = bytearray()
    for y in range(h-1, -1, -1):
        for x in range(w):
            r, g, b, a = px[x, y]
            rows += bytes((b, g, r, a))
    pixel_data = bytes(rows)
    header_size = 108
    offset = 14 + header_size
    file_size = offset + len(pixel_data)
    fh = b'BM' + struct.pack('<IHHI', file_size, 0, 0, offset)
    v4 = struct.pack('<IiiHHIIiiII', header_size, w, h, 1, 32, 3,
                     len(pixel_data), 2835, 2835, 0, 0)
    v4 += struct.pack('<IIII', 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
    v4 += struct.pack('<I', 0)
    v4 += b'\x00' * 36
    v4 += struct.pack('<III', 0, 0, 0)
    assert len(v4) == 108, len(v4)
    with open(path, 'wb') as f:
        f.write(fh + v4 + pixel_data)


# ===========================================================================
# Orkestratie
# ===========================================================================

def variants_for(kind):
    if kind == "button":  return ["normal", "pressed"]
    if kind == "radio":   return ["normal", "pressed", "selected"]
    if kind == "static":  return ["normal"]
    if kind == "start":   return ["normal", "pressed"]
    if kind == "title":   return [None]
    return ["normal"]


def build(themes, langs, home_only, make_preview, only_keys=None, root="out_themes"):
    os.makedirs(root, exist_ok=True)
    total = 0
    for tname in themes:
        theme = THEMES[tname]
        troot = os.path.join(root, tname)
        # ---- HOME assets (thema-breed, in common/) ----
        common_dir = os.path.join(troot, "common")
        os.makedirs(common_dir, exist_ok=True)
        for st in ["normal", "pressed"]:
            g = render_gear(GEAR, st, theme)
            save_bgra_bmp(g, os.path.join(common_dir, f"gear_icon_{st}.bmp"))
            total += 1

        for lang in langs:
            spec = TRANSLATIONS[lang]
            ldir = os.path.join(troot, lang)
            os.makedirs(ldir, exist_ok=True)
            n = 0
            for key, (text, icon, size, kind) in spec.items():
                if only_keys is not None and key not in only_keys:
                    continue
                if home_only and kind != "start":
                    continue
                if (not home_only) and kind == "start":
                    # start-knop is taal-specifiek -> hoort in de taalmap
                    pass
                for var in variants_for(kind):
                    if kind == "title":
                        img = render_title(text, size, theme); name = f"{key}.bmp"
                    elif kind == "static":
                        img = render_button(text, icon, size, "disabled", theme)
                        name = f"{key}_normal.bmp"
                    else:
                        img = render_button(text, icon, size, var, theme) \
                            if kind != "start" else \
                            render_button(text, icon, size, var, theme)
                        name = f"{key}_{var}.bmp"
                    save_bgra_bmp(img, os.path.join(ldir, name)); n += 1; total += 1
            print(f"  [{tname}/{lang}] {n} bestanden")
            if make_preview and not home_only:
                _preview(tname, theme, lang, spec, ldir)
        print(f"  -> thema '{tname}' klaar ({theme['label']})")
    print(f"\nKlaar. {total} BMP-bestanden in ./{root}/")


def _preview(tname, theme, lang, spec, ldir):
    """Nabootsing: home-knop + settings-knoppen naast elkaar op thema-bg."""
    p = _palette(theme)
    W, Hc = 1280, 720
    canvas = Image.new("RGBA", (W, Hc), tuple(p["bg"])+(255,))
    draw = ImageDraw.Draw(canvas)
    random.seed(3)
    for _ in range(120):
        x, y = random.randint(0, W-1), random.randint(0, Hc-1)
        draw.ellipse([x, y, x+2, y+2], fill=(220, 228, 255, 160))

    def paste(img, x, y): canvas.alpha_composite(img, (x, y))

    # links: home (gear rechtsboven + SPELEN gecentreerd in linkerhelft)
    gear = render_gear(GEAR, "normal", theme)
    paste(gear, 300-gear.width//2, 40)
    start = render_button(spec["start_button"][0], "play", START, "normal", theme)
    paste(start, 300-start.width//2, 300)

    # rechts: settings-titel + 3 knoppen
    y = 60
    title = render_title(spec["settings_title"][0], TITLE, theme)
    paste(title, 900-title.width//2, y); y += 140
    for key in ["taal_button", "pin_button", "settings_back"]:
        text, icon, size, kind = spec[key]
        state = "disabled" if kind == "static" else "normal"
        btn = render_button(text, icon, size, state, theme)
        paste(btn, 900-btn.width//2, y); y += 175

    canvas.convert("RGB").save(os.path.join(ldir, f"_preview_{tname}_{lang}.png"))


def main():
    ap = argparse.ArgumentParser(description="KittenHATS multi-thema generator")
    ap.add_argument("--theme", nargs="+", default=list(THEMES.keys()),
                    choices=list(THEMES.keys()))
    ap.add_argument("--lang", nargs="+", default=list(TRANSLATIONS.keys()),
                    choices=list(TRANSLATIONS.keys()))
    ap.add_argument("--home-only", action="store_true",
                    help="alleen SPELEN + tandwiel")
    ap.add_argument("--no-preview", action="store_true")
    ap.add_argument("--only", nargs="+", default=None,
                    help="Alleen deze asset-keys genereren (bv. pin_title back_button)")
    ap.add_argument("--output", default="out_themes",
                    help="Output directory (default: out_themes)")
    args = ap.parse_args()

    # Pre-flight: validate --only keys
    if args.only is not None:
        known_keys = set().union(*(t.keys() for t in TRANSLATIONS.values()))
        unknown = set(args.only) - known_keys
        if unknown:
            raise SystemExit(
                f"Fout: onbekende asset key(s): {', '.join(sorted(unknown))}\n"
                f"Bekende keys: {', '.join(sorted(known_keys))}"
            )
        # Verify all selected languages have the requested keys
        for lang in args.lang:
            spec = TRANSLATIONS[lang]
            missing = [k for k in args.only if k not in spec]
            if missing:
                raise SystemExit(
                    f"Fout: taal '{lang}' mist keys: {', '.join(missing)}"
                )

    print(f"Thema's: {', '.join(args.theme)} | Talen: {', '.join(args.lang)}"
          f"{' | Only: ' + ', '.join(args.only) if args.only else ''}"
          f" | Output: {args.output}")
    build(args.theme, args.lang, args.home_only, not args.no_preview,
          only_keys=set(args.only) if args.only else None,
          root=args.output)


if __name__ == "__main__":
    main()
