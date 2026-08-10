#!/usr/bin/env python3
# ==============================================================================
# ttf2c.py — Convertit un fichier TTF en données C + aperçu PNG
# ==============================================================================
# Modèle : psf2c.py (même style de sortie, même PNG)
#
# Usage :
#   python3 ttf2c.py POLICE.ttf                         → sortie POLICE_8x8.c + POLICE_8x8.png
#   python3 ttf2c.py *.ttf                              → plusieurs fichiers
#   python3 ttf2c.py REPERTOIRE/                        → tous les .ttf du répertoire
#   python3 ttf2c.py POLICE.ttf -s 16x16                → taille 16×16
#   python3 ttf2c.py POLICE.ttf -s 8x16                 → taille 8×16
#   python3 ttf2c.py POLICE.ttf -o REPERTOIRE/          → répertoire de sortie
#   python3 ttf2c.py POLICE.ttf --preview                → aperçu ASCII dans le terminal
#   python3 ttf2c.py POLICE.ttf --range 20-7E           → plage hex seulement
#   python3 ttf2c.py POLICE.ttf --chars "ABC"           → caractères spécifiques
#   python3 ttf2c.py POLICE.ttf --encoding cp850        → commentaires en CP850 (défaut)
#   python3 ttf2c.py POLICE.ttf --encoding cp437        → commentaires en CP437
#   python3 ttf2c.py POLICE.ttf --encoding unicode      → commentaires caractère Unicode direct
#
# Sortie :
#   <nom>_WxH.c   → bloc prêt à intégrer dans font1/
#   <nom>_WxH.png → aperçu visuel identique au style psf2c.py
#
# Dépendances :
#   pip install freetype-py pillow
# ==============================================================================

import argparse
import os
import sys

try:
    import freetype
except ImportError:
    print("Erreur : freetype-py n'est pas installé.")
    print("  pip install freetype-py")
    sys.exit(1)

try:
    from PIL import Image, ImageDraw
except ImportError:
    print("Erreur : Pillow n'est pas installé.")
    print("  pip install pillow")
    sys.exit(1)

# ==============================================================================
# Rendu d'un glyphe TTF → grille de pixels
# ==============================================================================

def render_char(face, code: int, glyph_w: int, glyph_h: int) -> list:
    """
    Rend le caractère 'code' dans une grille glyph_w × glyph_h.

    Pour 8×N (largeur ≤ 8) : retourne une liste de glyph_h octets (unsigned char).
    Pour 16×N              : retourne une liste de glyph_h unsigned int (16 bits).

    Stratégie de placement :
      - Calé sur la baseline (descender en bas)
      - Centré horizontalement
      - Tronqué si le bitmap déborde
    """
    rows = [0] * glyph_h

    try:
        face.load_char(code, freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_MONO)
    except freetype.FT_Exception:
        return rows

    bm  = face.glyph.bitmap
    bl  = face.glyph.bitmap_left
    bt  = face.glyph.bitmap_top

    if bm.width == 0 or bm.rows == 0:
        return rows  # espace ou glyphe vide

    buf   = bytes(bm.buffer)
    pitch = bm.pitch

    desc_abs = -(face.size.descender >> 6)
    baseline = glyph_h - desc_abs

    advance = face.glyph.advance.x >> 6
    cell_w  = min(advance, glyph_w)
    x_off   = bl + (glyph_w - cell_w) // 2

    for src_row in range(bm.rows):
        dst_row = baseline - bt + src_row
        if dst_row < 0 or dst_row >= glyph_h:
            continue

        for src_col in range(bm.width):
            dst_col = x_off + src_col
            if dst_col < 0 or dst_col >= glyph_w:
                continue

            byte_idx = src_row * pitch + src_col // 8
            bit_idx  = 7 - (src_col % 8)
            pixel    = (buf[byte_idx] >> bit_idx) & 1

            if pixel:
                if glyph_w <= 8:
                    # bit 7 = pixel le plus à gauche (format PSF / font1DefineChar8x*)
                    rows[dst_row] |= (0x80 >> dst_col)
                else:
                    # bit 15 = pixel le plus à gauche (format font1DefineChar16x16)
                    rows[dst_row] |= (0x8000 >> dst_col)

    return rows


# ==============================================================================
# Glyphes de commentaire selon l'encodage
# ==============================================================================

