#ifndef KEYBOARD_H
#define KEYBOARD_H

/* =========================================================
   KEYBOARD.H — Handler clavier bas niveau (INT 09h)
   =========================================================
   Environnement : Open Watcom 1.9, DOS

   Installe une ISR sur INT 09h qui détecte la touche Échap
   et lève quitRequested (déclaré dans app.h).
   Toutes les autres touches sont ignorées.
   L'ISR gère elle-même l'EOI au PIC.
   ========================================================= */

void installKeyboard(void);
void restoreKeyboard(void);

#endif /* KEYBOARD_H */
