#!/usr/bin/env python3
"""
KittenHATS UI button generator
==============================================================================
Genereert alle Settings-, Taal- en meertalige knoppen in EEN consistente stijl,
gebaseerd op de "SPELEN"-knop van het homescherm:

    - blauwe verticale gradient body
    - dikke goud/oranje afgeronde rand met buitengloed
    - subtiele binnen-highlight bovenaan (glans)
    - witte tekst met donkere schaduw
    - optioneel icoon links van de tekst (slot, pijl, vinkje, vlag-stip)

Output: 32-bit BGRA .bmp bestanden (met alfakanaal), klaar voor LVGL.

Gebruik:
    python3 generate_buttons.py                 # genereer alles -> ./out
    python3 generate_buttons.py --lang nl en    # alleen deze talen
    python3 generate_buttons.py --settings-only  # alleen de 5 Settings-BMPs
    python3 generate_buttons.py --preview        # maak ook 1 preview-PNG per scherm

Stijl aanpassen: pas het THEME-blok hieronder aan (kleuren, radius, randdikte).
==============================================================================
"""

import argparse
import os
import struct
from PIL import Image, ImageDraw, ImageFont, ImageFilter

# ---------------------------------------------------------------------------
# THEME  -- pas dit aan om de hele look te veranderen
# ---------------------------------------------------------------------------
THEME = {
    # Body gradient (boven -> onder). Overgenomen van de SPELEN-knop.
    "body_top":        (122, 138, 226),   # lichter blauw-paars bovenaan
    "body_bottom":     (74,  92,  190),   # dieper blauw onderaan

    # "Disabled" variant (voor de TAAL-knop die niet klikbaar is)
    "disabled_top":    (150, 152, 168),
    "disabled_bottom": (120, 122, 140),

    # "Selected" variant (actieve radioknop, nl/en selectie)
    "selected_top":    (104, 200, 140),   # fris groen bovenaan
    "selected_bottom": (58,  150, 96),    # dieper groen onderaan

    # Rand
    "border_outer":    (208, 150, 70),    # goud/oranje (zoals SPELEN)
    "border_inner":    (255, 206, 130),   # lichte binnenlijn van de rand
    "border_disabled": (170, 172, 185),   # grijze rand voor disabled

    # Tekst
    "text":            (255, 255, 255),
    "text_shadow":     (30,  36,  80, 200),
    "text_disabled":   (238, 240, 248),

    # Buitengloed rondom de rand
    "glow":            (255, 190, 110),

    # Geometrie
    "radius_frac":     0.46,   # hoekradius als fractie van de hoogte (pill-vorm)
    "border_px":       7,      # dikte van de goud rand
    "glow_px":         10,     # hoe ver de gloed naar buiten valt
    "pressed_shift":   True,   # pressed = iets donkerder + tekst 2px omlaag
}

FONT_PATH = "/usr/share/fonts/truetype/google-fonts/Poppins-Bold.ttf"
SS = 4  # supersampling-factor voor gladde randen

# ---------------------------------------------------------------------------
# Teksten per taal
# ---------------------------------------------------------------------------
# Elk item: (bestandsnaam-zonder-variant, tekst, icoon, formaat, soort)
#   icoon: None | "lock" | "arrow" | "check" | "flag"
#   formaat: (breedte, hoogte)
#   soort: "button" (normal+pressed) | "radio" (normal+pressed+selected)
#          | "title" (1 plaatje, transparante achtergrond) | "static" (alleen normal)

BTN = (420, 160)
TITLE = (500, 120)

