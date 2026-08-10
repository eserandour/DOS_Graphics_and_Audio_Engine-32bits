@echo off
REM =========================================================
REM BUILD.BAT — Compilation 32 bits (386+, modele flat)
REM =========================================================
REM Necessite Open Watcom 1.9 avec wcc386/wlink dans le PATH,
REM et DOS32A.EXE + STUB32A.EXE accessibles depuis LINK.RSP
REM (voir PORTAGE_32BITS.md pour la mise en place de DOS/32A).
REM
REM -3s  = instructions 386, convention d'appel "stack"
REM        (plus sure a marier avec les gestionnaires
REM        d'interruption ecrits a la main que -3r)
REM -mf  = modele memoire "flat" (obligatoire pour DOS/32A)
REM -os  = optimise pour la taille (comme le build 16 bits)
REM -I.  = cherche les .h dans le repertoire courant
REM =========================================================

wcc386 -3s -mf -os -I. main.c
wcc386 -3s -mf -os -I. timer.c
wcc386 -3s -mf -os -I. keyboard.c
wcc386 -3s -mf -os -I. video.c
wcc386 -3s -mf -os -I. palette.c
wcc386 -3s -mf -os -I. graphics.c
wcc386 -3s -mf -os -I. image.c
wcc386 -3s -mf -os -I. sprite.c
wcc386 -3s -mf -os -I. font1.c
wcc386 -3s -mf -os -I. font1\f1_8x8.c
wcc386 -3s -mf -os -I. font1\f1_8x16.c
wcc386 -3s -mf -os -I. font1\f1_16x16.c
wcc386 -3s -mf -os -I. font2.c
wcc386 -3s -mf -os -I. sblaster.c
wcc386 -3s -mf -os -I. s3m.c
wcc386 -3s -mf -os -I. wav.c
wcc386 -3s -mf -os -I. audio.c
wcc386 -3s -mf -os -I. scene.c
wcc386 -3s -mf -os -I. scenes\scene0.c
wcc386 -3s -mf -os -I. scenes\scene1.c
wcc386 -3s -mf -os -I. scenes\scene2.c
wcc386 -3s -mf -os -I. scenes\scene3.c
wcc386 -3s -mf -os -I. scenes\scene4.c
wcc386 -3s -mf -os -I. scenes\scene5.c
wcc386 -3s -mf -os -I. scenes\scene6.c
wcc386 -3s -mf -os -I. scenes\scene7.c
wcc386 -3s -mf -os -I. scenes\scene8.c

wlink @LINK.RSP
