/* =========================================================
   TIMER.C — Système de timer haute résolution
   ========================================================= */

#include <dos.h>    /* _disable, _enable, _dos_getvect,
                       _dos_setvect, _chain_intr            */
#include <conio.h>  /* outp                                 */
#include "timer.h"

/* ---------------------------------------------------------
   Variables privées (static = invisibles hors de ce fichier)
   --------------------------------------------------------- */

/* Compteur global de ticks, incrémenté par l'ISR.
   volatile : le compilateur ne doit pas l'optimiser car
   sa valeur peut changer à tout moment (interruption). */
volatile unsigned long timer_ticks = 0;

/* Sauvegarde du pointeur vers l'ISR timer originale du BIOS.
   On la restaurera à la sortie du programme. */
static void interrupt (*old_timer_isr)(void);

/* Accumulateur pour le chaînage de l'ISR BIOS.
   Le BIOS attend une interruption toutes les 65536 unités
   PIT. On simule cela en accumulant DIVISOR à chaque tick
   et en appelant le BIOS quand on dépasse 65536. */
static unsigned long accum = 0;

/* ---------------------------------------------------------
   ISR principale — appelée 70 fois par seconde
   --------------------------------------------------------- */

static void interrupt new_timer_isr(void)
{
    /* Incrémenter le compteur de ticks global */
    timer_ticks++;

    /* Pourquoi cet accumulateur ?
       Notre ISR tourne à 70 Hz, mais le BIOS s'attend à être
       appelé à 18,2 Hz (= 1 193 180 / 65536). Si on ne l'appelle
       jamais, l'horloge DOS décroche et certains drivers plantent.

       Principe : à chaque tick, on accumule DIVISOR (≈ 17045).
       Quand l'accumulateur franchit 65536, on a "simulé" une
       interruption PIT complète à la fréquence BIOS, donc on
       passe la main au BIOS et on retranche 65536.

       En moyenne : 65536 / 17045 ≈ 3,84 ticks avant de chaîner,
       ce qui donne 70 / 3,84 ≈ 18,2 appels BIOS par seconde. */
    accum += DIVISOR;

    if (accum >= 65536UL)
    {
        /* L'accumulateur a dépassé 65536 : c'est le moment
           d'appeler l'ISR BIOS pour qu'elle fasse son
           travail (horloge, moteur de disquette, etc.).
           _chain_intr transfère le contrôle à l'ISR BIOS
           qui enverra elle-même l'EOI (End Of Interrupt)
           au PIC, donc on ne le fait pas ici. */
        accum -= 65536UL;
        _chain_intr(old_timer_isr);
    }
    else
    {
        /* On ne chaîne pas vers le BIOS ce coup-ci.
           On doit quand même signaler la fin d'interruption
           au PIC (Programmable Interrupt Controller) en
           envoyant un octet EOI (0x20) au port 0x20,
           sinon le PIC bloquera toutes les interruptions
           suivantes. */
        outp(0x20, 0x20);
    }
}

/* ---------------------------------------------------------
   Installation du timer
   --------------------------------------------------------- */

void installTimer(void)
{
    /* Désactiver les interruptions pendant la reconfiguration
       pour éviter qu'une interruption timer se déclenche
       pendant qu'on est en train de modifier les vecteurs. */
    _disable();

    /* Sauvegarder l'ISR originale du BIOS (vecteur 08h)
       pour pouvoir la restaurer à la sortie. */
    old_timer_isr = _dos_getvect(0x08);

    /* Installer notre propre ISR à la place. */
    _dos_setvect(0x08, new_timer_isr);

    /* Reprogrammer le PIT canal 0 :
       Port 0x43 = registre de contrôle du PIT.
       0x36 = 0011 0110 en binaire :
         bits 7-6 : 00 = canal 0
         bits 5-4 : 11 = accès octet bas puis octet haut
         bits 3-1 : 011 = mode 3 (onde carrée)
         bit  0   : 0 = binaire (pas BCD) */
    outp(0x43, 0x36);

    /* Charger le diviseur dans le PIT (canal 0, port 0x40)
       en deux fois : octet bas d'abord, octet haut ensuite. */
    outp(0x40, DIVISOR & 0xFF);   /* octet bas  */
    outp(0x40, DIVISOR >> 8);     /* octet haut */

    /* Réactiver les interruptions. */
    _enable();
}

/* ---------------------------------------------------------
   Restauration du timer
   --------------------------------------------------------- */

void restoreTimer(void)
{
    /* Désactiver les interruptions pendant la restauration. */
    _disable();

    /* Remettre l'ISR BIOS originale sur le vecteur 08h. */
    _dos_setvect(0x08, old_timer_isr);

    /* Remettre le PIT à sa fréquence par défaut (~18,2 Hz).
       Charger 0 dans le diviseur = 65536, ce qui donne
       1193180 / 65536 ≈ 18,2 Hz. */
    outp(0x43, 0x36);
    outp(0x40, 0);   /* octet bas  = 0 */
    outp(0x40, 0);   /* octet haut = 0 → diviseur = 65536 */

    /* Réactiver les interruptions. */
    _enable();
}

/* ---------------------------------------------------------
   Lecture du timer
   --------------------------------------------------------- */

/* Retourne le nombre de ticks depuis installTimer(). */
unsigned long readTimer(void)
{
    return timer_ticks;
}

/* ---------------------------------------------------------
   Calcul du temps écoulé
   --------------------------------------------------------- */

/* Retourne stop - start en ticks, en gérant le débordement.
   Quand timer_ticks dépasse 0xFFFFFFFF il repasse à 0.
   Si stop < start c'est qu'il y a eu débordement. */
unsigned long elapsedTime(unsigned long start, unsigned long stop)
{
    if (stop >= start)
        return stop - start;

    /* Cas de débordement : on calcule la distance en
       faisant le tour complet de l'espace 32 bits. */
    return (0xFFFFFFFFUL - start) + stop + 1;
}

/* Convertit un écart de ticks en millisecondes.
   Formule : ticks * 1000 / TARGET_HZ. */
unsigned long elapsedTimeMs(unsigned long start, unsigned long stop)
{
    return (elapsedTime(start, stop) * 1000UL) / TARGET_HZ;
}

/* ---------------------------------------------------------
   Pause précise
   --------------------------------------------------------- */

/* Attente active (busy-wait) pendant ms millisecondes.
   Convertit ms en ticks (ms * TARGET_HZ / 1000), puis
   boucle jusqu'à ce que timer_ticks ait avancé d'autant.

   Pourquoi le NOP dans la boucle ?
   Sans instruction, certains compilateurs suppriment la
   boucle entière (ils supposent qu'une boucle sans effet
   de bord est inutile). Le NOP force la génération d'une
   instruction réelle et préserve le comportement d'attente.
   timer_ticks est volatile, ce qui protège déjà en théorie,
   mais le NOP est une garantie explicite supplémentaire. */
void pause(unsigned long ms)
{
    unsigned long start          = timer_ticks;
    unsigned long ticks_to_wait  = (ms * TARGET_HZ) / 1000UL;

    while ((unsigned long)(timer_ticks - start) < ticks_to_wait)
    {
        _asm { nop }   /* empêche l'optimiseur de supprimer la boucle */
    }
}