TRANSLATIONS = {
    "nl": {
        "settings_title":  ("INSTELLINGEN", None, TITLE, "title"),
        "taal_button":     ("TAAL",        None,    BTN, "static"),
        "pin_button":      ("PIN",         "lock",  BTN, "button"),
        "settings_back":   ("TERUG",       "arrow", BTN, "button"),
        "language_title":  ("Taal / Language", None, TITLE, "title"),
        "nl_button":       ("Nederlands",  "flag",  BTN, "radio"),
        "en_button":       ("English",     "flag",  BTN, "radio"),
        "lang_back":       ("TERUG",       "arrow", BTN, "button"),
    },
    "en": {
        "settings_title":  ("SETTINGS",    None,    TITLE, "title"),
        "taal_button":     ("LANGUAGE",    None,    BTN, "static"),
        "pin_button":      ("PIN",         "lock",  BTN, "button"),
        "settings_back":   ("BACK",        "arrow", BTN, "button"),
        "language_title":  ("Language",    None,    TITLE, "title"),
        "nl_button":       ("Nederlands",  "flag",  BTN, "radio"),
        "en_button":       ("English",     "flag",  BTN, "radio"),
        "lang_back":       ("BACK",        "arrow", BTN, "button"),
    },
    "de": {
        "settings_title":  ("EINSTELLUNGEN", None,  TITLE, "title"),
        "taal_button":     ("SPRACHE",     None,    BTN, "static"),
        "pin_button":      ("PIN",         "lock",  BTN, "button"),
        "settings_back":   ("ZURÜCK",      "arrow", BTN, "button"),
        "language_title":  ("Sprache / Language", None, TITLE, "title"),
        "de_button":       ("Deutsch",     "flag",  BTN, "radio"),
        "en_button":       ("English",     "flag",  BTN, "radio"),
        "lang_back":       ("ZURÜCK",      "arrow", BTN, "button"),
    },
    "fr": {
        "settings_title":  ("PARAMÈTRES",  None,    TITLE, "title"),
        "taal_button":     ("LANGUE",      None,    BTN, "static"),
        "pin_button":      ("PIN",         "lock",  BTN, "button"),
        "settings_back":   ("RETOUR",      "arrow", BTN, "button"),
        "language_title":  ("Langue / Language", None, TITLE, "title"),
        "fr_button":       ("Français",    "flag",  BTN, "radio"),
        "en_button":       ("English",     "flag",  BTN, "radio"),
        "lang_back":       ("RETOUR",      "arrow", BTN, "button"),
    },
    "es": {
        "settings_title":  ("AJUSTES",     None,    TITLE, "title"),
        "taal_button":     ("IDIOMA",      None,    BTN, "static"),
        "pin_button":      ("PIN",         "lock",  BTN, "button"),
        "settings_back":   ("VOLVER",      "arrow", BTN, "button"),
        "language_title":  ("Idioma / Language", None, TITLE, "title"),
        "es_button":       ("Español",     "flag",  BTN, "radio"),
        "en_button":       ("English",     "flag",  BTN, "radio"),
        "lang_back":       ("VOLVER",      "arrow", BTN, "button"),
    },
}

# ---------------------------------------------------------------------------
# Teken-helpers
# ---------------------------------------------------------------------------

def _vgrad(size, top, bottom):
    """Verticale gradient als RGB-image."""
    w, h = size
    grad = Image.new("RGB", (1, h))
    for y in range(h):
        t = y / max(1, h - 1)
        grad.putpixel((0, y), tuple(
            int(round(top[i] + (bottom[i] - top[i]) * t)) for i in range(3)))
    return grad.resize((w, h))


def _rounded_mask(size, radius):
    m = Image.new("L", size, 0)
    ImageDraw.Draw(m).rounded_rectangle([0, 0, size[0] - 1, size[1] - 1],
                                        radius=radius, fill=255)
    return m


def _fit_font(text, max_w, max_h, start):
    """Grootste Poppins-maat die binnen max_w x max_h past."""
    size = start
    while size > 10:
        f = ImageFont.truetype(FONT_PATH, size)
        box = f.getbbox(text)
        if (box[2] - box[0]) <= max_w and (box[3] - box[1]) <= max_h:
            return f
        size -= 2
    return ImageFont.truetype(FONT_PATH, 10)


