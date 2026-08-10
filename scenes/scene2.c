/* =========================================================
   SCENE2.C — Scène : démonstration palette VGA
   =========================================================
   Phases et durées :
   1.  1 s : affichage statique (palette par défaut)
   2.  5 s : cycle de palette vers la droite
   3.  5 s : cycle de palette vers la gauche
   4.  3 s : lerp vers redPalette
   Aucune gestion clavier (sauf Échap global via INT 09h).
   ========================================================= */

#include "timer.h"
#include "video.h"
#include "palette.h"
#include "graphics.h"
#include "scene.h"

/* =========================================================
   FONCTION LOCALE : grille de palette
   ========================================================= */

/* Dessine la palette courante sous forme de grille 16x16.
   Chaque cellule est un carré de 10x10 pixels de couleur
   uniforme, avec 2 pixels d'espacement entre les cellules.
   La grille est centrée sur l'écran.
   static = visible uniquement dans ce fichier. */
static void drawPaletteGrid(void)
{
    const int gridSize = 16;  /* 16 colonnes et 16 lignes  */
    const int cellSize = 10;  /* taille d'une cellule (px) */
    const int spacing  = 2;   /* espace entre cellules (px)*/
    const int step     = cellSize + spacing;  /* pas entre deux cellules */

    /* Taille totale de la grille en pixels. */
    const int gridW = gridSize * cellSize + (gridSize - 1) * spacing;
    const int gridH = gridSize * cellSize + (gridSize - 1) * spacing;
    
    /* Offset de centrage. */
    const int offsetX = (SCREEN_WIDTH  - gridW) / 2;
    const int offsetY = (SCREEN_HEIGHT - gridH) / 2;

    int x, y;
    int i = 0;  /* index de couleur (0 à 255) */

    /* Parcourir les 16*16 = 256 cellules.
       i s'incrémente de gauche à droite, de haut en bas. */
    for (y = 0; y < gridSize; y++)
        for (x = 0; x < gridSize; x++)
        {
            int px = offsetX + x * step;  /* coin gauche de la cellule */
            int py = offsetY + y * step;  /* coin haut de la cellule   */
            /* Dessiner un carré plein de la couleur i.
               La couleur réelle dépend de la palette active
               dans le DAC VGA, pas des composantes RGB
               stockées dans workingPalette. */
            drawRectFill(px, py, px + cellSize - 1, py + cellSize - 1,
                         (unsigned char)i++);
        }
}

/* =========================================================
   SCÈNE PRINCIPALE
   ========================================================= */

void scene2(void)
{
    /* Variables statiques : persistent entre les appels. */
    static unsigned long lastRender  = 0UL;
    static int           initialized = 0;
    static int           phase4_initialized = 0;   /* flag pour la phase 4 */

    /* -------------------------------------------------
       Durées de chaque phase (ms)
       ------------------------------------------------- */
    const unsigned long D1 = 1000UL;   /* statique      */
    const unsigned long D2 = 5000UL;   /* cycle droite  */
    const unsigned long D3 = 5000UL;   /* cycle gauche  */
    const unsigned long D4 = 3000UL;   /* lerp → rouge     */

    /* Seuils cumulés : à quel moment (depuis sceneStart)
       chaque phase se termine. */
    const unsigned long T1 = D1;
    const unsigned long T2 = T1 + D2;
    const unsigned long T3 = T2 + D3;
    const unsigned long T4 = T3 + D4;

    /* Intervalle de rafraîchissement pour les phases
       animées (cycle, lerp) : 25 ms ≈ 40 Hz. */
    const unsigned long render_interval_ms = 25UL;

    unsigned long now     = readTimer();
    unsigned long elapsed = elapsedTimeMs(sceneStart, now);

    if (!initialized)
    {
        initialized        = 1;
        phase4_initialized = 0;   /* toujours réinitialiser au démarrage */
        copyPalette(workingPalette, defaultPalette);
        setPalette(workingPalette);
        buildRedPalette(redPalette);
        clearScreen(0);
        flip();
    }

    /* Phase 1 : statique */
    if (elapsed < T1)
    {
        drawPaletteGrid();
        flip();
        return;
    }

    /* Phase 2 : cycle droite */
    if (elapsed < T2)
    {
        if (elapsedTimeMs(lastRender, now) >= render_interval_ms)
        {
            cyclePaletteRight(workingPalette, 0, 255);
            drawPaletteGrid();
            flip();
            lastRender = now;
        }
        return;
    }

    /* Phase 3 : cycle gauche */
    if (elapsed < T3)
    {
        if (elapsedTimeMs(lastRender, now) >= render_interval_ms)
        {
            cyclePaletteLeft(workingPalette, 0, 255);
            drawPaletteGrid();
            flip();
            lastRender = now;
        }
        return;
    }
    
    /* -------------------------------------------------------
       Phase 4 : Lerp vers redPalette (T3 → T4 ms)
       Interpolation linéaire entre la palette de travail
       actuelle (après les cycles) et redPalette.
       On sauvegarde paletteA une seule fois au début de
       la phase pour que l'interpolation parte toujours du
       même point (et non de la palette en cours de lerp).
       ------------------------------------------------------- */
    if (elapsed < T4)
    {
        unsigned long fadeElapsed = elapsed - T3;   /* temps dans cette phase */
        float t = (float)fadeElapsed / (float)D4;   /* 0.0 → 1.0             */

        /* Sauvegarder la palette de départ une seule fois. */
        if (!phase4_initialized)
        {
            copyPalette(paletteA, workingPalette);  /* état actuel → source  */
            copyPalette(paletteB, redPalette);       /* cible                 */
            phase4_initialized = 1;
        }

        /* Calculer la palette interpolée et l'envoyer au DAC. */
        lerpPalette(workingPalette, paletteA, paletteB, t);
        setPalette(workingPalette);
        drawPaletteGrid();
        flip();
        return;
    }

    /* Fin de scène : remettre les flags à zéro AVANT sceneSignalEnd()
       pour éviter qu'un rejeu immédiat trouve phase4_initialized == 1. */
    initialized        = 0;
    phase4_initialized = 0;
    sceneSignalEnd();
}
