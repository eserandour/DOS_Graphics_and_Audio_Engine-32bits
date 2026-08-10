/* =========================================================
   SCENE8.C — Scène de test : cycle de vie complet du moteur
              audio S3M (playMusic / fadeMusicIn / fadeMusicOut
              / stopMusic)
   =========================================================
   Enchaîne les quatre phases du cycle de vie audio sur
   musique.s3m ("Starshine - PM", Gv=48 Mv=48 — un morceau qui
   n'est PAS censé sortir à 100 % du numérique, bon test pour
   vérifier que Gv/Mv sont bien respectés, voir la section
   VOLUME de audio.h) :

     0.0s -  6.0s : FADE IN   — fadeMusicIn(6000)
     6.0s - 22.0s : LECTURE   — plein volume "normal" du morceau
    22.0s - 28.0s : FADE OUT  — fadeMusicOut(6000)
    28.0s - 32.0s : ARRET     — stopMusic(), mémoire libérée      

   L'écran affiche en direct la phase courante, le statut
   isMusicPlaying(), et le nombre de bouclages détectés via
   hasMusicLooped() (compteur cumulé — voir audio.h : le drapeau
   est consommé à chaque appel, donc on l'accumule nous-mêmes).
   Ce morceau dure plusieurs minutes (40 ordres/motifs à
   speed=6/tempo=125) : ne pas s'étonner que ce compteur reste à
   0 sur les 32 secondes de la scène, c'est attendu — le
   mécanisme est démontré, pas un bouclage complet.

   AFFICHAGE — police FONT1_BIOS (8x8), écran 320 px : toutes
   les chaînes affichées sont volontairement tenues sous ~36
   caractères pour ne jamais déborder à l'écran.

   NOTE C89 (Open Watcom)
   ----------------------
   Toutes les déclarations en tête de bloc.
   ========================================================= */

#include <stdio.h>
#include "timer.h"
#include "video.h"
#include "graphics.h"
#include "font1.h"
#include "scene.h"
#include "audio.h"
#include "palette.h"

#define FADE_MS     6000UL   /* durée de chaque fondu 3000 à l'origine*/
#define PLAY_MS     16000UL   /* palier plein volume entre les deux fondus */
#define STOPPED_MS  4000UL   /* palier "arrêté" après stopMusic() */

#define T_FADE_IN_END   (FADE_MS)
#define T_PLAY_END      (T_FADE_IN_END + PLAY_MS)
#define T_FADE_OUT_END  (T_PLAY_END + FADE_MS)
#define T_SCENE_END     (T_FADE_OUT_END + STOPPED_MS)

/* Phases, dans l'ordre chronologique. */
#define PHASE_FADE_IN   0
#define PHASE_PLAYING   1
#define PHASE_FADE_OUT  2
#define PHASE_STOPPED   3

static const char *statusText(int r)
{
    switch (r)
    {
    case AUD_OK:         return "OK";
    case AUD_ERR_NOCARD: return "ERREUR: pas de carte son (BLASTER)";
    case AUD_ERR_FILE:   return "ERREUR: musique.s3m introuvable";
    case AUD_ERR_FORMAT: return "ERREUR: fichier .s3m invalide/tronque";
    case AUD_ERR_MEM:    return "ERREUR: memoire insuffisante";
    default:             return "ERREUR: inconnue";
    }
}

static const char *phaseText(int phase)
{
    switch (phase)
    {
    case PHASE_FADE_IN:  return "FADE IN   0% -> 100% (6s)";
    case PHASE_PLAYING:  return "LECTURE - volume normal du fichier";
    case PHASE_FADE_OUT: return "FADE OUT  100% -> 0% (6s)";
    default:              return "ARRET - stopMusic(), memoire liberee";
    }
}

static void formatSeconds(char *buf, unsigned long ms)
{
    unsigned long sec  = ms / 1000UL;
    unsigned long dsec = (ms % 1000UL) / 100UL;
    sprintf(buf, "%lu.%lus", sec, dsec);
}