def _draw_icon(draw, kind, cx, cy, s, color):
    """Teken een simpel wit icoon gecentreerd op (cx, cy), grootte ~s."""
    if kind == "lock":
        bw, bh = s * 0.9, s * 0.7
        draw.rounded_rectangle([cx - bw/2, cy - bh/2 + s*0.12,
                                cx + bw/2, cy + bh/2 + s*0.12],
                               radius=s*0.12, fill=color)
        # beugel
        draw.arc([cx - bw*0.34, cy - bh*0.75, cx + bw*0.34, cy + bh*0.1],
                 start=180, end=360, fill=color, width=max(3, int(s*0.12)))
        # sleutelgat
        draw.ellipse([cx - s*0.09, cy - s*0.02, cx + s*0.09, cy + s*0.16],
                     fill=THEME["body_bottom"])
    elif kind == "arrow":
        w = s * 0.5
        draw.polygon([(cx + w*0.2, cy - s*0.5), (cx - w*0.8, cy),
                      (cx + w*0.2, cy + s*0.5)], fill=color)
        draw.rectangle([cx + w*0.0, cy - s*0.16, cx + w*0.9, cy + s*0.16],
                       fill=color)
    elif kind == "check":
        w = max(4, int(s * 0.16))
        draw.line([(cx - s*0.45, cy + s*0.05), (cx - s*0.1, cy + s*0.4)],
                  fill=color, width=w, joint="curve")
        draw.line([(cx - s*0.1, cy + s*0.4), (cx + s*0.5, cy - s*0.45)],
                  fill=color, width=w, joint="curve")
    elif kind == "flag":
        # simpele ronde "taal"-stip met een bolletje erin
        r = s * 0.5
        draw.ellipse([cx - r, cy - r, cx + r, cy + r], fill=color)
        draw.ellipse([cx - r*0.45, cy - r*0.45, cx + r*0.45, cy + r*0.45],
                     fill=THEME["body_bottom"])


# ---------------------------------------------------------------------------
# Kern: 1 knop renderen
# ---------------------------------------------------------------------------

