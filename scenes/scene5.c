/* =========================================================
   SCENE5.C — Scrolling horizontal puis scrolling vertical
   =========================================================
   Environnement : Open Watcom 1.9, DOS
   Mode video    : 13h (320x200, 256 couleurs)

   La scène se déroule en deux parties enchaînées :

   PARTIE A — Scroller horizontal (credits)
   -----------------------------------------
   Texte long qui défile de droite à gauche, centré
   verticalement, encadré de deux barres horizontales.
   Algorithme colonne par colonne :
     posX      = scrollX + c
     charIdx   = (posX / char_w) % textLen
     pixInChar = posX % char_w
     -> blit de la colonne pixInChar du glyphe charIdx
   Fin : après un passage complet du ruban.

   PARTIE B — Scroller vertical (Star Wars / générique)
   ------------------------------------------------------
   Plusieurs lignes de texte défilent du bas vers le haut.
   La fenêtre de rendu est centrée horizontalement.
   Algorithme ligne par ligne :
     posY      = scrollY + r             (r = ligne écran)
     lineIdx   = posY / char_h           (index de ligne)
     pixInLine = posY % char_h           (pixel dans le glyphe)
     -> blit de la ligne pixInLine de la chaîne lineIdx
   scrollY avance d'un pixel à la fois (VSCROLL_SPEED_MS).
   Fin : quand la dernière ligne a entièrement quitté l'écran.

   TRANSITION A→B
   --------------
   Fondu sortant (fade-out) en FADE_MS ms, puis clearScreen
   et démarrage de la partie B.

   NOTE C89 (Open Watcom)
   ----------------------
   Toutes les déclarations en tête de bloc ou au niveau fichier.
   ========================================================= */

#include "video.h"
#include "palette.h"
#include "timer.h"
#include "graphics.h"
#include "font2.h"
#include "scene.h"
#include "app.h"
#include <conio.h>   /* outp */

/* =========================================================
   TEXTES
   ========================================================= */

/* Scroller horizontal */
#define HSCROLL_TEXT \
    "     DEMO DOS     MODE 13H     OPEN WATCOM 1.9     "

/* Lignes du générique vertical.
   Chaîne unique, lignes séparées par '\n'.
   Une chaîne vide ("") insère une ligne blanche. */
static const char *vlines[] = {
    "",
    "DEMO DOS",
    "",
    "MODE 13H",
    "",
    "OPEN WATCOM 1.9",
    "",
};
#define VLINES_COUNT  ((int)(sizeof(vlines) / sizeof(vlines[0])))

/* =========================================================
   PARAMETRES
   ========================================================= */

/* Partie A — horizontal
   On travaille en ticks timer (70 Hz) plutôt qu'en ms pour
   éviter la saccade causée par la division entière :
   à 20 ms/pixel et un tick de ~14 ms, steps alternait
   entre 0 et 1, ce qui donnait un défilement irrégulier.
   1 pixel tous les HSCROLL_TICKS ticks = débit constant.
   HSCROLL_TICKS = 2 → 35 px/s  (défilement lent, lisible)
   HSCROLL_TICKS = 1 → 70 px/s  (défilement rapide)       */
#define HSCROLL_TICKS     2UL
#define SCROLL_BG_COLOR    0
#define SCROLL_BAR_COLOR   8
#define SCROLL_BAR_H       2

/* Transition */
#define FADE_MS           600UL

/* Partie B — vertical */
#define VSCROLL_SPEED_MS  30UL    /* ms par pixel */
#define VSCROLL_BG_COLOR   0
#define VSCROLL_WIN_W    240      /* largeur de la fenêtre texte (px) */

/* =========================================================
   ETAT DE LA MACHINE A ETATS
   =========================================================
   state = 0 : partie A (scroll horizontal)
   state = 1 : fondu sortant A→B
   state = 2 : partie B (scroll vertical)
   ========================================================= */
