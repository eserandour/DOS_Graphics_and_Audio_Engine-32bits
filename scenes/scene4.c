/* =========================================================
   SCENE4.C — Affiche HELLO et WORLD avec font2
   =========================================================
   Charge la palette font.pal puis affiche :
     - "HELLO" centre horizontalement a y=84
     - "WORLD" en (0, 120) pour illustrer le positionnement
       libre (non centre) de font2DrawText.
   Duree : 3 secondes
   Aucune gestion clavier (sauf Échap global via INT 09h).
   ========================================================= */

#include "timer.h"
#include "video.h"
#include "graphics.h"
#include "palette.h"
#include "font2.h"
#include "scene.h"
#include "app.h"

/* Descripteur global (init statique au niveau fichier = OK pour Watcom) */
static Font2Desc s4_font = FONT2_DESC_DEFAULT;

void scene4(void)
{
    static int initialized = 0;

    const unsigned long scene_ms = 3000UL;
    unsigned long now = readTimer();

    if (!initialized)
    {
        int err;

        initialized = 1;
        err = loadPalette("font2\\font.pal");
        if (err != PAL_OK) { quitRequested = 1; return; }
        if (!font2Load(&s4_font)) { quitRequested = 1; return; }
        clearScreen(0);
        font2DrawTextCentered(&s4_font, "HELLO", 45);
        font2DrawTextCentered(&s4_font, "WORLD", 122);
        flip();  
    }

    if (elapsedTimeMs(sceneStart, now) >= scene_ms)
    {
        initialized = 0;
        font2Free(&s4_font);
        sceneSignalEnd();
    }
}
