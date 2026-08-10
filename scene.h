#ifndef SCENE_H
#define SCENE_H

/* =========================================================
   SCENE.H — Gestionnaire de scenes
   ========================================================= */

typedef enum {
    SCENE_0  = 0,
    SCENE_1  = 1,
    SCENE_2  = 2,
    SCENE_3  = 3,
    SCENE_4  = 4,
    SCENE_5  = 5,
    SCENE_6  = 6,
    SCENE_7  = 7,
    SCENE_8  = 8,
} Scene;

extern Scene currentScene;
extern unsigned long sceneStart;

/* Callback appelé par sceneSignalEnd() quand une scène se déclare
   terminée. L'implémentation (ex: main.c) décide quelle scène vient
   ensuite et avec quelle transition.
   Signature : void handler(Scene sceneQuiVientDeFinir); */
typedef void (*SceneEndHandler)(Scene);
extern SceneEndHandler onSceneEnd;

void setScene(Scene s);
void runCurrentScene(void);

/* Appelé par une scène pour signaler qu'elle est terminée.
   La scène ne choisit PAS la suivante : c'est onSceneEnd qui décide. */
void sceneSignalEnd(void);

#endif /* SCENE_H */
