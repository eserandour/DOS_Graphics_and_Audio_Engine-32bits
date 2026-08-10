#!/usr/bin/env python3
"""
gen_palettes.py — Génère automatiquement toutes les palettes procédurales
                   du projet, plus leurs images de prévisualisation.

Aucun nom de palette à taper à la main : ce script fait tout seul.

Comment ça marche
------------------
  1. Il lit palette.h pour récupérer la définition de la structure Color.
  2. Il lit palette.c et repère automatiquement toute fonction de la
     forme :
         void build<Nom>Palette(Color *pal)
     Aucune liste à tenir à jour : le jour où quelqu'un ajoute une
     nouvelle fonction build...Palette() dans palette.c, elle est
     détectée toute seule au prochain lancement.
  3. Il compile ces fonctions dans un petit programme C natif
     temporaire (via gcc) et les EXÉCUTE réellement : ce n'est pas
     une réimplémentation en Python, c'est le vrai code de
     palette.c qui tourne pour produire les couleurs.
  4. Il écrit chaque palette dans images/<nom>.pal (768 octets bruts,
     256 x R/G/B sur 6 bits — format standard du projet).
  5. Il génère l'image de prévisualisation images/<nom>_palette.png,
     avec le même rendu que vgatool.py (grille 16x16 étiquetée).
  6. Il nettoie les fichiers temporaires.

DEFAULT.PAL n'est PAS concernée par ce script : elle reste générée
par DUMPPAL.C, qui la lit directement sur le DAC VGA (ce n'est pas
une fonction build...Palette() de palette.c, donc il n'y a rien à
"découvrir" dans le code source pour elle).

Prérequis : un compilateur C natif (gcc/cc) disponible sur la
machine qui lance le script (PAS Open Watcom — celui-ci compile un
programme hôte temporaire, pas un exécutable DOS).

Usage :
    python3 gen_palettes.py

À lancer depuis OUTILS/.
"""

import os
import re
import shutil
import subprocess
import sys
import tempfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PALETTE_H = os.path.join(SCRIPT_DIR, "..", "palette.h")
PALETTE_C = os.path.join(SCRIPT_DIR, "..", "palette.c")
IMAGES_DIR = os.path.join(SCRIPT_DIR, "..", "images")

FUNC_PATTERN = re.compile(
    r"void\s+build(\w+)Palette\s*\(\s*Color\s*\*\s*pal\s*\)\s*"
)
STRUCT_PATTERN = re.compile(
    r"typedef\s+struct\s*\{.*?\}\s*Color\s*;", re.DOTALL
)


def read(path):
    if not os.path.isfile(path):
        print(f"ERREUR : {path} introuvable. "
              f"Lancer ce script depuis le dossier OUTILS.")
        sys.exit(1)
    with open(path, "r", encoding="utf-8") as f:
        return f.read()


def extract_color_struct(header_src):
    m = STRUCT_PATTERN.search(header_src)
    if not m:
        print("ERREUR : structure 'Color' introuvable dans palette.h.")
        sys.exit(1)
    return m.group(0)


def extract_function_body(source, start_of_signature):
    """À partir de l'index où commence 'void build...Palette(...)',
    retrouve l'accolade ouvrante puis la fermante correspondante
    (comptage d'accolades, gère l'imbrication) et renvoie le texte
    complet de la fonction, signature comprise."""
    brace_open = source.index("{", start_of_signature)
    depth = 0
    i = brace_open
    while i < len(source):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[start_of_signature:i + 1]
        i += 1
    raise ValueError("Accolade fermante introuvable (fichier corrompu ?).")


def discover_generators(source):
    """Renvoie une liste de (nom_palette, corps_de_fonction_complet)."""
    generators = []
    for m in FUNC_PATTERN.finditer(source):
        name = m.group(1)              # ex: "Gray"
        body = extract_function_body(source, m.start())
        generators.append((name, body))
    return generators


