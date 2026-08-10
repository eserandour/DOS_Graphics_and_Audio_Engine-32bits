#!/usr/bin/env python3
# Usage: python3 psf2c.py FICHIER.psf[.gz]               → un seul fichier
#        python3 psf2c.py *.psf *.psf.gz                 → plusieurs fichiers
#        python3 psf2c.py REPERTOIRE/                    → tous les .psf/.psf.gz du répertoire
#        python3 psf2c.py FICHIER.psf -o REPERTOIRE/     → répertoire de sortie
#        python3 psf2c.py FICHIER.psf --encoding cp850   → commentaires en CP850 (défaut)
#        python3 psf2c.py FICHIER.psf --encoding cp437   → commentaires en CP437
#        python3 psf2c.py FICHIER.psf --encoding unicode → commentaires caractère Unicode direct
# Génère FICHIER.c et FICHIER.png à partir d'une police PSF1 ou PSF2

# Dans Debian, chercher un fichier .psf.gz dans /usr/share/consolefonts/ par exemple

import sys
import os
import gzip
import glob
import struct
import argparse

PSF_SUFFIXES = ('.psf', '.psf.gz')

def psf_basename(path):
    """Retourne le nom de base sans l'extension PSF (quelle que soit la variante)."""
    for suffix in sorted(PSF_SUFFIXES, key=len, reverse=True):
        if path.endswith(suffix):
            return path[:-len(suffix)]
    return os.path.splitext(path)[0]

def is_psf(path):
    return any(path.endswith(s) for s in PSF_SUFFIXES)

PSF1_MAGIC = b'\x36\x04'
PSF2_MAGIC = b'\x72\xb5\x4a\x86'

def read_psf(font_file):
    """Lit un fichier PSF1 ou PSF2 (éventuellement gzippé).
    Retourne (chars, char_width, char_height) où chars est une liste de bytes."""
    opener = gzip.open if font_file.endswith('.gz') else open
    with opener(font_file, 'rb') as f:
        data = f.read()

    if data[:2] == PSF1_MAGIC:
        chars, w, h = _read_psf1(data)
        return chars, w, h, 1
    elif data[:4] == PSF2_MAGIC:
        chars, w, h = _read_psf2(data)
        return chars, w, h, 2
    else:
        raise ValueError("Format inconnu : ni PSF1 ni PSF2")

def _read_psf1(data):
    # En-tête PSF1 : magic(2) + mode(1) + charsize(1)
    charsize  = data[3]
    char_width  = 8   # PSF1 : toujours 8 px de large
    char_height = charsize
    num_chars = 512 if (data[2] & 0x01) else 256

    chars = []
    for i in range(num_chars):
        start = 4 + i * charsize
        chars.append(data[start:start + charsize])

    return chars, char_width, char_height

def _read_psf2(data):
    # En-tête PSF2 (32 octets) :
    #   magic(4) version(4) headersize(4) flags(4)
    #   num_chars(4) charsize(4) height(4) width(4)
    hdr = struct.unpack_from('<8I', data, 0)
    _, _, headersize, _, num_chars, charsize, char_height, char_width = hdr

    chars = []
    for i in range(num_chars):
        start = headersize + i * charsize
        chars.append(data[start:start + charsize])

    return chars, char_width, char_height

# ==============================================================================
# Glyphes de commentaire selon l'encodage
# ==============================================================================

CP437_CONTROL_GLYPHS = {
    0: '∅',
    1: '☺', 2: '☻', 3: '♥', 4: '♦', 5: '♣', 6: '♠',
    7: '•', 8: '◘', 9: '○', 10: '◙', 11: '♂', 12: '♀',
    13: '♪', 14: '♫', 15: '☼', 16: '►', 17: '◄', 18: '↕',
    19: '‼', 20: '¶', 21: '§', 22: '▬', 23: '↨', 24: '↑',
    25: '↓', 26: '→', 27: '←', 28: '∟', 29: '↔', 30: '▲',
    31: '▼', 127: '⌂'
}

def get_char_label(i: int, encoding: str = 'cp850') -> str:
    """
    Retourne le label (caractère lisible) pour le code i selon l'encodage choisi.

    encoding : 'cp850'   → CP850 pour 0x20-0xFF, CP437 pour les contrôles (0x00-0x1F, 0x7F)
               'cp437'   → CP437 pour 0x20-0xFF, CP437 pour les contrôles
               'unicode' → caractère Unicode direct (chr(i)), U+XXXX si non imprimable
    """
    if encoding == 'unicode':
        import unicodedata
        try:
            ch = chr(i)
            if unicodedata.category(ch) in ('Cc', 'Cs', 'Co', 'Cn'):
                return f'U+{i:04X}'
            return ch
        except Exception:
            return f'U+{i:04X}'

    # CP437 ou CP850 : les contrôles (0x00-0x1F et 0x7F) sont identiques
    if i in CP437_CONTROL_GLYPHS:
        return CP437_CONTROL_GLYPHS[i]

    # Zone imprimable : décodage selon l'encodage demandé
    codec = 'cp437' if encoding == 'cp437' else 'cp850'
    try:
        return bytes([i]).decode(codec)
    except Exception:
        return f'0x{i:02X}'