static int           state       = 0;
static int           initialized = 0;

/* --- Partie A --- */
static Font2Desc     hFont     = FONT2_DESC_16X16_F2;
static long          scrollX   = 0;
static unsigned long lastH     = 0;
static long          rubanW    = 0;
static int           hTextLen  = 0;
static int           textY     = 0;        /* Y de la ligne de texte H */

/* --- Transition --- */
static unsigned long fadeStart = 0;

/* --- Partie B --- */
static Font2Desc     vFont     = FONT2_DESC_16X16_F2;
static long          scrollY   = 0;        /* pixels déjà remontés */
static unsigned long lastV     = 0;
static long          rubanH    = 0;        /* hauteur totale du ruban */
static int           vWinX     = 0;        /* X gauche de la fenêtre  */

/* =========================================================
   UTILITAIRES
   ========================================================= */
static int f2len(const char *s) { int n = 0; while (s[n]) n++; return n; }

/* =========================================================
   FADE-OUT EN VIRGULE FIXE
   =========================================================
   facteur : 64 = pleine luminosité, 0 = noir.
   Directement sur le DAC, sans modifier workingPalette.
   ========================================================= */
static void fadePaletteInt5(Color *pal, unsigned int facteur)
{
    int i;
    waitVRetrace();
    outp(0x3C8, 0);
    for (i = 0; i < 256; i++)
    {
        outp(0x3C9, (unsigned char)((pal[i].r * facteur) >> 6));
        outp(0x3C9, (unsigned char)((pal[i].g * facteur) >> 6));
        outp(0x3C9, (unsigned char)((pal[i].b * facteur) >> 6));
    }
}

/* =========================================================
   PARTIE A — BLIT D'UNE COLONNE (scroll horizontal)
   ========================================================= */
static void blitColumn(int screenCol)
{
    long posX;
    int  charIdx, pixInChar;
    unsigned char c;
    int  glyphIdx, glyphCol, glyphRow, srcX, srcY, row;
    unsigned char *dst;
    unsigned char pix;
    unsigned char ck;

    posX      = scrollX + (long)screenCol;
    charIdx   = (int)((posX / hFont.char_w) % hTextLen);
    pixInChar = (int)(posX % hFont.char_w);

    c   = (unsigned char)HSCROLL_TEXT[charIdx];
    dst = backbuffer + OFFSET(screenCol, textY);
    ck  = (unsigned char)hFont.colorKey;

    if (c < (unsigned char)hFont.first_char ||
        c > (unsigned char)hFont.last_char)
    {
        for (row = 0; row < hFont.char_h; row++)
        {
            *dst = SCROLL_BG_COLOR;
            dst += SCREEN_WIDTH;
        }
        return;
    }

    glyphIdx = c - (unsigned char)hFont.first_char;
    glyphCol = glyphIdx % hFont.cols;
    glyphRow = glyphIdx / hFont.cols;
    srcX     = glyphCol * hFont.char_w + pixInChar;
    srcY     = glyphRow * hFont.char_h;

    for (row = 0; row < hFont.char_h; row++)
    {
        pix  = font2GetPixel(&hFont, srcX, srcY + row);
        *dst = (pix == ck) ? SCROLL_BG_COLOR : pix;
        dst += SCREEN_WIDTH;
    }
}

/* =========================================================
   PARTIE B — BLIT D'UNE LIGNE (scroll vertical)
   =========================================================
   Pour chaque ligne écran r (0..SCREEN_HEIGHT-1) :
     posY      = scrollY + r         (position dans le ruban)
     lineIdx   = posY / char_h       (indice de ligne de texte)
     pixInLine = posY % char_h       (pixel vertical dans le glyphe)

   Pour chaque colonne c de la fenêtre (vWinX..vWinX+VSCROLL_WIN_W-1) :
     - identifier le glyphe à la position c dans la ligne lineIdx
     - lire le pixel (pixInCol, pixInLine) de ce glyphe
     - écrire dans le backbuffer ou fond si transparent
   ========================================================= */
