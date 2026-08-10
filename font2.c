/* =========================================================
   FONT2.C — Affichage de texte par feuille de sprites
   =========================================================
   Environnement : Open Watcom 1.9, DOS (modele flat 32 bits, DOS/32A)
   Mode video    : 13h (320x200, 256 couleurs, 1 octet/pixel)

   ARITHMETIQUE DES TAILLES/OFFSETS
   ---------------------------------
   Les tailles/offsets de la feuille (sheet_w * sheet_h, qui peut
   dépasser 32767, ex: 320*192 = 61440) sont calculés en
   unsigned long : sous wcc386, "int" fait déjà 32 bits et ne
   déborderait pas non plus, mais unsigned long documente
   explicitement qu'aucun débordement n'est possible ici, quelle
   que soit la taille de la feuille.

   Charge toute la feuille en RAM une seule fois via font2Load().
   Lecture ligne par ligne (sheet_w octets par appel) via un petit
   buffer intermédiaire (voir GESTION DU BUFFER plus bas).
   Blit direct depuis le buffer : zero acces disque pendant le rendu.
   ========================================================= */

#include <malloc.h>   /* malloc, free                       */
#include <stdio.h>    /* FILE, fopen, fread, fclose          */
#include <string.h>   /* memcpy                              */
#include "video.h"    /* backbuffer, SCREEN_WIDTH, OFFSET   */
#include "font2.h"


/* =========================================================
   UTILITAIRES INTERNES
   ========================================================= */

/* Calcule la position (srcX, srcY) du glyphe c dans la feuille.
   Retourne 1 si le caractere est dans la plage, 0 sinon.     */
static int glyphPos(const Font2Desc *fd, unsigned char c,
                    int *outSrcX, int *outSrcY)
{
    int idx;
    if (c < (unsigned char)fd->first_char ||
        c > (unsigned char)fd->last_char) return 0;
    idx      = c - (unsigned char)fd->first_char;
    *outSrcX = (idx % fd->cols) * fd->char_w;
    *outSrcY = (idx / fd->cols) * fd->char_h;
    return 1;
}

static int f2strlen(const char *s)
{
    int n = 0; while (s[n]) n++; return n;
}


/* =========================================================
   GESTION DU BUFFER
   =========================================================
   Lecture ligne par ligne (sheet_w octets à la fois) via un
   buffer intermédiaire de 320 octets, plutôt qu'un seul gros
   fread() : simple et suffisant, une ligne en mode 13h ne
   dépassant jamais 320 pixels.
   ========================================================= */

int font2Load(Font2Desc *fd)
{
    FILE *f;
    unsigned char rowBuf[320];
    unsigned long row;
    unsigned char *dst;

    if (fd->sheet) return 1;   /* deja en RAM */

    fd->sheet = (unsigned char *)malloc(
        (unsigned long)fd->sheet_w * (unsigned long)fd->sheet_h);
    if (!fd->sheet) return 0;

    f = fopen(fd->path, "rb");
    if (!f) { free(fd->sheet); fd->sheet = NULL; return 0; }

    dst = fd->sheet;
    for (row = 0; row < (unsigned long)fd->sheet_h; row++)
    {
        if (fread(rowBuf, 1, (unsigned)fd->sheet_w, f)
                != (unsigned)fd->sheet_w)
        {
            fclose(f);
            free(fd->sheet);
            fd->sheet = NULL;
            return 0;
        }
        memcpy(dst, rowBuf, (unsigned)fd->sheet_w);
        dst += fd->sheet_w;
    }

    fclose(f);
    return 1;
}

void font2Free(Font2Desc *fd)
{
    if (fd->sheet) { free(fd->sheet); fd->sheet = NULL; }
}

int font2IsLoaded(const Font2Desc *fd)
{
    return fd->sheet != NULL;
}


/* =========================================================
   RENDU
   ========================================================= */

/* Contrairement aux primitives de graphics.c (putPixel, drawLine...),
   ce blit n'effectue AUCUN clipping sur les bords de l'écran : x, y
   et (x + char_w-1, y + char_h-1) doivent rester dans [0, SCREEN_WIDTH)
   x [0, SCREEN_HEIGHT), sous peine d'écrire hors du backbuffer. C'est
   à l'appelant de garantir des coordonnées valides (voir les scènes
   scene4.c/scene5.c pour des exemples de positionnement sûr). */
void font2DrawChar(Font2Desc *fd, unsigned char c,
                   int x, int y)
{
    int srcX, srcY, row, col;
    unsigned char *dst;
    unsigned char pix;
    unsigned char ck;

    if (!glyphPos(fd, c, &srcX, &srcY)) return;

    ck  = (unsigned char)fd->colorKey;
    dst = backbuffer + OFFSET(x, y);

    for (row = 0; row < fd->char_h; row++)
    {
        for (col = 0; col < fd->char_w; col++)
        {
            /* Cast unsigned long : garantit qu'aucun terme de ce
               calcul d'offset ne peut déborder, quelle que soit
               la taille de la feuille. */
            pix = fd->sheet[
                (unsigned long)(srcY + row) * fd->sheet_w
                + (srcX + col)];
            if (fd->colorKey < 0 || pix != ck)
                dst[col] = pix;
        }
        dst += SCREEN_WIDTH;
    }
}

void font2DrawText(Font2Desc *fd, const char *text,
                   int x, int y)
{
    int i;
    for (i = 0; text[i] != '\0'; i++)
        font2DrawChar(fd, (unsigned char)text[i],
                      x + i * fd->char_w, y);
}

void font2DrawTextCentered(Font2Desc *fd, const char *text,
                           int y)
{
    int x = (SCREEN_WIDTH - f2strlen(text) * fd->char_w) / 2;
    if (x < 0) x = 0;
    font2DrawText(fd, text, x, y);
}

unsigned char font2GetPixel(const Font2Desc *fd,
                             int sx, int sy)
{
    return fd->sheet[
        (unsigned long)sy * fd->sheet_w + sx];
}
