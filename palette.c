/* =========================================================
   PALETTE.C — Gestion de la palette VGA 256 couleurs
   ========================================================= */

#include <stdio.h>    /* FILE, fopen, fread, fwrite, fclose */
#include <string.h>   /* memcpy, memset                     */
#include <conio.h>    /* outp, inp                          */
#include "palette.h"
#include "video.h"    /* waitVRetrace                       */

/* =========================================================
   PALETTES GLOBALES
   =========================================================
   Définies ici (une seule fois), déclarées extern dans
   palette.h pour être accessibles depuis les autres modules.
   ========================================================= */

Color defaultPalette[256];   /* palette BIOS au démarrage   */
Color workingPalette[256];   /* palette de travail courante */
Color paletteA[256];         /* palette source pour lerp    */
Color paletteB[256];         /* palette cible pour lerp     */
Color grayPalette[256];      /* dégradé noir → blanc        */
Color redPalette[256];       /* noir + rouge → blanc        */
Color bluePalette[256];      /* noir + bleu → blanc         */
Color greenPalette[256];     /* noir + vert → blanc         */
Color rainbowPalette[256];   /* cercle chromatique HSV 360° */

/* =========================================================
   FICHIER .PAL
   ========================================================= */

/* Charge un fichier .pal (768 octets : 256 × R/G/B sur 6 bits)
   dans workingPalette et envoie immédiatement la palette
   au DAC VGA.

   Format .pal : 256 entrées × 3 octets (R, G, B).
   Chaque composante est sur 6 bits (0-63), format natif
   du DAC VGA (pas de conversion nécessaire).

   Pourquoi un buffer intermédiaire ?
   La structure Color peut contenir du padding selon
   l'alignement choisi par le compilateur. On lit dans un
   tableau d'octets contigu garantissant l'absence de trous,
   puis on affecte champ par champ dans la structure.

   Retourne PAL_OK, PAL_ERR_FILE ou PAL_ERR_READ. */
int loadPalette(const char *palFile)
{
    FILE *f;
    unsigned char buf[768];
    int i;

    f = fopen(palFile, "rb");
    if (!f) return PAL_ERR_FILE;

    if (fread(buf, 1, 768, f) != 768)
    {
        fclose(f);
        return PAL_ERR_READ;
    }

    fclose(f);

    for (i = 0; i < 256; i++)
    {
        workingPalette[i].r = buf[i * 3];
        workingPalette[i].g = buf[i * 3 + 1];
        workingPalette[i].b = buf[i * 3 + 2];
    }

    setPalette(workingPalette);
    return PAL_OK;
}

/* Écrit 256 triplets R/G/B (6 bits) dans un fichier binaire.
   Format identique à celui des .pal du projet : 768 octets
   bruts, sans en-tête, compatibles avec vgatool.py.
   Retourne 1 si succès, 0 si échec (fopen ou fwrite). */
int savePalette(const Color *pal, const char *filename)
{
    FILE *f;
    int i;
    unsigned char buf[3];

    f = fopen(filename, "wb");
    if (!f) return 0;

    for (i = 0; i < 256; i++)
    {
        buf[0] = pal[i].r;
        buf[1] = pal[i].g;
        buf[2] = pal[i].b;
        if (fwrite(buf, 1, 3, f) != 3)
        {
            fclose(f);
            return 0;
        }
    }

    fclose(f);
    return 1;
}

/* =========================================================
   ACCÈS MATÉRIEL DAC VGA
   =========================================================
   Le DAC VGA s'accède via trois ports :
     0x3C7 : port de lecture (sélectionne l'index à lire)
     0x3C8 : port d'écriture (sélectionne l'index à écrire)
     0x3C9 : port de données (R puis G puis B, 6 bits chacun)
   Après chaque triplet R/G/B, l'index s'incrémente
   automatiquement, ce qui permet d'écrire tous les
   256 triplets en séquence sans ressélectionner l'index.
   ========================================================= */

/* Définit une couleur individuelle dans le DAC.
   Utile pour changer une seule couleur sans tout réécrire. */
void setPaletteColor(unsigned char index,
                     unsigned char r, unsigned char g, unsigned char b)
{
    outp(0x3C8, index);   /* sélectionner l'index à écrire */
    outp(0x3C9, r);       /* composante rouge (0-63)       */
    outp(0x3C9, g);       /* composante verte (0-63)       */
    outp(0x3C9, b);       /* composante bleue (0-63)       */
}

/* Envoie les 256 couleurs d'une palette vers le DAC.
   On attend le retrace vertical avant d'écrire pour éviter
   le "palette tearing" : des bandes visibles à l'écran
   causées par un changement de palette en cours d'affichage.
   L'index s'incrémente automatiquement après chaque octet,
   donc on n'écrit 0x3C8 qu'une seule fois pour les 256. */
