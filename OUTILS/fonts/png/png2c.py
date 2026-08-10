#!/usr/bin/env python3
# ==============================================================================
# png2c.py — Convertit un PNG de police bitmap en données C (font1)
# ==============================================================================
#
# Format d'entrée attendu :
#   Image PNG en noir et blanc (1 bit ou palette 2 couleurs).
#   Grille de glyphes organisée en N_COLS colonnes × N_ROWS lignes.
#   Pixel noir (valeur 0) = pixel allumé du glyphe.
#   Pixel blanc (valeur ≠ 0) = fond transparent.
#
# Exemple : WY-700a.png → 512×128 px, 32 cols × 8 lignes, glyphes 16×16
#   → 256 glyphes (codes 0x00 à 0xFF)
#
# Usage :
#   python3 png2c.py POLICE.png                        → glyphes 16x16 (défaut)
#   python3 png2c.py POLICE.png -s 8x8                 → glyphes 8×8
#   python3 png2c.py POLICE.png -s 8x16                → glyphes 8×16
#   python3 png2c.py POLICE.png -s 16x16               → glyphes 16×16
#   python3 png2c.py POLICE.png -o REPERTOIRE/         → répertoire de sortie du .c
#   python3 png2c.py POLICE.png --start 0x20           → premier code caractère
#   python3 png2c.py POLICE.png --cols 16              → 16 glyphes par ligne
#   python3 png2c.py POLICE.png --encoding cp850       → commentaires CP850
#   python3 png2c.py POLICE.png --encoding cp437       → commentaires CP437
#   python3 png2c.py POLICE.png --encoding unicode     → commentaires Unicode
#   python3 png2c.py POLICE.png --preview              → aperçu ASCII terminal
#
# Sortie :
#   <nom>_WxH.c   → bloc prêt à intégrer dans font1/
#
# Dépendances :
#   pip install pillow
# ==============================================================================

import argparse
import os
import sys
import unicodedata

try:
    from PIL import Image
except ImportError:
    print("Erreur : Pillow n'est pas installé.")
    print("  pip install pillow")
    sys.exit(1)

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

    codec = 'cp437' if encoding == 'cp437' else 'cp850'
    try:
        return bytes([i]).decode(codec)
    except Exception:
        return f'0x{i:02X}'

# ==============================================================================
# Extraction des glyphes depuis le PNG
# ==============================================================================

def extract_glyphs(img: Image.Image, glyph_w: int, glyph_h: int,
                   n_cols: int, start_code: int) -> list:
    """
    Découpe l'image en glyphes et retourne une liste de (code, rows).

    code : code caractère (start_code + index du glyphe)
    rows : liste de glyph_h entiers représentant les lignes de pixels.
           Pour glyph_w <= 8  : unsigned char (8 bits)
           Pour glyph_w <= 16 : unsigned int  (16 bits)

    Pixel noir (luminosité 0) = bit allumé dans le glyphe.
    """
    # Convertir en niveaux de gris pour une détection robuste du noir,
    # quelle que soit la palette d'origine (1-bit, P, RGB…).
    gray = img.convert('L')
    px   = gray.load()

    img_w, img_h = gray.size
    n_cols_actual = img_w // glyph_w
    n_rows_actual = img_h // glyph_h

    # Utilise n_cols fourni par l'utilisateur, vérifié contre l'image.
    if n_cols != n_cols_actual:
        print(f"Attention : --cols {n_cols} mais l'image contient "
              f"{n_cols_actual} glyphes par ligne ({img_w} px / {glyph_w} px).",
              file=sys.stderr)

    glyphs = []
    idx    = 0

    for row in range(n_rows_actual):
        for col in range(n_cols_actual):
            x0   = col * glyph_w
            y0   = row * glyph_h
            code = start_code + idx
            rows = []

            for y in range(glyph_h):
                val = 0
                for x in range(glyph_w):
                    # Pixel noir (< 128) = bit allumé
                    if px[x0 + x, y0 + y] < 128:
                        if glyph_w <= 8:
                            val |= (0x80 >> x)
                        else:
                            val |= (0x8000 >> x)
                rows.append(val)

            glyphs.append((code, rows))
            idx += 1

    return glyphs

# ==============================================================================
# Aperçu ASCII dans le terminal
# ==============================================================================

