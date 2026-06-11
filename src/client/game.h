#ifndef SHROOM_CLIENT_GAME_H
#define SHROOM_CLIENT_GAME_H

#include "raylib.h"

#include "net.h"
#include "shared/world.h"

typedef struct Game {
    Camera2D camera;
    ClientNetState net;
    ShroomWorldState world;
    ShroomPlayerState *local_player;
    int screen_width;
    int screen_height;
} Game;

void GameInit(Game *game, int screen_width, int screen_height);
void GameUpdate(Game *game, float delta_time);
void GameDraw(const Game *game);
void GameShutdown(Game *game);

#endif