def build_harness(color_struct, generators, out_dir):
    """Construit le source C du programme hôte temporaire qui
    appelle chaque fonction découverte et écrit le résultat brut
    dans images/<nom>.pal."""
    calls = []
    for name, _ in generators:
        lname = name.lower()
        pal_path = os.path.join(out_dir, f"{lname}.pal").replace("\\", "/")
        calls.append(f'''
    build{name}Palette(pal);
    f = fopen("{pal_path}", "wb");
    if (!f) {{
        fprintf(stderr, "ERREUR: impossible d'ecrire {lname}.pal\\n");
        ok = 0;
    }} else {{
        for (i = 0; i < 256; i++) {{
            unsigned char rgb[3];
            rgb[0] = pal[i].r; rgb[1] = pal[i].g; rgb[2] = pal[i].b;
            fwrite(rgb, 1, 3, f);
        }}
        fclose(f);
        printf("{lname}.pal ecrit (768 octets)\\n");
    }}''')

    bodies = "\n\n".join(body for _, body in generators)

    return f"""/* Programme hote temporaire, genere et execute par
   gen_palettes.py — appelle le vrai code de palette.c pour produire
   les fichiers .pal. Ce fichier est supprime apres execution. */
#include <stdio.h>

{color_struct}

{bodies}

int main(void)
{{
    Color pal[256];
    FILE *f;
    int i;
    int ok = 1;
{"".join(calls)}
    return ok ? 0 : 1;
}}
"""


def compile_and_run(harness_src):
    compiler = shutil.which("gcc") or shutil.which("cc")
    if not compiler:
        print("ERREUR : aucun compilateur C natif (gcc/cc) trouve sur "
              "cette machine. Ce script doit tourner sur l'hote de "
              "developpement, pas dans FreeDOS.")
        sys.exit(1)

    with tempfile.TemporaryDirectory() as tmp:
        src_path = os.path.join(tmp, "harness.c")
        exe_path = os.path.join(tmp, "harness")
        with open(src_path, "w", encoding="utf-8") as f:
            f.write(harness_src)

        r = subprocess.run([compiler, "-O2", "-o", exe_path, src_path],
                            capture_output=True, text=True)
        if r.returncode != 0:
            print("ERREUR : compilation du harness temporaire a echoue.")
            print(r.stderr)
            sys.exit(1)

        r = subprocess.run([exe_path], capture_output=True, text=True)
        print(r.stdout, end="")
        if r.returncode != 0:
            print(r.stderr, end="")
            sys.exit(1)


def render_previews(names):
    """Réutilise le rendu de vgatool.py (mêmes couleurs de fond,
    grille 16x16, étiquettes) pour rester cohérent avec les images
    déjà présentes dans images/."""
    sys.path.insert(0, SCRIPT_DIR)
    import vgatool  # module local, OUTILS/vgatool.py

    for name in names:
        lname = name.lower()
        pal_path = os.path.join(IMAGES_DIR, f"{lname}.pal")
        png_path = os.path.join(IMAGES_DIR, f"{lname}_palette.png")

        with open(pal_path, "rb") as f:
            raw = f.read()

        colors = [(raw[i * 3] << 2, raw[i * 3 + 1] << 2, raw[i * 3 + 2] << 2)
                  for i in range(256)]

        vgatool._render_palette(f"{lname}.pal", colors, png_path)
        print(f"{lname}_palette.png ecrit")


def main():
    os.makedirs(IMAGES_DIR, exist_ok=True)

    header_src = read(PALETTE_H)
    source_src = read(PALETTE_C)

    color_struct = extract_color_struct(header_src)
    generators = discover_generators(source_src)

    if not generators:
        print("ERREUR : aucune fonction 'void build<Nom>Palette(Color *pal)' "
              "trouvee dans palette.c.")
        sys.exit(1)

    names = [name for name, _ in generators]
    print("Palettes decouvertes : " + ", ".join(f"build{n}Palette" for n in names))
    print()

    harness_src = build_harness(color_struct, generators, IMAGES_DIR)
    compile_and_run(harness_src)

    print()
    render_previews(names)

    print(f"\n{len(names)} palette(s) generee(s) avec succes dans "
          f"{os.path.relpath(IMAGES_DIR, SCRIPT_DIR)}/.")


if __name__ == "__main__":
    main()
