/* =========================================================
   WAV.C — Chargement et mixage de sons WAV
   =========================================================
   Voir wav.h pour la documentation de l'API et les limites.
   ========================================================= */

#include <stdio.h>    /* FILE, fopen, fread, fseek, ftell, fclose */
#include <string.h>   /* memcpy, memset                           */
#include <malloc.h>   /* malloc, free                             */
#include "wav.h"

typedef struct {
    unsigned char *data;
    unsigned long        len;    /* échantillons (octets, 8 bits mono) */
    unsigned long        pos;    /* position 16.16 dans data           */
    unsigned long        step;   /* pas 16.16 (rééchantillonnage)      */
    int                   active;
} WavVoice;

static WavVoice voices[WAV_MAX_VOICES];
static unsigned long engineRate = 11025UL;

void wavInit(unsigned long mixRate)
{
    int i;
    engineRate = (mixRate == 0) ? 11025UL : mixRate;
    for (i = 0; i < WAV_MAX_VOICES; i++)
    {
        voices[i].data   = NULL;
        voices[i].active = 0;
    }
}

/* ---------------------------------------------------------
   Lecture bas niveau little-endian
   --------------------------------------------------------- */

static unsigned int rdWordLE(FILE *f)
{
    unsigned char b[2];
    if (fread(b, 1, 2, f) != 2) return 0;
    return (unsigned int)b[0] | ((unsigned int)b[1] << 8);
}

static unsigned long rdDwordLE(FILE *f)
{
    unsigned char b[4];
    if (fread(b, 1, 4, f) != 4) return 0;
    return (unsigned long)b[0] | ((unsigned long)b[1] << 8) |
           ((unsigned long)b[2] << 16) | ((unsigned long)b[3] << 24);
}

/* ---------------------------------------------------------
   wavPlay
   --------------------------------------------------------- */

