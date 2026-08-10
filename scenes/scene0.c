/* =========================================================
   SCENE0.C — Scène : écran noir
   =========================================================
   Durée totale : 1 seconde.
   Aucune gestion clavier (sauf Échap global via INT 09h).
   ========================================================= */

#include "timer.h"
#include "video.h"
#include "graphics.h"
#include "scene.h"

void scene0(void)
{
    static int initialized = 0;
    const unsigned long scene_ms = 1000UL; // 1 seconde

    if (!initialized)
    {
        initialized = 1;
        clearScreen(0); // Écran noir
        flip();
        sceneStart = readTimer(); // Démarre le chrono
    }

    // Si le temps écoulé dépasse 1 seconde, on termine la scène
    if (elapsedTimeMs(sceneStart, readTimer()) >= scene_ms)
    {
        initialized = 0;
        sceneSignalEnd();
    }
}
