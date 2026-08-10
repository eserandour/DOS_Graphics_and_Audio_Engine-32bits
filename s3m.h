#ifndef S3M_H
#define S3M_H

/* =========================================================
   S3M.H — Chargeur et moteur de lecture de modules S3M
   =========================================================
   Environnement : Open Watcom 1.9, DOS

   SOUS-ENSEMBLE SUPPORTÉ (volontairement limité — voir
   les notes de audio.c pour le détail complet) :
     - Échantillons PCM non compressés, 8 ou 16 bits, mono
       ou stéréo (convertis au chargement en 8 bits non
       signés mono, format natif du moteur de mixage).
     - Jusqu'à S3M_MAX_CHANNELS voies mixées simultanément
       (les voies au-delà sont ignorées si le module en
       utilise davantage — rare en pratique).
     - Volume global initial (Gv) et volume maître (Mv) lus
       depuis l'en-tête, fidèles au fichier plutôt que forcés
       à un maximum arbitraire (voir application dans s3m.c).
     - Commandes : Axx (vitesse), Txx (tempo), Bxx (saut de
       position), Cxx (rupture de motif, paramètre décodé en
       BCD conformément au format S3M réel), Dxx (glissement
       de volume, avec mémoire d'effet et glissements fins
       DxF/DFx appliqués une seule fois), Vxx (volume global),
       colonne de volume.
     - Toutes les autres commandes (portamento, vibrato,
       arpège, offset...) sont ignorées : la note se
       déclenche quand même, seul l'effet fin est absent.
     - Bouclage automatique : à la fin de la table d'ordres,
       la lecture reprend au premier ordre valide — adapté
       à une musique de fond de démo qui tourne en boucle.
       Chaque bouclage peut être détecté depuis l'extérieur
       (voir s3mConsumeLoopFlag ci-dessous).
   ========================================================= */

/* ---------------------------------------------------------
   Codes de retour
   --------------------------------------------------------- */
#define S3M_OK          0
#define S3M_ERR_FILE    1   /* impossible d'ouvrir le fichier      */
#define S3M_ERR_READ    2   /* fichier tronqué / lecture incomplète */
#define S3M_ERR_FORMAT  3   /* signature 'SCRM' absente             */
#define S3M_ERR_MEM     4   /* mémoire insuffisante                 */

/* Nombre maximal de voies réellement mixées (le format S3M
   autorise jusqu'à 32 canaux, mais en mixer autant en logiciel
   sur cette cible est hors de portée — voir note ci-dessus). */
#define S3M_MAX_CHANNELS  16

/* ---------------------------------------------------------
   API
   --------------------------------------------------------- */

/* À appeler une seule fois avant tout usage, avec la fréquence
   de mixage du moteur audio (voir audio.h : MIX_RATE). */
void s3mInit(unsigned long mixRate);

/* Charge et démarre la lecture de 'filename'. Le module
   précédemment chargé (s'il y en a un) est libéré d'abord.
   En cas d'échec, l'état "aucune musique" est conservé
   (silence), rien ne plante. */
int s3mLoad(const char *filename);

/* Libère le module en cours et repasse en silence. */
void s3mUnload(void);

/* Génère 'n' octets de musique (0..255, silence = 128) dans
   buf. Écrit TOUJOURS n octets, y compris du silence         
   si aucun module n'est chargé — le buffer peut donc servir
   de base pour un mixage additif ultérieur (voir wavMix). */
void s3mMix(unsigned char *buf, unsigned int n);

/* Retourne 1 si un module est chargé ET en cours de lecture,
   0 sinon (rien chargé, ou fin de lecture faute de motif
   jouable — voir advanceRow). */
int s3mIsPlaying(void);

/* Retourne 1 si la table d'ordres a rebouclé sur son point de
   départ depuis le dernier appel à cette fonction, puis remet
   le drapeau à 0 (consommé) ; retourne 0 sinon. Un bouclage
   entre deux appels n'est jamais perdu, mais deux bouclages
   entre deux appels ne comptent que pour un seul 1 — pour
   compter précisément les tours, interroger plus souvent
   (typiquement une fois par tour de boucle principale). */
int s3mConsumeLoopFlag(void);

/* Lance un fondu (fade-in ou fade-out) vers 'targetPercent' (0-100,
   clampé) du volume normal, étalé sur 'durationMs' millisecondes de
   lecture réelle (indépendant du tempo/vitesse du morceau — voir
   doTick). Un fondu déjà en cours est repris depuis son niveau actuel,
   pas depuis 0/100 : appeler s3mFadeTo() en plein fondu ne produit
   aucun saut audible. durationMs=0 applique le niveau cible
   immédiatement. N'affecte QUE l'enveloppe de fondu : le volume
   "normal" (Gv/Mv/Vxx/volume par note) continue d'être respecté
   par-dessous, voir doTick(). */
void s3mFadeTo(int targetPercent, unsigned long durationMs);

#endif /* S3M_H */
