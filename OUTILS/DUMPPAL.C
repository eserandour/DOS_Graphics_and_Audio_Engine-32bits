/* =========================================================
   DUMPPAL.C — Dump de la palette par défaut du mode 13h
   =========================================================
   Passe en mode 13h, lit les 256 couleurs du DAC VGA
   (palette initialisée par le BIOS pour ce mode) et les
   écrit dans DEFAULT.PAL, puis repasse en mode texte.

   Compilation et exécution (FreeDOS, Open Watcom 1.9) :
     Se placer dans le répertoire OUTILS qui contient ce fichier
     wcl -ml -0 -os -I.. DUMPPAL.C ..\palette.c ..\video.c
     DUMPPAL

   Le fichier DEFAULT.PAL produit est au format standard du
   projet : 768 octets bruts (256 × R/G/B sur 6 bits).
   ========================================================= */

#include <stdio.h>
#include "palette.h"
#include "video.h"

int main(void)
{
    /* Passer en mode 13h : le BIOS charge sa palette par défaut. */
    setVideoMode(0x13);

    /* Lire le DAC immédiatement, avant tout autre changement. */
    getPalette(defaultPalette);

    /* Repasser en mode texte. */
    setVideoMode(0x03);

    if (savePalette(defaultPalette, "DEFAULT.PAL"))
        printf("DEFAULT.PAL ecrit (768 octets).\n");
    else
        printf("ERREUR : impossible d'ecrire DEFAULT.PAL.\n");

    return 0;
}