# ==============================================================================
# Génération du fichier C
# ==============================================================================

def generate_c_file(chars, char_width, char_height, array_name, font_file,
                    psf_version, encoding='cp850'):
    """Génère un fichier C complet au même format que ttf2c.py."""
    font_name = os.path.basename(psf_basename(font_file))

    lines = []
    lines.append('#include "../font1.h"')
    lines.append("")
    lines.append("/* =========================================================")
    lines.append(f"   Police : {font_name} — {os.path.basename(font_file)}")
    lines.append(f"   Format : PSF version {psf_version}")
    lines.append(f"   Taille : {char_width}×{char_height} pixels")
    lines.append(f"   Généré par psf2c.py")
    lines.append(f"   Encodage commentaires : {encoding.upper()}")
    if char_width <= 8:
        lines.append(f"   Chaque octet = une ligne de {char_width} pixels.")
        lines.append( "   Bit 7 (0x80) = pixel le plus à gauche.")
    else:
        lines.append(f"   Chaque unsigned int = une ligne de {char_width} pixels.")
        lines.append( "   Bit 15 (0x8000) = pixel le plus à gauche.")
    lines.append( "   ========================================================= */")
    lines.append("")
    lines.append(f"void _initFont1_{char_width}x{char_height}(void)")
    lines.append("{")

    # Largeur du numéro de caractère alignée sur le nombre max de chiffres
    num_width = len(str(len(chars) - 1))

    # Pré-construire toutes les lignes sans le ';' pour mesurer la longueur max
    raw_lines = []
    for i, char_data in enumerate(chars):
        hex_bytes = [f"0x{b:02x}" for b in char_data]
        raw = f"    font1DefineChar{char_width}x{char_height}({array_name}, {i:{num_width}}, {', '.join(hex_bytes)})"
        raw_lines.append(raw)

    max_len = max(len(r) for r in raw_lines)

    for i, (raw, char_data) in enumerate(zip(raw_lines, chars)):
        hex_comment  = f"/* 0x{i:02X} */"
        char_comment = f"/* {get_char_label(i, encoding)} */"
        line = f"{raw:<{max_len}}; {hex_comment}    {char_comment}"
        lines.append(line)

    lines.append("}")
    lines.append("")
    return "\n".join(lines)

# ==============================================================================
# Génération du PNG
# ==============================================================================