void setPalette(Color *pal)
{
    int i;

    waitVRetrace();      /* attendre le retrace vertical    */
    outp(0x3C8, 0);     /* commencer à l'index 0           */
    for (i = 0; i < 256; i++)
    {
        outp(0x3C9, pal[i].r);   /* rouge  */
        outp(0x3C9, pal[i].g);   /* vert   */
        outp(0x3C9, pal[i].b);   /* bleu   */
        /* l'index DAC passe automatiquement à i+1 */
    }
}

/* Lit les 256 couleurs actuelles du DAC VGA.
   Port 0x3C7 : sélection de l'index en lecture.
   Après chaque triplet lu sur 0x3C9, l'index s'incrémente
   automatiquement (même mécanisme qu'en écriture). */
void getPalette(Color *pal)
{
    int i;
    for (i = 0; i < 256; i++)
    {
        outp(0x3C7, i);           /* sélectionner l'index  */
        pal[i].r = inp(0x3C9);   /* lire rouge            */
        pal[i].g = inp(0x3C9);   /* lire vert             */
        pal[i].b = inp(0x3C9);   /* lire bleu             */
    }
}

/* =========================================================
   MANIPULATION DE PALETTE
   ========================================================= */

/* Copie une palette complète (256 * 3 octets = 768 octets).
   memcpy est plus rapide qu'une boucle manuelle. */
void copyPalette(Color *dest, Color *src)
{
    memcpy(dest, src, 256 * sizeof(Color));
}

/* Interpolation linéaire entre deux palettes.
   Pour chaque composante : dest = palA + t * (palB - palA)
     t = 0.0 → dest = palA (100% source)
     t = 0.5 → dest = mélange à 50%
     t = 1.0 → dest = palB (100% cible)

   Pourquoi le cast (int) avant la soustraction ?
   Les composantes sont unsigned char (0-63). En C, un
   unsigned char est automatiquement promu en int (signé) dans
   une expression arithmétique — pas en unsigned int — car int
   peut représenter toutes les valeurs d'un unsigned char : la
   soustraction palB[i].r - palA[i].r est donc déjà correctement
   signée même sans cast explicite (contrairement à une
   soustraction entre deux unsigned int/unsigned long, qui,
   elle, "enroulerait" bel et bien en cas de résultat négatif,
   ex : 10u - 20u devenant un grand nombre positif). Le cast
   (int) ici est donc redondant avec les règles de promotion du
   C, mais conservé à dessein : il documente explicitement
   l'intention (arithmétique signée) sans dépendre du lecteur
   pour connaître ces règles de promotion implicite. */
void lerpPalette(Color *dest, Color *palA, Color *palB, float t)
{
    int i;
    for (i = 0; i < 256; i++)
    {
        dest[i].r = (unsigned char)(palA[i].r + t * (int)(palB[i].r - palA[i].r));
        dest[i].g = (unsigned char)(palA[i].g + t * (int)(palB[i].g - palA[i].g));
        dest[i].b = (unsigned char)(palA[i].b + t * (int)(palB[i].b - palA[i].b));
    }
}

/* Applique un facteur de luminosité et envoie au DAC.
   Chaque composante est multipliée par t (0.0 à 1.0).
   Ne modifie pas pal en mémoire : les valeurs originales
   sont préservées pour pouvoir refaire le calcul à chaque
   frame (fade progressif). */
void fadePalette(Color *pal, float t)
{
    int i;
    waitVRetrace();
    outp(0x3C8, 0);
    for (i = 0; i < 256; i++)
    {
        outp(0x3C9, (unsigned char)(pal[i].r * t));
        outp(0x3C9, (unsigned char)(pal[i].g * t));
        outp(0x3C9, (unsigned char)(pal[i].b * t));
    }
}

/* Décale toutes les couleurs d'un cran vers la gauche.
   Sauvegarde pal[start], décale pal[start+1..end] d'un
   cran, remet la sauvegarde en pal[end].
   Effet visuel : les couleurs "tournent" vers la gauche.
   Appelle setPalette() pour appliquer immédiatement. */
void cyclePaletteLeft(Color *pal, int start, int end)
{
    int i;
    Color tmp = pal[start];          /* sauvegarder le premier */
    for (i = start; i < end; i++)
        pal[i] = pal[i + 1];         /* décaler vers la gauche */
    pal[end] = tmp;                  /* remettre en fin        */
    setPalette(pal);
}

/* Décale toutes les couleurs d'un cran vers la droite.
   Sauvegarde pal[end], décale pal[start..end-1] d'un
   cran vers la droite (chaque entrée prend la valeur de
   son voisin de gauche), puis remet la sauvegarde en pal[start].
   Effet visuel : les couleurs "tournent" vers la droite. */
void cyclePaletteRight(Color *pal, int start, int end)
{
    int i;
    Color tmp = pal[end];            /* sauvegarder le dernier */
    for (i = end; i > start; i--)
        pal[i] = pal[i - 1];         /* décaler vers la droite */
    pal[start] = tmp;                /* remettre en tête       */
    setPalette(pal);
}