int wavPlay(const char *filename)
{
    FILE *f;
    char id[4];
    unsigned long chunkSize;
    unsigned int  audioFormat, channelsIn, blockAlign, bitsPerSample;
    unsigned long sampleRate, byteRate;
    int haveFmt, haveData;
    unsigned long dataSize, dataOffset;
    int slot, i;
    unsigned long destLen, remaining, produced;
    unsigned int  frameBytes;
    unsigned char *dest;
    unsigned char nearBuf[512];

    f = fopen(filename, "rb");
    if (!f) return WAV_ERR_FILE;

    if (fread(id, 1, 4, f) != 4 ||
        id[0] != 'R' || id[1] != 'I' || id[2] != 'F' || id[3] != 'F')
    { fclose(f); return WAV_ERR_FORMAT; }

    rdDwordLE(f);   /* taille RIFF globale, non utilisée */

    if (fread(id, 1, 4, f) != 4 ||
        id[0] != 'W' || id[1] != 'A' || id[2] != 'V' || id[3] != 'E')
    { fclose(f); return WAV_ERR_FORMAT; }

    haveFmt = 0;
    haveData = 0;
    audioFormat = 0; channelsIn = 1; bitsPerSample = 8;
    sampleRate = engineRate; byteRate = 0; blockAlign = 0;
    dataSize = 0; dataOffset = 0;

    while (!haveData)
    {
        if (fread(id, 1, 4, f) != 4) break;
        chunkSize = rdDwordLE(f);

        if (id[0]=='f' && id[1]=='m' && id[2]=='t' && id[3]==' ')
        {
            unsigned long fmtStart;
            fmtStart = (unsigned long)ftell(f);
            audioFormat   = rdWordLE(f);
            channelsIn    = rdWordLE(f);
            sampleRate    = rdDwordLE(f);
            byteRate      = rdDwordLE(f);
            blockAlign    = rdWordLE(f);
            bitsPerSample = rdWordLE(f);
            fseek(f, (long)(fmtStart + chunkSize), SEEK_SET);
            if (chunkSize & 1UL) fseek(f, 1, SEEK_CUR);
            haveFmt = 1;
        }
        else if (id[0]=='d' && id[1]=='a' && id[2]=='t' && id[3]=='a')
        {
            dataSize   = chunkSize;
            dataOffset = (unsigned long)ftell(f);
            haveData   = 1;
        }
        else
        {
            fseek(f, (long)chunkSize, SEEK_CUR);
            if (chunkSize & 1UL) fseek(f, 1, SEEK_CUR);
        }
    }
    (void)byteRate;
    (void)blockAlign;

    if (!haveFmt || !haveData || audioFormat != 1)
    { fclose(f); return WAV_ERR_FORMAT; }
    if (bitsPerSample != 8 && bitsPerSample != 16)
    { fclose(f); return WAV_ERR_FORMAT; }
    if (channelsIn < 1 || channelsIn > 2)
    { fclose(f); return WAV_ERR_FORMAT; }
    if (sampleRate == 0) sampleRate = engineRate;

    frameBytes = (unsigned int)(bitsPerSample / 8) * (unsigned int)channelsIn;
    if (frameBytes == 0) { fclose(f); return WAV_ERR_FORMAT; }

    destLen = dataSize / frameBytes;
    if (destLen == 0) { fclose(f); return WAV_ERR_FORMAT; }
    /* malloc() en modèle flat 32 bits n'impose aucune limite de
       taille de bloc : le son entier peut être chargé d'un coup. */

    dest = (unsigned char *)malloc((size_t)destLen);
    if (!dest) { fclose(f); return WAV_ERR_MEM; }

    fseek(f, (long)dataOffset, SEEK_SET);

    remaining = destLen;
    produced  = 0;
    while (remaining > 0)
    {
        unsigned int framesThisChunk;
        unsigned int bytesToRead;
        unsigned int k;
        unsigned char outChunk[128];

        framesThisChunk = 128;
        if ((unsigned long)framesThisChunk > remaining)
            framesThisChunk = (unsigned int)remaining;

        bytesToRead = framesThisChunk * frameBytes;
        if (fread(nearBuf, 1, bytesToRead, f) != bytesToRead)
        {
            memset(dest + produced, 128, (size_t)(destLen - produced));
            break;
        }

        for (k = 0; k < framesThisChunk; k++)
        {
            int v;

            if (bitsPerSample == 8)
            {
                unsigned char s0;
                s0 = nearBuf[k * frameBytes];
                v = (int)s0;   /* WAV 8 bits = non signé par convention RIFF */
                if (channelsIn == 2)
                {
                    unsigned char s1;
                    s1 = nearBuf[k * frameBytes + 1];
                    v = (v + (int)s1) / 2;
                }
            }
            else
            {
                int lo, hi;
                short s16;
                lo  = nearBuf[k * frameBytes];
                hi  = nearBuf[k * frameBytes + 1];
                /* (unsigned short)->(short) reinterprete le motif binaire
                   en complement a deux : s16 est signe correctement par
                   cette conversion. "short" est garanti 16 bits par
                   Watcom aussi bien en cible 16 qu'en cible 32 bits
                   (contrairement a "int", qui passe de 16 a 32 bits
                   entre wcc et wcc386 — utiliser "int" ici casserait le
                   signe des echantillons negatifs en modele flat). */
                s16 = (short)((unsigned short)lo | ((unsigned short)hi << 8));
                v = ((int)s16 >> 8) + 128;
                if (channelsIn == 2)
                {
                    int lo2, hi2;
                    short s16b;
                    int v2;
                    lo2  = nearBuf[k * frameBytes + 2];
                    hi2  = nearBuf[k * frameBytes + 3];
                    s16b = (short)((unsigned short)lo2 | ((unsigned short)hi2 << 8));
                    v2 = ((int)s16b >> 8) + 128;
                    v  = (v + v2) / 2;
                }
            }
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            outChunk[k] = (unsigned char)v;
        }

        memcpy(dest + produced, outChunk, framesThisChunk);
        produced  += framesThisChunk;
        remaining -= framesThisChunk;
    }

    fclose(f);

    /* Voie libre en priorité, sinon la plus ancienne (0) : playSound()
       est ainsi TOUJOURS audible, jamais silencieusement ignoré. */
    slot = -1;
    for (i = 0; i < WAV_MAX_VOICES; i++)
        if (!voices[i].active) { slot = i; break; }
    if (slot < 0) slot = 0;

    if (voices[slot].data) free(voices[slot].data);

    voices[slot].data = dest;
    voices[slot].len  = destLen;
    voices[slot].pos  = 0;
    voices[slot].step = (sampleRate / engineRate) << 16;
    voices[slot].step += ((sampleRate % engineRate) << 16) / engineRate;
    if (voices[slot].step == 0) voices[slot].step = 1;   /* jamais figé */
    voices[slot].active = 1;

    return WAV_OK;
}

/* ---------------------------------------------------------
   wavMix
   --------------------------------------------------------- */

void wavMix(unsigned char *buf, unsigned int n)
{
    int i;

    for (i = 0; i < WAV_MAX_VOICES; i++)
    {
        unsigned int s;

        if (!voices[i].active) continue;

        for (s = 0; s < n; s++)
        {
            unsigned long idx;
            int sample, cur, mixed;

            idx = voices[i].pos >> 16;
            if (idx >= voices[i].len) { voices[i].active = 0; break; }

            sample = (int)voices[i].data[idx] - 128;
            cur    = (int)buf[s] - 128;
            mixed  = cur + sample;
            if (mixed > 127)  mixed = 127;
            if (mixed < -128) mixed = -128;
            buf[s] = (unsigned char)(mixed + 128);

            voices[i].pos += voices[i].step;
        }
    }
}

void wavStopAll(void)
{
    int i;
    for (i = 0; i < WAV_MAX_VOICES; i++)
    {
        if (voices[i].data) { free(voices[i].data); voices[i].data = NULL; }
        voices[i].active = 0;
    }
}
