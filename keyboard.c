/* =========================================================
   KEYBOARD.C — Handler clavier bas niveau (INT 09h)
   =========================================================
   Environnement : Open Watcom 1.9, DOS

   Détecte uniquement la touche Échap (scan code 0x01) et
   lève quitRequested. L'EOI est envoyé au PIC directement ;
   on ne chaîne pas vers le BIOS (le buffer BIOS reste vide).
   ========================================================= */

#include <dos.h>    /* _disable, _enable, _dos_getvect,
                       _dos_setvect                        */
#include "keyboard.h"
#include "app.h"    /* quitRequested                       */

int quitRequested = 0;

static void interrupt (*old_kbd_isr)(void);

static void interrupt new_kbd_isr(void)
{
    unsigned char scan;
    _asm { in al, 0x60
           mov scan, al }

    if (scan == 0x01)           /* Échap make */
        quitRequested = 1;

    _asm { mov al, 0x20
           out 0x20, al }       /* EOI au PIC 8259 */
}

void installKeyboard(void)
{
    _disable();
    old_kbd_isr = _dos_getvect(0x09);
    _dos_setvect(0x09, new_kbd_isr);
    _enable();
}

void restoreKeyboard(void)
{
    _disable();
    _dos_setvect(0x09, old_kbd_isr);
    _enable();
}