static void blitVLine(int screenRow)
{
    long posY;
    int  lineIdx, pixInLine;
    const char *line;
    int  lineLen;
    int  col;
    unsigned char ck;
    unsigned char *dst;

    posY      = scrollY + (long)screenRow;
    lineIdx   = (int)(posY / vFont.char_h);
    pixInLine = (int)(posY % vFont.char_h);

    if (lineIdx < 0 || lineIdx >= VLINES_COUNT)
    {
        /* Hors ruban : ligne de fond. */
        dst = backbuffer + OFFSET(vWinX, screenRow);
        {
            int c;
            for (c = 0; c < VSCROLL_WIN_W; c++)
            {
                *dst = VSCROLL_BG_COLOR;
                dst++;
            }
        }
        return;
    }

    line    = vlines[lineIdx];
    lineLen = f2len(line);
    ck      = (unsigned char)vFont.colorKey;

    /* Centrage de la chaîne dans la fenêtre. */
    {
        int textPxW  = lineLen * vFont.char_w;
        int textXoff = (VSCROLL_WIN_W - textPxW) / 2;  /* peut être négatif */
        int winCol;

        dst = backbuffer + OFFSET(vWinX, screenRow);

        for (winCol = 0; winCol < VSCROLL_WIN_W; winCol++)
        {
            int charPx  = winCol - textXoff;  /* position dans le texte (px) */
            unsigned char pix = VSCROLL_BG_COLOR;

            if (charPx >= 0 && charPx < textPxW)
            {
                int charIdx  = charPx / vFont.char_w;
                int pixInChr = charPx % vFont.char_w;
                unsigned char c = (unsigned char)line[charIdx];

                if (c >= (unsigned char)vFont.first_char &&
                    c <= (unsigned char)vFont.last_char)
                {
                    int gi   = c - (unsigned char)vFont.first_char;
                    int gCol = gi % vFont.cols;
                    int gRow = gi / vFont.cols;
                    int sx   = gCol * vFont.char_w + pixInChr;
                    int sy   = gRow * vFont.char_h + pixInLine;
                    unsigned char raw = font2GetPixel(&vFont, sx, sy);
                    if (raw != ck)
                        pix = raw;
                }
            }

            *dst = pix;
            dst++;
        }
    }
}

/* =========================================================
   SCENE PRINCIPALE
   ========================================================= */