def ascii_preview(code: int, rows: list, glyph_w: int, glyph_h: int,
                  encoding: str) -> None:
    label = get_char_label(code, encoding)
    msb   = 0x8000 if glyph_w > 8 else 0x80
    print(f"  Char 0x{code:02X} ({label!r}):")
    for r in rows:
        line = "".join("X" if r & (msb >> c) else "." for c in range(glyph_w))
        print("    " + line)
    print()

# ==============================================================================
# Génération du bloc C
# ==============================================================================

def generate_c_block(glyphs: list, glyph_w: int, glyph_h: int,
                     array_name: str, png_file: str,
                     encoding: str, preview: bool) -> str:
    """
    Génère un fichier C autonome au format font1 (font1DefineChar*).
    Format identique à ttf2c.py et psf2c.py.
    """
    png_basename = os.path.splitext(os.path.basename(png_file))[0]

    lines = []
    lines.append('#include "../font1.h"')
    lines.append("")
    lines.append("/* =========================================================")
    lines.append(f"   Police : {png_basename}.png")
    lines.append(f"   Taille : {glyph_w}×{glyph_h} pixels")
    lines.append(f"   Généré par png2c.py")
    lines.append(f"   Encodage commentaires : {encoding.upper()}")
    if glyph_w <= 8:
        lines.append(f"   Chaque octet = une ligne de {glyph_w} pixels.")
        lines.append( "   Bit 7 (0x80) = pixel le plus à gauche.")
    else:
        lines.append(f"   Chaque unsigned int = une ligne de {glyph_w} pixels.")
        lines.append( "   Bit 15 (0x8000) = pixel le plus à gauche.")
    lines.append( "   ========================================================= */")
    lines.append("")
    lines.append(f"void _initFont1_{glyph_w}x{glyph_h}(void)")
    lines.append("{")

    need_r = (glyph_w > 8)
    if need_r:
        lines.append(f"    unsigned int r[{glyph_h}];")
        lines.append("")

    max_code   = max(code for code, _ in glyphs) if glyphs else 0
    num_width  = len(str(max_code))

    for code, rows in glyphs:
        if preview:
            ascii_preview(code, rows, glyph_w, glyph_h, encoding)

        label = get_char_label(code, encoding)

        if glyph_w <= 8:
            hex_bytes = ", ".join(f"0x{b:02x}" for b in rows)
            raw       = (f"    font1DefineChar{glyph_w}x{glyph_h}"
                         f"({array_name}, {code:{num_width}}, {hex_bytes})")
            comment   = f"; /* 0x{code:02X} */    /* {label} */"
            lines.append(raw + comment)
        else:
            lines.append(f"    /* --- 0x{code:02X} --- */")
            per_line = 4
            for start in range(0, glyph_h, per_line):
                chunk = rows[start:start + per_line]
                parts = "; ".join(
                    f"r[{start+i:2d}]=0x{v:04X}" for i, v in enumerate(chunk)
                )
                lines.append(f"    {parts};")
            lines.append(
                f"    font1DefineChar{glyph_w}x{glyph_h}"
                f"({array_name}, {code:{num_width}}, r); "
                f"/* 0x{code:02X} */ /* {label} */"
            )
            lines.append("")

    lines.append("}")
    lines.append("")
    return "\n".join(lines)

# ==============================================================================
# Traitement principal
# ==============================================================================

def process(png_path: str, glyph_w: int, glyph_h: int,
            out_dir: str, n_cols: int, start_code: int,
            encoding: str, preview: bool) -> None:

    img = Image.open(png_path)
    img_w, img_h = img.size

    print(f"Image     : {png_path}  ({img_w}×{img_h} px)")
    print(f"Glyphe    : {glyph_w}×{glyph_h} px")
    print(f"Grille    : {img_w // glyph_w} cols × {img_h // glyph_h} lignes")
    print(f"Encodage  : {encoding.upper()}")

    glyphs = extract_glyphs(img, glyph_w, glyph_h, n_cols, start_code)
    print(f"Glyphes   : {len(glyphs)}  (codes 0x{start_code:02X} → "
          f"0x{start_code + len(glyphs) - 1:02X})")

    array_name = f"font1Bank{glyph_w}x{glyph_h}"
    c_file     = os.path.join(out_dir, f"f1_{glyph_w}x{glyph_h}.c")

    c_code = generate_c_block(glyphs, glyph_w, glyph_h,
                               array_name, png_path, encoding, preview)

    with open(c_file, "w", encoding="utf-8") as f:
        f.write(c_code)
    print(f"  .c  : {c_file}")

    print()
    print("Intégration :")
    print(f"  1. Ajouter {c_file} au projet (compiler avec les autres .c).")
    print(f"  2. Vérifier que _initFont1_{glyph_w}x{glyph_h}() est déclarée dans font1.h.")
    print(f"  3. S'assurer que font1InitBank{glyph_w}x{glyph_h}() l'appelle dans font1.c.")

