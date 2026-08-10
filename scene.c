/* =========================================================
   SCENE.C — Gestionnaire de scenes
   ========================================================= */

#include "timer.h"
#include "scene.h"

void scene0(void);
void scene1(void);
void scene2(void);
void scene3(void);
void scene4(void);
void scene5(void);
void scene6(void);
void scene7(void);
void scene8(void);

Scene currentScene = SCENE_0;
unsigned long sceneStart = 0;
SceneEndHandler onSceneEnd = 0;   /* NULL par défaut — à brancher dans main.c */

typedef void (*SceneFunc)(void);

static SceneFunc scenes[] = {
    scene0,
    scene1,
    scene2,
    scene3,
    scene4,
    scene5,
    scene6,
    scene7,
    scene8,
};

void setScene(Scene s)
{
    currentScene = s;
    sceneStart   = readTimer();
}

void runCurrentScene(void)
{
    scenes[currentScene]();
}

void sceneSignalEnd(void)
{
    if (onSceneEnd)
        onSceneEnd(currentScene);
}