def render_button(text, icon, size, state):
    """
    state: 'normal' | 'pressed' | 'selected' | 'disabled'
    Retourneert RGBA-image op ware grootte.
    """
    w, h = size
    W, H = w * SS, h * SS
    glow_px = THEME["glow_px"] * SS
    pad = glow_px + 2 * SS  # ruimte voor de gloed rondom

    canvas = Image.new("RGBA", (W + 2*pad, H + 2*pad), (0, 0, 0, 0))
    radius = int(H * THEME["radius_frac"])

    # kleuren per state
    if state == "disabled":
        top, bot = THEME["disabled_top"], THEME["disabled_bottom"]
        border_o = THEME["border_disabled"]
        border_i = THEME["border_disabled"]
        txt_col = THEME["text_disabled"]
        do_glow = False
    elif state == "selected":
        top, bot = THEME["selected_top"], THEME["selected_bottom"]
        border_o, border_i = THEME["border_outer"], THEME["border_inner"]
        txt_col = THEME["text"]
        do_glow = True
    else:
        top, bot = THEME["body_top"], THEME["body_bottom"]
        border_o, border_i = THEME["border_outer"], THEME["border_inner"]
        txt_col = THEME["text"]
        do_glow = True

    if state == "pressed" and THEME["pressed_shift"]:
        top = tuple(int(c * 0.86) for c in top)
        bot = tuple(int(c * 0.86) for c in bot)

    body_box = (pad, pad, pad + W, pad + H)

    # --- buitengloed ---
    if do_glow:
        glow = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
        gd = ImageDraw.Draw(glow)
        gd.rounded_rectangle(body_box, radius=radius,
                             fill=THEME["glow"] + (180,))
        glow = glow.filter(ImageFilter.GaussianBlur(glow_px * 0.6))
        canvas = Image.alpha_composite(canvas, glow)

    # --- goud rand (iets groter dan body) ---
    bpx = THEME["border_px"] * SS
    border_layer = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    bd = ImageDraw.Draw(border_layer)
    bd.rounded_rectangle(body_box, radius=radius, fill=border_o + (255,))
    canvas = Image.alpha_composite(canvas, border_layer)

    # --- body (binnen de rand) ---
    inner_box = (body_box[0] + bpx, body_box[1] + bpx,
                 body_box[2] - bpx, body_box[3] - bpx)
    iw, ih = inner_box[2] - inner_box[0], inner_box[3] - inner_box[1]
    inner_radius = max(1, radius - bpx)
    grad = _vgrad((iw, ih), top, bot).convert("RGBA")
    gmask = _rounded_mask((iw, ih), inner_radius)
    body = Image.new("RGBA", (iw, ih), (0, 0, 0, 0))
    body.paste(grad, (0, 0), gmask)

    # binnen-highlight bovenaan (glans)
    hl = Image.new("RGBA", (iw, ih), (0, 0, 0, 0))
    hd = ImageDraw.Draw(hl)
    hd.rounded_rectangle([0, 0, iw, int(ih * 0.5)],
                         radius=inner_radius, fill=(255, 255, 255, 55))
    hl = hl.filter(ImageFilter.GaussianBlur(6 * SS))
    hl.putalpha(hl.getchannel("A").point(lambda a: min(a, 70)))
    body = Image.alpha_composite(body, Image.composite(
        hl, Image.new("RGBA", (iw, ih), (0, 0, 0, 0)), gmask))

    # dunne lichte binnenlijn langs de rand
    ld = ImageDraw.Draw(body)
    ld.rounded_rectangle([1, 1, iw - 2, ih - 2], radius=inner_radius,
                         outline=border_i + (150,), width=max(1, SS))

    canvas.paste(body, (inner_box[0], inner_box[1]), body)

    # --- tekst + icoon ---
    draw = ImageDraw.Draw(canvas)
    has_icon = icon is not None
    icon_zone = ih * 0.9 if has_icon else 0

    # tekstruimte
    txt_max_w = iw - (icon_zone + 40 * SS if has_icon else 60 * SS)
    txt_max_h = int(ih * 0.5)
    font = _fit_font(text, txt_max_w, txt_max_h, start=int(ih * 0.5))

    tb = font.getbbox(text)
    tw, th = tb[2] - tb[0], tb[3] - tb[1]

    # layout: [icoon] [tekst] gecentreerd als geheel
    gap = 26 * SS if has_icon else 0
    icon_s = ih * 0.42
    total_w = (icon_s * 1.4 + gap if has_icon else 0) + tw
    start_x = inner_box[0] + (iw - total_w) / 2
    cy = inner_box[1] + ih / 2

    y_off = (3 * SS if state == "pressed" and THEME["pressed_shift"] else 0)

    if has_icon:
        icx = start_x + icon_s * 0.7
        _draw_icon(draw, icon, icx, cy + y_off, icon_s, txt_col)
        tx = start_x + icon_s * 1.4 + gap - tb[0]
    else:
        tx = start_x - tb[0]

    ty = cy - th / 2 - tb[1] + y_off

    # schaduw + tekst
    sh = THEME["text_shadow"]
    draw.text((tx + 2*SS, ty + 2*SS), text, font=font, fill=sh)
    draw.text((tx, ty), text, font=font, fill=txt_col + (255,))

    # terugschalen
    out = canvas.resize((canvas.size[0] // SS, canvas.size[1] // SS),
                        Image.LANCZOS)
    return out


def render_title(text, size):
    """Titeltekst met gloed, transparante achtergrond."""
    w, h = size
    W, H = w * SS, h * SS
    canvas = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(canvas)
    font = _fit_font(text, int(W * 0.92), int(H * 0.55), start=int(H * 0.6))
    tb = font.getbbox(text)
    tw, th = tb[2] - tb[0], tb[3] - tb[1]
    tx = (W - tw) / 2 - tb[0]
    ty = (H - th) / 2 - tb[1]

    # zachte gloed achter de tekst
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(glow).text((tx, ty), text, font=font,
                              fill=(150, 170, 255, 255))
    glow = glow.filter(ImageFilter.GaussianBlur(6 * SS))
    canvas = Image.alpha_composite(canvas, glow)

    draw = ImageDraw.Draw(canvas)
    draw.text((tx + 2*SS, ty + 2*SS), text, font=font, fill=(20, 26, 70, 200))
    draw.text((tx, ty), text, font=font, fill=(255, 255, 255, 255))

    return canvas.resize((w, h), Image.LANCZOS)


# ---------------------------------------------------------------------------
# BMP-export als 32-bit BGRA (met alfa)
# ---------------------------------------------------------------------------

def save_bgra_bmp(img, path):
    """Schrijf RGBA-image als 32-bit BMP met BGRA-volgorde (BITMAPV4 header)."""
    img = img.convert("RGBA")
    w, h = img.size
    px = img.load()
    rows = bytearray()
    # BMP: onderste rij eerst
    for y in range(h - 1, -1, -1):
        for x in range(w):
            r, g, b, a = px[x, y]
            rows += bytes((b, g, r, a))
    pixel_data = bytes(rows)

    header_size = 108  # BITMAPV4HEADER, exact
    offset = 14 + header_size
    file_size = offset + len(pixel_data)
    # File header (14 bytes)
    fh = b'BM' + struct.pack('<IHHI', file_size, 0, 0, offset)
    # BITMAPV4HEADER (108 bytes)
    v4 = struct.pack(
        '<IiiHHIIiiII',
        header_size, w, h, 1, 32, 3,   # size,w,h,planes,bpp,BI_BITFIELDS  (40)
        len(pixel_data), 2835, 2835, 0, 0)  # imgsize,ppmx,ppmy,clrused,clrimp
    v4 += struct.pack('<IIII', 0x00FF0000, 0x0000FF00,
                      0x000000FF, 0xFF000000)  # R,G,B,A masks           (16)
    v4 += struct.pack('<I', 0)          # CSType = 0 (calibrated/none)     (4)
    v4 += b'\x00' * 36                   # CIEXYZTRIPLE endpoints         (36)
    v4 += struct.pack('<III', 0, 0, 0)   # gamma R,G,B                    (12)
    assert len(v4) == 108, len(v4)
    with open(path, 'wb') as f:
        f.write(fh + v4 + pixel_data)


# ---------------------------------------------------------------------------
# Orkestratie
# ---------------------------------------------------------------------------

def variants_for(kind):
    if kind == "button":
        return ["normal", "pressed"]
    if kind == "radio":
        return ["normal", "pressed", "selected"]
    if kind == "static":
        return ["normal"]  # disabled-look, 1 plaatje
    if kind == "title":
        return [None]
    return ["normal"]


def build(langs, out_root, settings_only, preview):
    settings_keys = {"settings_title", "taal_button", "pin_button",
                     "settings_back"}
    made = 0
    for lang in langs:
        spec = TRANSLATIONS[lang]
        lang_dir = os.path.join(out_root, lang)
        os.makedirs(lang_dir, exist_ok=True)
        for key, (text, icon, size, kind) in spec.items():
            if settings_only and key not in settings_keys:
                continue
            for var in variants_for(kind):
                if kind == "title":
                    img = render_title(text, size)
                    name = f"{key}.bmp"
                elif kind == "static":
                    img = render_button(text, icon, size, "disabled")
                    name = f"{key}_normal.bmp"
                else:
                    img = render_button(text, icon, size, var)
                    name = f"{key}_{var}.bmp"
                save_bgra_bmp(img, os.path.join(lang_dir, name))
                made += 1
        print(f"  [{lang}] {len([1 for k,(t,i,s,ki) in spec.items() for _ in variants_for(ki) if not (settings_only and k not in settings_keys)])} bestanden -> {lang_dir}")

        if preview:
            _make_preview(lang, spec, lang_dir)
    print(f"\nKlaar. {made} BMP-bestanden gegenereerd in ./{os.path.basename(out_root)}/")


def _make_preview(lang, spec, lang_dir):
    """Bouw 1 PNG die het Settings-scherm nabootst voor snelle controle."""
    canvas = Image.new("RGBA", (1280, 720), (14, 16, 34, 255))
    draw = ImageDraw.Draw(canvas)
    # sterretjes-achtergrond fake
    import random
    random.seed(1)
    for _ in range(120):
        x, y = random.randint(0, 1279), random.randint(0, 719)
        draw.ellipse([x, y, x+2, y+2], fill=(210, 220, 255, 180))

    def paste_center(img, cy):
        canvas.alpha_composite(img, ((1280 - img.width)//2, cy))

    y = 60
    title = render_title(spec["settings_title"][0], TITLE)
    paste_center(title, y); y += 150
    for key in ["taal_button", "pin_button", "settings_back"]:
        if key not in spec:
            continue
        text, icon, size, kind = spec[key]
        state = "disabled" if kind == "static" else "normal"
        btn = render_button(text, icon, size, state)
        paste_center(btn, y); y += 180
    canvas.convert("RGB").save(os.path.join(lang_dir, f"_preview_settings_{lang}.png"))


def main():
    ap = argparse.ArgumentParser(description="KittenHATS knop-generator")
    ap.add_argument("--lang", nargs="+", default=list(TRANSLATIONS.keys()),
                    choices=list(TRANSLATIONS.keys()),
                    help="welke talen (default: alle)")
    ap.add_argument("--out", default="out", help="output-map")
    ap.add_argument("--settings-only", action="store_true",
                    help="alleen de Settings-BMPs (geen taalselectie)")
    ap.add_argument("--preview", action="store_true",
                    help="genereer ook een preview-PNG per taal")
    args = ap.parse_args()

    print(f"Genereren voor talen: {', '.join(args.lang)}")
    os.makedirs(args.out, exist_ok=True)
    build(args.lang, args.out, args.settings_only, args.preview)


if __name__ == "__main__":
    main()