void scene8(void)
{
    static int          initialized  = 0;
    static int          lastPhase    = -1;
    static int          loopCount    = 0;
    static int          loadStatus   = AUD_OK;
    static unsigned long lastDrawMs  = 0;

    unsigned long elapsed;
    int           phase;
    int           phaseChanged;
    char          line[40];
    char          elapsedStr[16];
    char          totalStr[16];

    if (!initialized)
    {
        initialized = 1;
        lastPhase   = -1;
        loopCount   = 0;
        lastDrawMs  = 0;

        font1InitBios();

        /* Cette scene affiche du texte avec des indices de couleur fixes
           (15, 14, 8, 7, 4) en supposant la palette VGA par defaut.
           Sans ce reset, un passage precedent par une scene qui modifie
           la palette (ex : scene7, qui termine sur un fondu au noir de
           rainbowPalette) laisse le DAC dans un etat quasi noir : le
           texte serait alors dessine avec les bons indices, mais ces
           indices pointeraient vers des teintes invisibles. Vu au
           deuxieme passage sur scene8 dans la playlist bouclee. */
        copyPalette(workingPalette, defaultPalette);
        setPalette(workingPalette);

        loadStatus = playMusic("audios\\musique.s3m");

        /* La musique vient de démarrer à plein volume "normal" (Gv/Mv
           du fichier, voir stopMusic()/playMusic() dans s3m.c qui
           remettent le fondu à 127/127 au chargement) : pour vraiment
           démarrer de RIEN, on la coupe instantanément (durée 0 =
           application immédiate, voir s3mFadeTo) avant de lancer la
           vraie montée progressive juste après. Sans effet si le
           chargement a échoué (fadeMusicOut/fadeMusicIn ne plantent
           jamais, voir audio.c). */
        fadeMusicOut(0);
        fadeMusicIn(FADE_MS);

        sceneStart = readTimer();
    }

    elapsed = elapsedTimeMs(sceneStart, readTimer());

    /* Compteur cumulé de bouclages : hasMusicLooped() consomme son
       drapeau à chaque appel (voir audio.h), donc on l'appelle une
       fois par frame et on accumule nous-mêmes plutôt que de risquer
       d'en rater un entre deux frames. */
    loopCount += hasMusicLooped();

    if      (elapsed < T_FADE_IN_END)  phase = PHASE_FADE_IN;
    else if (elapsed < T_PLAY_END)     phase = PHASE_PLAYING;
    else if (elapsed < T_FADE_OUT_END) phase = PHASE_FADE_OUT;
    else                                 phase = PHASE_STOPPED;

    /* Actions ponctuelles au changement de phase — appelées UNE SEULE
       fois chacune (comparaison à lastPhase), jamais à chaque frame :
       rappeler fadeMusicOut()/fadeMusicIn() en boucle referait
       repartir le fondu depuis son niveau courant à chaque frame,
       ce qui ne descendrait/monterait jamais réellement (voir
       s3mFadeTo, qui redémarre toujours depuis fadeLevel actuel). */
    phaseChanged = (phase != lastPhase);
    if (phaseChanged)
    {
        if (phase == PHASE_FADE_OUT)
        {
            fadeMusicOut(FADE_MS);
        }
        else if (phase == PHASE_STOPPED)
        {
            stopMusic();   /* coupe le son ET libère les ~290 Ko
                               de musique.s3m — voir audio.c */
        }
        lastPhase = phase;
    }

    /* -------------------------------------------------------
       Affichage — redessiné au changement de phase, ou au plus
       4 fois par seconde sinon (largement suffisant pour un
       compteur lisible par un humain). Redessiner à CHAQUE frame
       (clearScreen + 8 lignes de texte + flip, potentiellement
       70 fois/seconde) monopolisait assez de CPU pour empêcher
       audioUpdate() de suivre le rythme réel du DMA : l'horloge
       interne du moteur audio prenait alors plusieurs secondes de
       retard sur l'horloge murale (bouclage constaté à l'usage),
       ce qui ressemblait à des saccades/silences pendant les
       fondus alors que le calcul de volume lui-même était correct.
       ------------------------------------------------------- */
    if (phaseChanged || elapsed - lastDrawMs >= 250UL || lastDrawMs == 0)
    {
        clearScreen(0);

        font1DrawTextCentered( 48, "TEST AUDIO S3M", 15, &FONT1_BIOS);
        font1DrawTextCentered( 60, "playMusic + fadeIn/fadeOut + stop", 8, &FONT1_BIOS);

        font1DrawTextCentered( 84, "Fichier : musique.s3m", 7, &FONT1_BIOS);
        font1DrawTextCentered( 96, statusText(loadStatus),
                               (loadStatus == AUD_OK) ? 15 : 4, &FONT1_BIOS);

        font1DrawTextCentered(120, phaseText(phase), 14, &FONT1_BIOS);

        formatSeconds(elapsedStr, elapsed);
        formatSeconds(totalStr,   T_SCENE_END);
        sprintf(line, "t = %s / %s", elapsedStr, totalStr);
        font1DrawTextCentered(140, line, 7, &FONT1_BIOS);

        sprintf(line, "isMusicPlaying() = %s", isMusicPlaying() ? "OUI" : "NON");
        font1DrawTextCentered(156, line, 7, &FONT1_BIOS);

        sprintf(line, "bouclages detectes = %d", loopCount);
        font1DrawTextCentered(172, line, 7, &FONT1_BIOS);

        flip();

        lastDrawMs = elapsed;
    }

    if (elapsed >= T_SCENE_END)
    {
        initialized = 0;
        sceneSignalEnd();
    }
}