# ==============================================================================
# Point d'entrée
# ==============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Convertit un PNG de police bitmap en données C (font1).",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Exemples :
  python3 png2c.py wy700-1.png                        # 16x16, CP850
  python3 png2c.py wy700-1.png -s 8x8                 # glyphes 8×8
  python3 png2c.py wy700-1.png -s 8x16                # glyphes 8×16
  python3 png2c.py wy700-1.png --cols 16              # 16 glyphes par ligne
  python3 png2c.py wy700-1.png --start 0x20           # commence à 0x20
  python3 png2c.py wy700-1.png --encoding cp437       # commentaires CP437
  python3 png2c.py wy700-1.png --encoding unicode     # commentaires Unicode
  python3 png2c.py wy700-1.png --preview              # aperçu ASCII terminal
"""
    )
    parser.add_argument("source",
        metavar="PNG",
        help="Fichier PNG source de la police bitmap")
    parser.add_argument("-s", "--size", default="16x16",
        metavar="WxH",
        help="Taille d'un glyphe en pixels, ex. 8x8, 8x16, 16x16 (défaut : 16x16)")
    parser.add_argument("-o", "--output", default=None,
        metavar="BASE",
        help="Répertoire de sortie pour f1_WxH.c (défaut : répertoire du script)")
    parser.add_argument("--cols", default=None, type=int,
        metavar="N",
        help="Nombre de glyphes par ligne dans le PNG (auto-détecté si omis)")
    parser.add_argument("--start", default="0x00",
        metavar="CODE",
        help="Code du premier glyphe en hex (défaut : 0x00)")
    parser.add_argument("--encoding", default="cp850",
        choices=["cp850", "cp437", "unicode"],
        metavar="ENC",
        help="Encodage des commentaires : cp850 (défaut), cp437, unicode")
    parser.add_argument("--preview", action="store_true",
        help="Affiche un aperçu ASCII de chaque glyphe dans le terminal")

    args = parser.parse_args()

    # --- Vérification du fichier source ---
    if not os.path.isfile(args.source):
        print(f"Erreur : fichier introuvable : {args.source}", file=sys.stderr)
        sys.exit(1)

    # --- Parsing de la taille ---
    try:
        w_str, h_str = args.size.lower().split("x")
        glyph_w = int(w_str)
        glyph_h = int(h_str)
        assert glyph_w > 0 and glyph_h > 0
    except Exception:
        print(f"Erreur : format de taille invalide '{args.size}'.", file=sys.stderr)
        sys.exit(1)

    if glyph_w > 16:
        print(f"Attention : largeur {glyph_w} > 16 px — la macro font1DefineChar "
              f"utilise des unsigned int (16 bits).", file=sys.stderr)

    # --- Parsing du code de départ ---
    try:
        start_code = int(args.start, 0)   # accepte 0x20, 32, 0b100000…
    except Exception:
        print(f"Erreur : code de départ invalide '{args.start}'.", file=sys.stderr)
        sys.exit(1)

    # --- Nombre de colonnes (auto si non fourni) ---
    if args.cols is not None:
        n_cols = args.cols
    else:
        img = Image.open(args.source)
        n_cols = img.width // glyph_w
        img.close()

    # --- Base de nom de sortie ---
    if args.output:
        out_dir = args.output
        os.makedirs(out_dir, exist_ok=True)
    else:
        out_dir = os.path.dirname(os.path.abspath(__file__))

    print(f"\n→ {args.source}")
    try:
        process(args.source, glyph_w, glyph_h,
                out_dir, n_cols, start_code,
                args.encoding, args.preview)
    except Exception as e:
        print(f"  Erreur : {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
