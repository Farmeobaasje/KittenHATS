# KittenHATS — UI knoppen (Settings + Taal)

Alle knoppen zijn gegenereerd in **één consistente stijl**, gebaseerd op de
`SPELEN`-knop van het homescherm: blauwe verticale gradient, dikke goud/oranje
afgeronde rand met buitengloed, witte tekst met schaduw, en een icoon links.

## Formaat
- **32-bit BMP, BGRA-volgorde, met alfakanaal** (BITMAPV4HEADER, 108 bytes).
- Randen en gloed zijn transparant, dus de knoppen vallen netjes op je
  sterrenachtergrond.
- Knoppen: 420×160 px. Titels: 500×120 px.

> Let op: Pillow kan deze V4-BMP's zelf niet altijd teruglezen (bekende Pillow-
> beperking), maar het bestand is een geldige 32-bit BGRA-BMP. LVGL's
> `LVGLImage.py` / online image converter en `stb_image` lezen ze correct.
> Twijfel je over je toolchain? Zet in de converter het inputformaat op
> "True color with alpha (ARGB8888)".

## Mappen
Per taal een map met exact dezelfde bestandsnamen, zodat je alleen de actieve
taalmap hoeft te wisselen:

```
nl/  en/  de/  fr/  es/
```

## Bestanden per taal (15 stuks)

### Settings-scherm (direct te vervangen)
| Bestand | Grootte | Rol |
|---|---|---|
| `settings_title.bmp` | 500×120 | Titel ("INSTELLINGEN" / "SETTINGS" / …) |
| `taal_button_normal.bmp` | 420×160 | TAAL-knop — **disabled-stijl** (grijs) |
| `pin_button_normal.bmp` | 420×160 | PIN normal (met slot-icoon) |
| `pin_button_pressed.bmp` | 420×160 | PIN pressed (donkerder) |
| `settings_back_normal.bmp` | 420×160 | TERUG normal (met pijl) |
| `settings_back_pressed.bmp` | 420×160 | TERUG pressed |

### Taalselectie-scherm (nieuw)
| Bestand | Grootte | Rol |
|---|---|---|
| `language_title.bmp` | 500×120 | Titel "Taal / Language" |
| `xx_button_normal.bmp` | 420×160 | Taalkeuze normal (blauw) |
| `xx_button_pressed.bmp` | 420×160 | Taalkeuze pressed |
| `xx_button_selected.bmp` | 420×160 | Taalkeuze **actief** (groen) |
| `lang_back_normal.bmp` | 420×160 | Terug normal |
| `lang_back_pressed.bmp` | 420×160 | Terug pressed |

`xx` is de taalcode van die map:
- `nl/` heeft `nl_button_*` en `en_button_*`
- `de/` heeft `de_button_*` en `en_button_*`
- `fr/` heeft `fr_button_*` en `en_button_*`
- `es/` heeft `es_button_*` en `en_button_*`

De **selected** (groene) variant gebruik je voor de taal die op dit moment
actief is; de andere talen toon je in **normal**.

## Preview
In elke taalmap staat `_preview_settings_xx.png`: een nagebootst Settings-scherm
zodat je snel ziet hoe het eruitziet. Deze PNG's zijn alleen ter controle —
niet nodig op het apparaat.

## Zelf de stijl aanpassen
Draai `generate_buttons.py` opnieuw. Pas het `THEME`-blok bovenaan aan voor
kleuren, hoekradius (`radius_frac`), randdikte (`border_px`) en gloed
(`glow_px`). Teksten pas je aan in `TRANSLATIONS`.

```bash
python3 generate_buttons.py                  # alles -> ./out
python3 generate_buttons.py --lang nl en     # alleen deze talen
python3 generate_buttons.py --settings-only  # alleen de 5 Settings-BMPs
python3 generate_buttons.py --preview        # + preview-PNG per taal
```