void scene5(void)
{
    unsigned long now  = readTimer();
    unsigned long steps;
    int           i;

    /* -------------------------------------------------------
       INITIALISATION GLOBALE (une seule fois)
       ------------------------------------------------------- */
    if (!initialized)
    {
        int err;

        initialized = 1;
        state       = 0;

        err = loadPalette("font2\\16X16_F2.pal");
        if (err != PAL_OK) { quitRequested = 1; return; }

        /* Charger la police horizontale. */
        if (!font2Load(&hFont)) { quitRequested = 1; return; }

        hTextLen = f2len(HSCROLL_TEXT);
        textY    = (SCREEN_HEIGHT - hFont.char_h) / 2;
        rubanW   = (long)hTextLen * hFont.char_w;
        scrollX  = 0;

        clearScreen(SCROLL_BG_COLOR);
        drawRectFill(0, textY - SCROLL_BAR_H - 1,
                     SCREEN_WIDTH - 1, textY - 1,
                     SCROLL_BAR_COLOR);
        drawRectFill(0, textY + hFont.char_h,
                     SCREEN_WIDTH - 1,
                     textY + hFont.char_h + SCROLL_BAR_H,
                     SCROLL_BAR_COLOR);
        flip();
        /* lastH en ticks (pas en ms) pour la logique tick-based. */
        lastH = readTimer();
        return;
    }

    /* ======================================================= */
    /* ETAT 0 — Scroll horizontal                              */
    /* ======================================================= */
    if (state == 0)
    {
        /* Avancement en ticks : 1 pixel tous les HSCROLL_TICKS ticks.
           elapsedTime() retourne des ticks bruts (pas de ms),
           pas de division entière qui tronque → débit constant. */
        steps = elapsedTime(lastH, now) / HSCROLL_TICKS;
        if (steps == 0) return;
        if (steps > 8)  steps = 8;

        scrollX += (long)steps;

        /* Fin du ruban → déclencher le fondu. */
        if (scrollX >= rubanW)
        {
            state     = 1;
            fadeStart = now;
            return;
        }

        lastH += steps * HSCROLL_TICKS;

        /* Rendu horizontal. */
        for (i = 0; i < SCREEN_WIDTH; i++)
            blitColumn(i);

        drawRectFill(0, textY - SCROLL_BAR_H - 1,
                     SCREEN_WIDTH - 1, textY - 1,
                     SCROLL_BAR_COLOR);
        drawRectFill(0, textY + hFont.char_h,
                     SCREEN_WIDTH - 1,
                     textY + hFont.char_h + SCROLL_BAR_H,
                     SCROLL_BAR_COLOR);
        flip();
        return;
    }

    /* ======================================================= */
    /* ETAT 1 — Fondu sortant A→B                              */
    /* ======================================================= */
    if (state == 1)
    {
        unsigned long fadeElapsed = elapsedTimeMs(fadeStart, now);
        unsigned int  facteur;

        if (fadeElapsed >= FADE_MS)
        {
            /* Fondu terminé : initialiser la partie B. */
            fadePaletteInt5(workingPalette, 0);

            font2Free(&hFont);

            /* La police verticale est la même feuille. */
            vFont.sheet = 0;
            if (!font2Load(&vFont)) { quitRequested = 1; return; }

            rubanH  = (long)VLINES_COUNT * vFont.char_h;
            /* scrollY démarre négatif : le texte entre par le bas.
               scrollY = -(SCREEN_HEIGHT) place la première ligne
               juste sous l'écran. */
            scrollY = -(long)SCREEN_HEIGHT;
            vWinX   = (SCREEN_WIDTH - VSCROLL_WIN_W) / 2;

            /* Restaurer la palette à pleine luminosité. */
            setPalette(workingPalette);

            clearScreen(VSCROLL_BG_COLOR);
            flip();

            lastV = readTimer();
            state = 2;
            return;
        }

        /* Interpolation linéaire 64→0. */
        facteur = (unsigned int)((FADE_MS - fadeElapsed) * 64UL / FADE_MS);
        fadePaletteInt5(workingPalette, facteur);
        return;
    }

    /* ======================================================= */
    /* ETAT 2 — Scroll vertical                                */
    /* ======================================================= */
    if (state == 2)
    {
        steps = elapsedTimeMs(lastV, now) / VSCROLL_SPEED_MS;
        if (steps == 0) return;
        if (steps > 6)  steps = 6;

        scrollY += (long)steps;
        lastV   += steps * (VSCROLL_SPEED_MS * TARGET_HZ) / 1000UL;

        /* Fin : la dernière ligne a quitté le haut de l'écran.
           Condition : scrollY >= rubanH (tout le ruban est passé). */
        if (scrollY >= rubanH)
        {
            font2Free(&vFont);
            initialized = 0;
            state       = 0;
            sceneSignalEnd();
            return;
        }

        /* Rendu : effacer les marges latérales, puis blit ligne par ligne. */
        if (vWinX > 0)
        {
            drawRectFill(0, 0, vWinX - 1, SCREEN_HEIGHT - 1,
                         VSCROLL_BG_COLOR);
            drawRectFill(vWinX + VSCROLL_WIN_W, 0,
                         SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1,
                         VSCROLL_BG_COLOR);
        }

        for (i = 0; i < SCREEN_HEIGHT; i++)
            blitVLine(i);

        flip();
        return;
    }
}
