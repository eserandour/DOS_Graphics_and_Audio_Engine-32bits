/* =========================================================
   IMAGE.C -- Chargement one-shot d'images RAW/PAL en mode 13h
   =========================================================
   Voir image.h pour la documentation complete de l'API.
   ========================================================= */

#include <stdio.h>    /* FILE, fopen, fread, fclose         */
#include <string.h>   /* memcpy                              */
#include "video.h"    /* backbuffer, OFFSET, SCREEN_*       */
#include "palette.h"  /* loadPalette, PAL_OK                */
#include "image.h"


/* ---------------------------------------------------------
   rawBlit -- helper interne : copie un .raw dans le backbuffer
   ---------------------------------------------------------
   Lit srcW x srcH octets depuis f (deja ouvert, positionne
   au debut des pixels) et les copie dans le backbuffer en
   (dstX, dstY), une ligne a la fois.

   Pourquoi un buffer intermediaire buf[320] ?
   dst pointe directement dans le backbuffer, mais lire chaque
   ligne dans un petit buffer sur la pile avant de la copier
   isole rawBlit() de tout detail sur dst (alignement, stride) :
   fread() ne touche jamais directement au backbuffer.
   buf[320] suffit car une ligne en mode 13h fait 320 pixels
   au maximum.

   Le stride du backbuffer est toujours SCREEN_WIDTH (320),
   meme si srcW < 320 : on avance de 320 octets entre deux
   lignes pour rester aligne sur la grille de l'ecran.

   Le fichier f est ferme par l'appelant.
   Retourne IMG_OK ou IMG_ERR_READ.                          */
static int rawBlit(FILE *f, int srcW, int srcH, int dstX, int dstY)
{
    unsigned char *dst;
    unsigned char buf[320];
    int row;

    dst = backbuffer + OFFSET(dstX, dstY);

    for (row = 0; row < srcH; row++)
    {
        /* Lire une ligne du .raw dans le buffer intermediaire. */
        if (fread(buf, 1, (size_t)srcW, f) != (size_t)srcW)
            return IMG_ERR_READ;

        /* Copier la ligne vers le backbuffer. */
        memcpy(dst, buf, (size_t)srcW);

        /* Passer a la ligne suivante dans le backbuffer.
           Le stride est 320 (largeur de l'ecran en mode 13h),
           pas srcW, car le backbuffer est toujours 320x200. */
        dst += SCREEN_WIDTH;
    }

    return IMG_OK;
}


/* ---------------------------------------------------------
   drawImage
   ---------------------------------------------------------
   Combine le chargement de la palette et le blit du .raw
   en un seul appel.

   Ordre des operations :
     1. loadPalette(palFile) -- lit 768 octets, remplit
        workingPalette, envoie la palette au DAC VGA.
        Le DAC est le circuit analogique du VGA qui convertit
        les index de palette (0-255) en tensions R/G/B pour
        le moniteur. Sans cette etape, les couleurs de l'image
        seraient fausses si la palette active ne correspond
        pas a celle du .raw.
     2. fopen + rawBlit -- lit le .raw ligne par ligne et
        copie chaque ligne dans le backbuffer via memcpy.
     3. fclose.

   L'ecran ne change pas tant que flip() n'est pas appele :
   on dessine toujours dans le backbuffer en RAM, jamais
   directement dans la VRAM.

   Retourne IMG_OK ou un code IMG_ERR_*.                     */
int drawImage(const char *palFile, const char *rawFile,
              int srcW, int srcH, int dstX, int dstY)
{
    FILE *f;
    int r;

    /* Etape 1 : charger la palette et l'envoyer au DAC. */
    r = loadPalette(palFile);
    if (r != PAL_OK) return IMG_ERR_PAL;

    /* Etape 2 : ouvrir le .raw. */
    f = fopen(rawFile, "rb");
    if (!f) return IMG_ERR_RAW;

    /* Etape 3 : copier les pixels dans le backbuffer. */
    r = rawBlit(f, srcW, srcH, dstX, dstY);
    fclose(f);
    return r;
}


/* ---------------------------------------------------------
   drawScreen
   ---------------------------------------------------------
   Raccourci pour le cas le plus courant : image plein ecran
   320x200 positionnee en (0, 0).
   Appelle simplement drawImage avec les dimensions de l'ecran
   et l'origine comme coin superieur gauche.
   Retourne IMG_OK ou un code IMG_ERR_*.                     */
int drawScreen(const char *palFile, const char *rawFile)
{
    return drawImage(palFile, rawFile, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0);
}