def generate_png(chars, char_width, char_height, output_path, font_name="", psf_version=2):
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        print("Erreur : Pillow n'est pas installé. Lancez : pip install pillow", file=sys.stderr)
        sys.exit(1)

    bytes_per_row = (char_width + 7) // 8
    cols = 16             # 16 caractères par ligne
    rows = (len(chars) + cols - 1) // cols

    padding  = 4          # espace entre les glyphes (px)
    label_h  = 10         # hauteur réservée pour le numéro hex sous chaque glyphe
    margin   = 20         # marge extérieure
    scale    = 8          # facteur d'agrandissement des pixels

    cell_w = char_width  * scale + padding
    cell_h = char_height * scale + padding + label_h

    title_h  = 20         # hauteur du bandeau titre en haut

    img_w = cols * cell_w + 2 * margin
    img_h = rows * cell_h + 2 * margin + title_h

    img = Image.new("RGB", (img_w, img_h), color=(0, 255, 255))   # fond général
    draw = ImageDraw.Draw(img)

    # Titre
    title = (f"Filename : {font_name}  -  PSF version : {psf_version}  -  "
             f"Glyph size : {char_width} x {char_height} pixels  -  "
             f"Glyph count : {len(chars)}  -  "
             f"Zoom : {scale}x")
    draw.text((margin, (title_h - 10) // 2), title, fill=(0, 0, 0))

    for idx, char_data in enumerate(chars):
        col = idx % cols
        row = idx // cols
        x0 = margin + col * cell_w
        y0 = margin + row * cell_h + title_h

        # Fond noir de la cellule
        draw.rectangle(
            [x0, y0, x0 + char_width * scale - 1, y0 + char_height * scale - 1],
            fill=(0, 0, 0)
        )

        # Dessiner les pixels du glyphe (blanc)
        for y in range(char_height):
            row_bytes = char_data[y * bytes_per_row:(y + 1) * bytes_per_row]
            for x in range(char_width):
                byte_idx = x // 8
                bit_idx  = 7 - (x % 8)
                if byte_idx < len(row_bytes) and (row_bytes[byte_idx] >> bit_idx) & 1:
                    px = x0 + x * scale
                    py = y0 + y * scale
                    draw.rectangle(
                        [px, py, px + scale - 1, py + scale - 1],
                        fill=(255, 255, 255)
                    )

        # Numéro hex sous le glyphe
        label = f"{idx:02X}"
        draw.text((x0, y0 + char_height * scale), label, fill=(0, 0, 0))

    img.save(output_path)
    print(f"  PNG : {output_path}")

# ==============================================================================
# Collecte des fichiers PSF
# ==============================================================================

def collect_files(args):
    """Retourne la liste de tous les fichiers PSF à traiter."""
    files = []
    for arg in args:
        if os.path.isdir(arg):
            for suffix in PSF_SUFFIXES:
                files += sorted(glob.glob(os.path.join(arg, f'*{suffix}')))
        elif is_psf(arg):
            files.append(arg)
        else:
            print(f"Ignoré (extension inconnue) : {arg}", file=sys.stderr)
    # Dédoublonner en conservant l'ordre
    seen = set()
    return [f for f in files if not (f in seen or seen.add(f))]

# ==============================================================================
# Traitement d'un fichier
# ==============================================================================

def process_file(font_file, encoding='cp850', out_dir=None):
    base_name = psf_basename(font_file)
    png_file = base_name + ".png"

    chars, char_width, char_height, psf_version = read_psf(font_file)

    if out_dir is None:
        out_dir = os.path.dirname(os.path.abspath(__file__))
    c_file     = os.path.join(out_dir, f"f1_{char_width}x{char_height}.c")
    array_name = f"font1Bank{char_width}x{char_height}"
    c_code = generate_c_file(chars, char_width, char_height, array_name,
                              font_file, psf_version, encoding)
    with open(c_file, "w", encoding="utf-8") as f:
        f.write(c_code)
    print(f"  .c  : {c_file}")

    png_file  = os.path.join(out_dir, os.path.basename(base_name) + ".png")
    font_name = os.path.basename(base_name)
    generate_png(chars, char_width, char_height, png_file, font_name, psf_version)

# ==============================================================================
# Point d'entrée
# ==============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Convertit un ou plusieurs fichiers PSF en données C + PNG.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Exemples :
  python3 psf2c.py cp850-8x16.psf.gz                    # CP850 (défaut)
  python3 psf2c.py cp850-8x16.psf.gz --encoding cp437   # commentaires en CP437
  python3 psf2c.py cp850-8x16.psf.gz --encoding unicode # commentaires Unicode direct
  python3 psf2c.py /usr/share/consolefonts/             # tous les PSF d'un répertoire
"""
    )
    parser.add_argument("sources", nargs="+",
        metavar="FICHIER_ou_REPERTOIRE",
        help="Fichier(s) .psf/.psf.gz ou répertoire(s) à scanner")
    parser.add_argument("-o", "--output", default=None,
        metavar="REPERTOIRE",
        help="Répertoire de sortie pour f1_WxH.c et le PNG. "
             "Créé automatiquement s'il n'existe pas.")
    parser.add_argument("--encoding", default="cp850",
        choices=["cp850", "cp437", "unicode"],
        metavar="ENC",
        help="Encodage utilisé pour les commentaires de caractères dans le .c généré. "
             "Valeurs : cp850 (défaut), cp437, unicode. "
             "Exemple : --encoding cp437")

    args = parser.parse_args()

    files = collect_files(args.sources)
    if not files:
        print("Aucun fichier .psf ou .psf.gz trouvé.", file=sys.stderr)
        sys.exit(1)

    ok = err = 0
    for font_file in files:
        print(f"\n→ {font_file}")
        try:
            out_dir = args.output
            if out_dir:
                os.makedirs(out_dir, exist_ok=True)
            process_file(font_file, args.encoding, out_dir)
            ok += 1
        except FileNotFoundError:
            print(f"  Erreur : fichier introuvable", file=sys.stderr)
            err += 1
        except ValueError as e:
            print(f"  Erreur : {e}", file=sys.stderr)
            err += 1

    print(f"\n{ok} police(s) traitée(s)", end="")
    if err:
        print(f", {err} erreur(s).", file=sys.stderr)
    else:
        print(".")

if __name__ == "__main__":
    main()
