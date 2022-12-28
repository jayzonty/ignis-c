#include <ignis/ignis.h>

#include <stdio.h>

typedef enum
{
    STATE_GAME,
    STATE_GAMEOVER,
} State;

typedef struct
{
    Vector2 position;
    Vector2 size;
    float speed;
} Paddle;

typedef struct
{
    Vector2 position;
    float radius;

    Vector2 direction;
    float speed;
} Ball;

typedef struct Block
{
    bool isActive;
    Vector2 position;
    Vector2 size;
} Block;

int main()
{
    int screenWidth = 800;
    int screenHeight = 600;

    IgnisInit(screenWidth, screenHeight, "Breakout");

    {
        Color clearColor = { 0, 0, 0, 255 };
        SetClearColor(clearColor);
    }

    Font standardFont = LoadFont("resources/fonts/FiraMono-Regular.ttf");

    Key keyLeft = KEY_LEFT;
    Key keyRight = KEY_RIGHT;
    
    // --- Paddle properties ---
    Paddle paddle = { 0 };
    paddle.size.x = 100.0f;
    paddle.size.y = 15.0f;
    paddle.position.x = screenWidth / 2.0f - paddle.size.x / 2.0f;
    paddle.position.y = 50.0f;
    paddle.speed = 450.0f;
    Vector2 paddleOrigin = { 0.0f, 0.0f };
    Color paddleColor = { 255, 255, 255, 255 };

    Ball ball = { 0 };
    ball.position.x = paddle.position.x + paddle.size.x / 2.0f;
    ball.position.y = paddle.position.y + 50.0f;
    ball.radius = 7.0f;
    ball.speed = 400.0f;
    Vector2 ballOrigin = { 0.5f, 0.5f };
    Color ballColor = { 255, 255, 255, 255 };

    const int NUM_BLOCKS_X = 20;
    const int NUM_BLOCKS_Y = 14;
    const int totalBlockCount = NUM_BLOCKS_X * NUM_BLOCKS_Y;
    Vector2 blockSize = { 37.0f, 15.0f };
    Vector2 blockOrigin = { 0.0f, 0.0f };
    Color blockColor = { 150, 150, 150, 255 };
    Block blocks[NUM_BLOCKS_X * NUM_BLOCKS_Y];
    Vector2 blockSpacing = { 2.0f, 2.0f };
    for (int x = 0; x < NUM_BLOCKS_X; ++x)
    {
        for (int y = 0; y < NUM_BLOCKS_Y; ++y)
        {
            int index = y * NUM_BLOCKS_X + x;
            blocks[index].isActive = true;
            blocks[index].position.x = 10.0f + x * blockSize.x + x * blockSpacing.x;
            blocks[index].position.y = 300.0f + y * blockSize.y + y * blockSpacing.y;
            blocks[index].size = blockSize;
        }
    }

    float ballServeYOffset = 75.0f;
    bool isServing = true;

    const int maxLives = 5;
    int remainingLives = maxLives;

    State currentState = STATE_GAME;

    const char* GAME_OVER_TEXT = "Game Over!";
    const char* GAME_OVER_RESTART_INSTRUCTION_TEXT = "Press R to restart";
    const char* GAME_OVER_EXIT_INSTRUCTION_TEXT = "Press ESC to exit";

    while (!IsWindowClosed())
    {
        BeginFrame();

        float deltaTime = GetDeltaTime();

        if (currentState == STATE_GAME)
        {
            Vector2 paddleMovement = { 0.0f, 0.0f };
            if (IsKeyDown(keyLeft))
            {
                paddleMovement.x = -1.0f;
            }
            if (IsKeyDown(keyRight))
            {
                paddleMovement.x = 1.0f;
            }
            paddleMovement = Vector2Normalize(paddleMovement);
            paddle.position = Vector2Add(paddle.position, Vector2Scale(paddleMovement, paddle.speed *deltaTime));
            paddle.position.x = Clamp_Float(paddle.position.x, 0.0f, screenWidth - paddle.size.x);

            if (isServing)
            {
                ball.position = paddle.position;
                ball.position.x += paddle.size.x / 2.0f;
                ball.position.y += ballServeYOffset;
            }
            else
            {
                ball.position = Vector2Add(ball.position, Vector2Scale(ball.direction, ball.speed * deltaTime));
            }

            if (ball.position.x - ball.radius < 0.0f)
            {
                ball.position.x = ball.radius;
                ball.direction.x *= -1.0f;
            }
            else if (ball.position.x + ball.radius > screenWidth)
            {
                ball.position.x = screenWidth - ball.radius;
                ball.direction.x *= -1.0f;
            }
            if (ball.position.y + ball.radius > screenHeight)
            {
                ball.position.y = screenHeight - ball.radius;
                ball.direction.y *= -1.0f;
            }
            else if (ball.position.y < 0.0f)
            {
                ball.position.x = paddle.position.x + paddle.size.x / 2.0f;
                ball.position.y = paddle.position.y + ballServeYOffset;
                ball.direction.x = 0.0f;
                ball.direction.y = 0.0f;

                --remainingLives;
                if (remainingLives <= 0)
                {
                    currentState = STATE_GAMEOVER;
                }
                else
                {
                    isServing = true;
                }
            }

            Rect paddleRect = { paddle.position.x, paddle.position.y, paddle.size.x, paddle.size.y };
            if (CheckCircleRectOverlap(ball.position, ball.radius, paddleRect))
            {
                Vector2 Q = GetClosestRectPointFromPoint(ball.position, paddleRect);
                Vector2 collisionNormal = Vector2Subtract(ball.position, Q);

                ball.position = Vector2Add(Q, collisionNormal);

                Vector2 paddleCenter = Vector2Add(paddle.position, Vector2Scale(paddle.size, 0.5f));
                ball.direction = Vector2Normalize(Vector2Subtract(ball.position, paddleCenter));
            }

            for (int index = 0; index < totalBlockCount; ++index)
            {
                if (!blocks[index].isActive)
                {
                    continue;
                }

                Rect blockRect = { blocks[index].position.x, blocks[index].position.y, blocks[index].size.x, blocks[index].size.y };
                if (CheckCircleRectOverlap(ball.position, ball.radius, blockRect))
                {
                    Vector2 closestPoint = GetClosestRectPointFromPoint(ball.position, blockRect);

                    Vector2 normal = Vector2Subtract(closestPoint, ball.position);
                    normal = Vector2Normalize(normal);
                    ball.direction = Vector2Reflect(ball.direction, normal);

                    Vector2 nudge = Vector2Subtract(ball.position, closestPoint);
                    ball.position = Vector2Add(ball.position, nudge);

                    blocks[index].isActive = false;
                    break;
                }
            }

            if (isServing && IsKeyPressed(KEY_SPACE))
            {
                ball.direction.y = -1.0f;
                isServing = false;
            }

            BeginDrawing();
            DrawRectangle(paddle.position, paddle.size, paddleOrigin, paddleColor, 0.0f);
            DrawCircle(ball.position, ball.radius, ballOrigin, ballColor, 1.0f);
            for (int x = 0; x < NUM_BLOCKS_X; ++x)
            {
                for (int y = 0; y < NUM_BLOCKS_Y; ++y)
                {
                    int index = y * NUM_BLOCKS_X + x;
                    if (blocks[index].isActive)
                    {
                        DrawRectangle(blocks[index].position, blocks[index].size, blockOrigin, blockColor, 0.0f);
                    }
                }
            }

            // Draw remaining lives
            for (int i = 0; i < remainingLives; ++i)
            {
                float lifeIndicatorRadius = 5.0f;
                Vector2 lifeIndicatorOrigin = { 0.5f, 0.5f };
                Color lifeIndicatorColor = { 0, 0, 200, 255 };
                if (remainingLives == 1)
                {
                    lifeIndicatorColor.r = 200;
                    lifeIndicatorColor.g = 0;
                    lifeIndicatorColor.b = 0;
                }
                else if (remainingLives <= 3)
                {
                    lifeIndicatorColor.r = 200;
                    lifeIndicatorColor.g = 200;
                    lifeIndicatorColor.b = 0;
                }
                Vector2 pos = { 10.0f + 5.0f * i + lifeIndicatorRadius * 2.0f * i, screenHeight - 10.0f - lifeIndicatorRadius };
                DrawCircle(pos, lifeIndicatorRadius, lifeIndicatorOrigin, lifeIndicatorColor, 1.0f);
            }

            if (isServing)
            {
                Vector2 pos = { 10.0f, 10.0f };
                Color color = { 255, 255, 0, 255 };
                DrawText(pos, "Press [SPACE] to serve", standardFont, 18, color, 1.0f);
            }

            EndDrawing();
            Draw();
        }
        else if (currentState == STATE_GAMEOVER)
        {
            if (IsKeyPressed(KEY_ESCAPE))
            {
                CloseWindow();
            }
            else if (IsKeyPressed(KEY_R))
            {
                currentState = STATE_GAME;

                remainingLives = maxLives;
                for (int index = 0; index < totalBlockCount; ++index)
                {
                    blocks[index].isActive = true;
                }

                isServing = true;
            }
            else
            {
                const int GAME_OVER_TEXT_FONT_SIZE = 40;
                const int GAME_OVER_INSTRUCTION_FONT_SIZE = 26;
                const Color TEXT_COLOR = { 255, 255, 255, 255 };
                Vector2 gameOverTextSize = MeasureTextSize(GAME_OVER_TEXT, standardFont, GAME_OVER_TEXT_FONT_SIZE);
                Vector2 gameOverTextPosition = { screenWidth / 2.0f - gameOverTextSize.x / 2.0f, screenHeight / 2.0f + 25.0f };

                Vector2 gameOverRestartInstructionTextSize = MeasureTextSize(GAME_OVER_RESTART_INSTRUCTION_TEXT, standardFont, GAME_OVER_INSTRUCTION_FONT_SIZE);
                Vector2 gameOverRestartInstructionTextPosition = { screenWidth / 2.0f - gameOverRestartInstructionTextSize.x / 2.0f, screenHeight / 2.0f - 25.0f };

                Vector2 gameOverExitInstructionTextSize = MeasureTextSize(GAME_OVER_EXIT_INSTRUCTION_TEXT, standardFont, GAME_OVER_INSTRUCTION_FONT_SIZE);
                Vector2 gameOverExitInstructionTextPosition = { screenWidth / 2.0f - gameOverExitInstructionTextSize.x / 2.0f, screenHeight / 2.0f - 50.0f };

                BeginDrawing();
                DrawText(gameOverTextPosition, GAME_OVER_TEXT, standardFont, GAME_OVER_TEXT_FONT_SIZE, TEXT_COLOR, 0.0f);
                DrawText(gameOverRestartInstructionTextPosition, GAME_OVER_RESTART_INSTRUCTION_TEXT, standardFont, GAME_OVER_INSTRUCTION_FONT_SIZE, TEXT_COLOR, 0.0f);
                DrawText(gameOverExitInstructionTextPosition, GAME_OVER_EXIT_INSTRUCTION_TEXT, standardFont, GAME_OVER_INSTRUCTION_FONT_SIZE, TEXT_COLOR, 0.0f);
                EndDrawing();
                Draw();
            }
        }

        EndFrame();
    }

    UnloadFont(&standardFont);

    IgnisCleanup();

    return 0;
}