# Caractères de contrôle CP437 (0x00-0x1F et 0x7F) : communs à CP437 et CP850
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
               'unicode' → caractère Unicode direct (chr(i))
    """
    if encoding == 'unicode':
        import unicodedata
        try:
            ch = chr(i)
            # Catégories non imprimables : Cc (contrôle), Cs (surrogate), Co (usage privé), Cn (non assigné)
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
# Génération du bloc C
# ==============================================================================

def generate_c_block(face, char_range, glyph_w: int, glyph_h: int,
                     array_name: str, font_file: str, preview: bool,
                     encoding: str = 'cp850') -> str:
    """
    Génère un fichier C autonome f1_WxH.c contenant _initFont1_WxH().
    Format identique à psf2c.py (macro font1DefineChar*).
    """
    ttf_basename = os.path.splitext(os.path.basename(font_file))[0]
    family = face.family_name.decode() if face.family_name else ttf_basename
    style  = face.style_name.decode()  if face.style_name  else ""

    lines = []
    lines.append('#include "../font1.h"')
    lines.append("")
    lines.append("/* =========================================================")
    lines.append(f"   Police : {family} {style} — {ttf_basename}.ttf")
    lines.append(f"   Taille : {glyph_w}×{glyph_h} pixels")
    lines.append(f"   Généré par ttf2c.py")
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

    # Pour 16×16, on utilise un tableau r[] intermédiaire (comme ttf_to_font1_16x16.py)
    need_r = (glyph_w > 8)
    if need_r:
        lines.append("    unsigned int r[%d];" % glyph_h)
        lines.append("")

    char_list = list(char_range)
    num_width = len(str(max(char_list) if char_list else 0))

    for code in char_list:
        rows = render_char(face, code, glyph_w, glyph_h)

        if preview:
            _ascii_preview(code, rows, glyph_w, glyph_h, encoding)

        label = get_char_label(code, encoding)

        if glyph_w <= 8:
            # Format : font1DefineChar8x8 / font1DefineChar8x16
            hex_bytes = ", ".join(f"0x{b:02x}" for b in rows)
            raw  = f"    font1DefineChar{glyph_w}x{glyph_h}({array_name}, {code:{num_width}}, {hex_bytes})"
            comment = f"; /* 0x{code:02X} */    /* {label} */"
            lines.append(raw + comment)
        else:
            # Format 16×16 : on remplit r[] puis on appelle font1DefineChar16x16
            lines.append(f"    /* --- 0x{code:02X} --- */")
            # 4 valeurs par ligne, comme dans ttf_to_font1_16x16.py
            per_line = 4
            for start in range(0, glyph_h, per_line):
                chunk = rows[start:start + per_line]
                parts = "; ".join(
                    f"r[{start+i:2d}]=0x{v:04X}" for i, v in enumerate(chunk)
                )
                lines.append(f"    {parts};")
            lines.append(
                f"    font1DefineChar{glyph_w}x{glyph_h}({array_name}, {code:{num_width}}, r); "
                f"/* 0x{code:02X} */ /* {label} */"
            )
            lines.append("")

    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def _ascii_preview(code: int, rows: list, glyph_w: int, glyph_h: int,
                   encoding: str = 'cp850') -> None:
    label = get_char_label(code, encoding)
    msb = 0x8000 if glyph_w > 8 else 0x80
    print(f"  Char 0x{code:02X} ({label!r}):")
    for r in rows:
        line = "".join("X" if r & (msb >> c) else "." for c in range(glyph_w))
        print("    " + line)
    print()


# ==============================================================================
# Génération du PNG (style psf2c.py)
# ==============================================================================

def generate_png(face, char_range, glyph_w: int, glyph_h: int,
                 output_path: str, font_file: str) -> None:
    """Génère un PNG de prévisualisation identique au style de psf2c.py."""
    char_list = list(char_range)
    n_chars = len(char_list)

    cols    = 16
    rows_nb = (n_chars + cols - 1) // cols

    padding  = 4
    label_h  = 10
    margin   = 20
    scale    = 8
    title_h  = 20

    cell_w = glyph_w * scale + padding
    cell_h = glyph_h * scale + padding + label_h

    img_w = cols * cell_w + 2 * margin
    img_h = rows_nb * cell_h + 2 * margin + title_h

    img  = Image.new("RGB", (img_w, img_h), color=(0, 255, 255))
    draw = ImageDraw.Draw(img)

    ttf_basename = os.path.splitext(os.path.basename(font_file))[0]
    family = face.family_name.decode() if face.family_name else ttf_basename
    style  = face.style_name.decode()  if face.style_name  else ""

    title = (f"Font : {family} {style} ({ttf_basename}.ttf)  —  "
             f"Size : {glyph_w}×{glyph_h} px  —  "
             f"Glyphs : {n_chars}  —  "
             f"Zoom : {scale}x")
    draw.text((margin, (title_h - 10) // 2), title, fill=(0, 0, 0))

    msb = 0x8000 if glyph_w > 8 else 0x80

    for col_idx, code in enumerate(char_list):
        col = col_idx % cols
        row = col_idx // cols
        x0  = margin + col * cell_w
        y0  = margin + row * cell_h + title_h

        draw.rectangle(
            [x0, y0, x0 + glyph_w * scale - 1, y0 + glyph_h * scale - 1],
            fill=(0, 0, 0)
        )

        rows_data = render_char(face, code, glyph_w, glyph_h)

        for y in range(glyph_h):
            for x in range(glyph_w):
                if rows_data[y] & (msb >> x):
                    px = x0 + x * scale
                    py = y0 + y * scale
                    draw.rectangle(
                        [px, py, px + scale - 1, py + scale - 1],
                        fill=(255, 255, 255)
                    )

        label = f"{code:02X}"
        draw.text((x0, y0 + glyph_h * scale), label, fill=(0, 0, 0))

    img.save(output_path)
    print(f"  PNG : {output_path}")


# ==============================================================================
# Traitement principal
# ==============================================================================

def process(ttf_path: str, glyph_w: int, glyph_h: int,
            out_base: str, out_dir: str, char_range, preview: bool,
            encoding: str = 'cp850') -> None:

    face = freetype.Face(ttf_path)
    face.set_pixel_sizes(0, glyph_h)

    family = face.family_name.decode() if face.family_name else "?"
    style  = face.style_name.decode()  if face.style_name  else "?"
    print(f"Police    : {family} / {style}")
    print(f"Taille    : {glyph_w}×{glyph_h} px")
    print(f"Ascender  : {face.size.ascender  >> 6} px")
    print(f"Descender : {face.size.descender >> 6} px")
    print(f"Encodage  : {encoding.upper()}")

    array_name = f"font1Bank{glyph_w}x{glyph_h}"
    c_file   = os.path.join(out_dir, f"f1_{glyph_w}x{glyph_h}.c")
    png_file = os.path.join(out_dir, f"{out_base}_{glyph_w}x{glyph_h}.png")

    char_list = list(char_range)
    print(f"Glyphes   : {len(char_list)}  →  {c_file}")

    # --- Fichier .c ---
    c_code = generate_c_block(face, char_list, glyph_w, glyph_h,
                               array_name, ttf_path, preview, encoding)
    with open(c_file, "w", encoding="utf-8") as f:
        f.write(c_code)
    print(f"  .c  : {c_file}")

    # --- Fichier .png ---
    generate_png(face, char_list, glyph_w, glyph_h, png_file, ttf_path)

    print()
    print("Intégration :")
    print(f"  1. Ajouter {c_file} au projet (compiler avec les autres .c).")
    print(f"  2. Vérifier que _initFont1_{glyph_w}x{glyph_h}() est déclarée dans font1.h.")
    print(f"  3. S'assurer que font1InitBank{glyph_w}x{glyph_h}() l'appelle dans font1.c.")


# ==============================================================================
# Collecte des fichiers TTF à traiter (fichier, glob, répertoire)
# ==============================================================================

import glob as _glob

TTF_SUFFIXES = ('.ttf', '.TTF')

def is_ttf(path: str) -> bool:
    return any(path.endswith(s) for s in TTF_SUFFIXES)

def collect_files(args_list: list) -> list:
    """
    Accepte un mélange de :
      - fichiers .ttf explicites
      - globs (*.ttf, chemin/vers/*.ttf) — expandés par le shell ou non
      - répertoires → tous les .ttf à la racine du répertoire
    Retourne la liste dédoublonnée en conservant l'ordre.
    """
    files = []
    for arg in args_list:
        if os.path.isdir(arg):
            # Répertoire : scanner tous les .ttf dedans
            for suffix in TTF_SUFFIXES:
                files += sorted(_glob.glob(os.path.join(arg, f'*{suffix}')))
        elif '*' in arg or '?' in arg or '[' in arg:
            # Glob explicite (non expandé par le shell, ex: Windows)
            expanded = sorted(_glob.glob(arg))
            if expanded:
                files += [f for f in expanded if is_ttf(f)]
            else:
                print(f"Ignoré (aucun fichier correspondant) : {arg}", file=sys.stderr)
        elif is_ttf(arg):
            if os.path.isfile(arg):
                files.append(arg)
            else:
                print(f"Ignoré (introuvable) : {arg}", file=sys.stderr)
        else:
            print(f"Ignoré (extension inconnue) : {arg}", file=sys.stderr)

    # Dédoublonner en conservant l'ordre
    seen = set()
    return [f for f in files if not (f in seen or seen.add(f))]


# ==============================================================================
# Point d'entrée
# ==============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Convertit un ou plusieurs TTF en données C + PNG style psf2c.py.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Exemples :
  python3 ttf2c.py Mecha.ttf                       # 8×8 par défaut
  python3 ttf2c.py *.ttf                           # tous les TTF du répertoire courant
  python3 ttf2c.py OUTILS/ttf/                     # tous les TTF d'un répertoire
  python3 ttf2c.py Mecha.ttf KiwiSoda.ttf -s 16x16 # plusieurs fichiers en 16×16
  python3 ttf2c.py Mecha.ttf -s 8x16 -o Mecha      # base de nom explicite (1 fichier)
  python3 ttf2c.py Mecha.ttf --range 20-7E          # ASCII imprimable seulement
  python3 ttf2c.py Mecha.ttf --chars "AaBbCc"       # caractères spécifiques
  python3 ttf2c.py Mecha.ttf --preview              # aperçu ASCII terminal
  python3 ttf2c.py Mecha.ttf --encoding cp437        # commentaires en CP437
  python3 ttf2c.py Mecha.ttf --encoding unicode      # commentaires U+XXXX NOM
"""
    )
    parser.add_argument("sources", nargs="+",
        metavar="TTF_ou_REPERTOIRE",
        help="Fichier(s) .ttf, glob (*.ttf) ou répertoire(s) à scanner")
    parser.add_argument("-s", "--size", default="8x8",
        metavar="WxH",
        help="Taille cible des glyphes, ex. 8x8, 8x16, 16x16 (défaut : 8x8)")
    parser.add_argument("-o", "--output", default=None,
        metavar="BASE",
        help="Répertoire de sortie pour f1_WxH.c et le PNG. "
             "Créé automatiquement s'il n'existe pas.")
    parser.add_argument("--range", default=None,
        metavar="LO-HI",
        help="Plage de codes caractère en hex (ex. 20-7E). Défaut : 00-FF")
    parser.add_argument("--chars", default=None,
        metavar="STRING",
        help="Convertir uniquement les caractères de cette chaîne (ex. 'ABCabc')")
    parser.add_argument("--preview", action="store_true",
        help="Affiche un aperçu ASCII de chaque glyphe dans le terminal")
    parser.add_argument("--encoding", default="cp850",
        choices=["cp850", "cp437", "unicode"],
        metavar="ENC",
        help="Encodage utilisé pour les commentaires de caractères dans le .c généré. "
             "Valeurs : cp850 (défaut), cp437, unicode. "
             "Exemple : --encoding cp437")

    args = parser.parse_args()

    # --- Collecte des fichiers ---
    files = collect_files(args.sources)
    if not files:
        print("Aucun fichier .ttf trouvé.", file=sys.stderr)
        sys.exit(1)

    # --- Parsing de la taille ---
    try:
        w_str, h_str = args.size.lower().split("x")
        glyph_w = int(w_str)
        glyph_h = int(h_str)
        assert glyph_w > 0 and glyph_h > 0
    except Exception:
        print(f"Erreur : format de taille invalide '{args.size}'. Exemples : 8x8, 8x16, 16x16",
              file=sys.stderr)
        sys.exit(1)

    if glyph_w > 16:
        print(f"Attention : largeur {glyph_w} > 16 px — la macro font1DefineChar utilise "
              f"des unsigned int (16 bits). Vérifiez la compatibilité.", file=sys.stderr)

    # --- Plage de caractères ---
    if args.chars:
        char_range = sorted(set(ord(c) for c in args.chars))
    elif args.range:
        try:
            lo_s, hi_s = args.range.split("-")
            lo = int(lo_s, 16)
            hi = int(hi_s, 16)
            assert lo <= hi
            char_range = range(lo, hi + 1)
        except Exception:
            print(f"Erreur : plage invalide '{args.range}'. Format attendu : 20-7E",
                  file=sys.stderr)
            sys.exit(1)
    else:
        char_range = range(0, 256)

    # --- Traitement ---
    ok = err = 0
    for ttf_path in files:
        ttf_base = os.path.splitext(os.path.basename(ttf_path))[0]
        if args.output:
            out_dir = args.output
            os.makedirs(out_dir, exist_ok=True)
        else:
            out_dir = os.path.dirname(os.path.abspath(__file__))

        print(f"\n→ {ttf_path}")
        try:
            process(ttf_path, glyph_w, glyph_h, ttf_base, out_dir, char_range, args.preview,
                    args.encoding)
            ok += 1
        except FileNotFoundError:
            print(f"  Erreur : fichier introuvable", file=sys.stderr)
            err += 1
        except Exception as e:
            print(f"  Erreur : {e}", file=sys.stderr)
            err += 1

    print(f"\n{ok} police(s) traitée(s)", end="")
    if err:
        print(f", {err} erreur(s).")
    else:
        print(".")


if __name__ == "__main__":
    main()