/* =========================================================
   GÉNÉRATEURS DE PALETTE
   ========================================================= */

/* Génère un dégradé linéaire du noir au blanc.
   Les composantes VGA sont sur 6 bits (0-63).
   index / 4 = index >> 2 convertit 0-255 en 0-63. */
void buildGrayPalette(Color *pal)
{
    int i;
    unsigned char v;
    for (i = 0; i < 256; i++)
    {
        v = (unsigned char)(i >> 2);  /* 0-255 → 0-63 */
        pal[i].r = v;
        pal[i].g = v;
        pal[i].b = v;
    }
}

/* Génère la palette rouge :
   - index 0 : noir total (fond d'écran)
   - index 1-255 : rouge fixe à 63 (maximum), vert et bleu
     qui montent linéairement de 0 à 63.
   Résultat visuel : du rouge vif vers le blanc en passant
   par le rose. Formule : (i * 63) / 255 = i / 4.04... */
void buildRedPalette(Color *pal)
{
    int i;
    pal[0].r = 0; pal[0].g = 0; pal[0].b = 0;   /* noir */
    for (i = 1; i < 256; i++)
    {
        pal[i].r = 63;                                  /* rouge max        */
        pal[i].g = (unsigned char)((i * 63) / 255);    /* 0 → 63 linéaire */
        pal[i].b = (unsigned char)((i * 63) / 255);    /* 0 → 63 linéaire */
    }
}

/* Génère la palette bleue :
   - index 0 : noir total (fond d'écran)
   - index 1-255 : bleu fixe à 63 (maximum), rouge et vert
     qui montent linéairement de 0 à 63.
   Résultat visuel : du bleu vif vers le blanc en passant
   par le bleu ciel. Formule : (i * 63) / 255 = i / 4.04... */
void buildBluePalette(Color *pal)
{
    int i;
    pal[0].r = 0; pal[0].g = 0; pal[0].b = 0;   /* noir */
    for (i = 1; i < 256; i++)
    {
        pal[i].r = (unsigned char)((i * 63) / 255);    /* 0 → 63 linéaire */
        pal[i].g = (unsigned char)((i * 63) / 255);    /* 0 → 63 linéaire */
        pal[i].b = 63;                                  /* bleu max         */
    }
}

/* Génère la palette verte :
   - index 0 : noir total (fond d'écran)
   - index 1-255 : vert fixe à 63 (maximum), rouge et bleu
     qui montent linéairement de 0 à 63.
   Résultat visuel : du vert vif vers le blanc en passant
   par le vert pâle. Formule : (i * 63) / 255 = i / 4.04... */
void buildGreenPalette(Color *pal)
{
    int i;
    pal[0].r = 0; pal[0].g = 0; pal[0].b = 0;   /* noir */
    for (i = 1; i < 256; i++)
    {
        pal[i].r = (unsigned char)((i * 63) / 255);    /* 0 → 63 linéaire */
        pal[i].g = 63;                                  /* vert max         */
        pal[i].b = (unsigned char)((i * 63) / 255);    /* 0 → 63 linéaire */
    }
}

/* Génère un cercle chromatique HSV complet sur 360° :
   - index 0   : noir (fond d'écran)
   - index 1-255 : teinte HSV à saturation=1, valeur=1.
   La teinte tourne de 0° (rouge) à 360° (rouge) de façon
   continue, ce qui permet les effets de cycle de palette
   sans saut de couleur entre les index 255 et 1.
   Conversion HSV → RGB par sextants de 60°. */
void buildRainbowPalette(Color *pal)
{
    int i, hi;
    float h, f, r, g, b;

    pal[0].r = 0; pal[0].g = 0; pal[0].b = 0;   /* noir */

    for (i = 1; i < 256; i++)
    {
        h  = (float)(i - 1) / 255.0f * 360.0f;  /* 0° → 360° */
        hi = (int)(h / 60.0f) % 6;
        f  = h / 60.0f - (int)(h / 60.0f);

        switch (hi)
        {
            case 0: r = 1.0f;   g = f;      b = 0.0f;   break;
            case 1: r = 1.0f-f; g = 1.0f;   b = 0.0f;   break;
            case 2: r = 0.0f;   g = 1.0f;   b = f;      break;
            case 3: r = 0.0f;   g = 1.0f-f; b = 1.0f;   break;
            case 4: r = f;      g = 0.0f;   b = 1.0f;   break;
            default:r = 1.0f;   g = 0.0f;   b = 1.0f-f; break;
        }

        pal[i].r = (unsigned char)(r * 63.0f);
        pal[i].g = (unsigned char)(g * 63.0f);
        pal[i].b = (unsigned char)(b * 63.0f);
    }
}
