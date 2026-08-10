#!/usr/bin/env python3
# vgatool.py — Boîte à outils images pour le mode 13h VGA (320×200, 256 couleurs)
#
# Nécessite : python3-pillow
#
# COMMANDES
# ---------
#   convert <image>  [base]         Convertit une image en .raw + .pal + _palette.png
#   palette <fichier.pal> [sortie]  Visualise une palette .pal existante
#
# EXEMPLES
#   python3 vgatool.py convert  sprite.png
#   python3 vgatool.py convert  sprite.png  images/sprite
#   python3 vgatool.py palette  images/font.pal
#   python3 vgatool.py palette  images/font.pal  font_palette.png

import sys
import os
from PIL import Image, ImageDraw, ImageFont


# ══════════════════════════════════════════════════════════════════
#  RENDU DE PALETTE (commun aux deux commandes)
# ══════════════════════════════════════════════════════════════════

COLS        = 16
ROWS        = 16
SWATCH_SIZE = 32
MARGIN      = 20
HEADER      = 40
LABEL_H     = 16
FONT_SIZE   = 11
BG_COLOR    = (18, 18, 18)
TEXT_COLOR  = (220, 220, 220)

def _render_palette(title, colors, out_path):
    """Génère une image PNG depuis une liste de 256 tuples (r, g, b) en 8 bits."""
    cell_w  = SWATCH_SIZE
    cell_h  = SWATCH_SIZE + LABEL_H
    total_w = COLS * cell_w + 2 * MARGIN
    total_h = ROWS * cell_h + 2 * MARGIN + HEADER

    img  = Image.new("RGB", (total_w, total_h), BG_COLOR)
    draw = ImageDraw.Draw(img)

    try:
        font_title = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf", 16)
        font_label = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", FONT_SIZE)
    except Exception:
        font_title = ImageFont.load_default()
        font_label = font_title

    bbox = draw.textbbox((0, 0), title, font=font_title)
    tx   = (total_w - (bbox[2] - bbox[0])) // 2
    draw.text((tx, (HEADER - (bbox[3] - bbox[1])) // 2),
              title, fill=TEXT_COLOR, font=font_title)

    for idx, color in enumerate(colors):
        col = idx % COLS
        row = idx // COLS
        x0  = MARGIN + col * cell_w
        y0  = MARGIN + HEADER + row * cell_h

        draw.rectangle([x0, y0, x0 + cell_w - 1, y0 + SWATCH_SIZE - 1], fill=color)

        label = f"{idx:3d}"
        lbbox = draw.textbbox((0, 0), label, font=font_label)
        lw    = lbbox[2] - lbbox[0]
        lh    = lbbox[3] - lbbox[1]
        lx    = x0 + (cell_w - lw) // 2
        ly    = y0 + SWATCH_SIZE + (LABEL_H - lh) // 2

        r, g, b     = color
        luminance   = 0.299 * r + 0.587 * g + 0.114 * b
        label_color = (240, 240, 240) if luminance < 128 else (20, 20, 20)

        draw.rectangle([x0, y0 + SWATCH_SIZE,
                        x0 + cell_w - 1, y0 + cell_h - 1], fill=color)
        draw.text((lx, ly), label, fill=label_color, font=font_label)

    img.save(out_path)


# ══════════════════════════════════════════════════════════════════
#  COMMANDE : convert
#  Produit en une passe : .raw + .pal + _palette.png
# ══════════════════════════════════════════════════════════════════

def cmd_convert(args):
    if len(args) < 1:
        print("Usage: python3 vgatool.py convert <image> [base_sortie]")
        print("  image       : fichier source (PNG, JPG, BMP…)")
        print("  base_sortie : chemin de sortie sans extension (défaut : même dossier)")
        sys.exit(1)

    src  = args[0]
    base = args[1] if len(args) >= 2 else src.rsplit('.', 1)[0]

    img = Image.open(src)
    width, height = img.size

    # Cas 1 : image déjà indexée
    if img.mode == 'P':
        pass
    # Cas 2 : RGBA — aplatit sur fond noir
    elif img.mode == 'RGBA':
        bg = Image.new('RGB', img.size, (0, 0, 0))
        bg.paste(img, mask=img.split()[3])
        img = bg.quantize(colors=256, dither=Image.Dither.FLOYDSTEINBERG)
    # Cas 3 : RGB ou niveaux de gris
    else:
        img = img.convert('RGB')
        img = img.quantize(colors=256, dither=Image.Dither.FLOYDSTEINBERG)

    # .raw
    raw = img.tobytes()
    assert len(raw) == width * height
    with open(base + ".raw", "wb") as f:
        f.write(raw)

    # .pal (6 bits)
    pal8 = img.getpalette()
    pal8 += [0] * (768 - len(pal8))
    pal6 = bytes(v >> 2 for v in pal8)
    with open(base + ".pal", "wb") as f:
        f.write(pal6)

    # _palette.png — via pal6 reconverti en 8 bits : fidèle au rendu VGA réel
    colors = [(pal6[i*3] << 2, pal6[i*3+1] << 2, pal6[i*3+2] << 2) for i in range(256)]
    pal_png = base + "_palette.png"
    _render_palette(os.path.basename(base + ".pal"), colors, pal_png)

    print(f"Source     : {src}  ({width} × {height} px)")
    print(f"OK  {base}.raw  ({len(raw)} octets)")
    print(f"OK  {base}.pal  (256 couleurs sur 6 bits)")
    print(f"OK  {pal_png}")


# ══════════════════════════════════════════════════════════════════
#  COMMANDE : palette
#  Visualise un fichier .pal existant
# ══════════════════════════════════════════════════════════════════

def cmd_palette(args):
    if len(args) < 1:
        print("Usage: python3 vgatool.py palette <fichier.pal> [sortie.png]")
        sys.exit(1)

    pal_path = args[0]
    out_path = args[1] if len(args) >= 2 \
               else pal_path.rsplit('.', 1)[0] + "_palette.png"

    with open(pal_path, "rb") as f:
        raw = f.read()

    if len(raw) < 768:
        print(f"Erreur : {len(raw)} octets, attendu 768 (256 × RGB 6 bits).")
        sys.exit(1)

    # Décodage 6 bits → 8 bits
    colors = [(raw[i*3] << 2, raw[i*3+1] << 2, raw[i*3+2] << 2)
              for i in range(256)]

    _render_palette(os.path.basename(pal_path), colors, out_path)

    print(f"Palette    : {pal_path}  (256 couleurs)")
    print(f"OK  {out_path}")


# ══════════════════════════════════════════════════════════════════
#  POINT D'ENTRÉE
# ══════════════════════════════════════════════════════════════════

COMMANDS = {
    "convert": cmd_convert,
    "palette": cmd_palette,
}

def usage():
    print("Usage: python3 vgatool.py <commande> [arguments]")
    print()
    print("Commandes :")
    print("  convert <image> [base]      → .raw + .pal + _palette.png")
    print("  palette <fichier.pal> [png] → visualisation de palette")
    print()
    print("Exemples :")
    print("  python3 vgatool.py convert sprite.png")
    print("  python3 vgatool.py convert sprite.png images/sprite")
    print("  python3 vgatool.py palette images/font.pal")
    print("  python3 vgatool.py palette images/font.pal font_palette.png")

if __name__ == "__main__":
    if len(sys.argv) < 2 or sys.argv[1] not in COMMANDS:
        usage()
        sys.exit(0 if len(sys.argv) < 2 else 1)

    COMMANDS[sys.argv[1]](sys.argv[2:])
