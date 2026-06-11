#include "game.h"

int main(void) {
    const int screen_width = 1280;
    const int screen_height = 720;
    Game game = {0};

    InitWindow(screen_width, screen_height, "shroomio");
    SetTargetFPS(60);
    GameInit(&game, screen_width, screen_height);

    while (!WindowShouldClose()) {
        GameUpdate(&game, GetFrameTime());
        GameDraw(&game);
    }

    GameShutdown(&game);
    CloseWindow();
    return 0;
}
