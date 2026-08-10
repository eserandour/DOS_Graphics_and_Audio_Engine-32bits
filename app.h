#ifndef APP_H
#define APP_H

/* =========================================================
   APP.H — Flags globaux de l'application
   =========================================================
   À inclure par tout module qui doit signaler ou consulter
   un état global du programme (hors logique de scène).
   ========================================================= */

/* Levé à 1 pour demander la sortie propre du programme.
   Écrit par keyboard.c (touche Échap) ou par une scène
   en cas d'erreur fatale. Lu par la boucle de main.c. */
extern int quitRequested;

#endif /* APP_H */
