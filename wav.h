#ifndef WAV_H
#define WAV_H

/* =========================================================
   WAV.H — Chargement et mixage de sons WAV (effets sonores)
   =========================================================
   Environnement : Open Watcom 1.9, DOS (modèle flat 32 bits, DOS/32A)

   PRINCIPE
   --------
   Chaque appel à wavPlay() charge ENTIÈREMENT le fichier en
   mémoire, convertit les échantillons au format
   natif du moteur (8 bits non signé, mono) et l'assigne à
   une voie libre parmi WAV_MAX_VOICES. La lecture démarre
   au tick audio suivant, sans re-toucher le disque ensuite.
   Adapté à de courts effets sonores (clics, impacts...),
   pas à de la musique longue (voir s3m.h pour cela).

   FORMATS SUPPORTÉS
   ------------------
   RIFF/WAVE canonique, PCM non compressé, 8 ou 16 bits,
   mono ou stéréo, n'importe quelle fréquence
   d'échantillonnage (ré-échantillonnée à la volée vers la
   fréquence du moteur audio par pas fixe — voir audio.h).

   LIMITES
   -------
   - Un seul bloc malloc() par son : sa taille n'est bornée que
     par la mémoire disponible (voir wav.c), ce qui plafonne la
     durée max d'un effet uniquement à cette limite.
   - Pas de compression (ADPCM, MP3...) ni de flottant.
   ========================================================= */

#define WAV_OK          0
#define WAV_ERR_FILE    1
#define WAV_ERR_READ    2
#define WAV_ERR_FORMAT  3
#define WAV_ERR_MEM     4

/* Nombre de sons pouvant jouer simultanément. Si toutes les
   voies sont occupées, la plus ancienne est réutilisée (le
   nouveau son est toujours audible, jamais silencieusement
   ignoré). */
#define WAV_MAX_VOICES  4

/* À appeler une seule fois avant tout usage, avec la fréquence
   de mixage du moteur audio (voir audio.h : MIX_RATE). */
void wavInit(unsigned long mixRate);

/* Charge 'filename' et le joue sur une voie libre (ou la plus
   ancienne si toutes sont occupées). Retourne WAV_OK ou un
   code d'erreur ; en cas d'erreur, aucune voie n'est modifiée. */
int wavPlay(const char *filename);

/* Mixe les voies actives par-dessus 'n' octets déjà présents
   dans buf — buf doit déjà contenir la musique (voir
   s3mMix), wavMix ajoute sans écraser. */
void wavMix(unsigned char *buf, unsigned int n);

/* Arrête toutes les voix et libère leur mémoire. À appeler à
   la fermeture du moteur audio. */
void wavStopAll(void);

#endif /* WAV_H */
