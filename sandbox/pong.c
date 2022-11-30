#include "ignis/ignis.h"

#include <stdio.h>

typedef enum
{
    STATE_START,
    STATE_PREPARE,
    STATE_SERVE,
    STATE_GAME,
    STATE_GAMEOVER,
} State;

int main(int argc, char *argv[])
{
    IgnisInit(800, 600, "Pong");
    SetTargetFPS(60);

    Font font = LoadFont("resources/fonts/FiraMono-Regular.ttf");

    Vector2 player1ScorePos = { 175.0f, 550.0f };
    Vector2 player2ScorePos = { 625.0f, 550.0f };
    Color scoreTextColor = { 255, 255, 255, 255 };

    Vector2 paddle1Pos = { 0.0f, 0.0f };
    Vector2 paddle1Direction = { 0.0f, 0.0f };
    Vector2 paddle2Pos = { 780.0f, 0.0f };
    Vector2 paddle2Direction = { 0.0f, 0.0f };
    Vector2 paddleSize = { 20.0f, 75.0f };
    Vector2 paddleOrigin = { 0.0f, 0.0f };

    Color paddle1Color = { 255, 255, 255, 255 };
    Color paddle2Color = { 255, 255, 255, 255 };

    Vector2 ballPos = { 400.0f, 300.0f };
    Vector2 ballSize = { 15.0f, 15.0f };
    Vector2 ballOrigin = { 0.0f, 0.0f };
    Vector2 ballDirection = { 1.0f, 1.0f };
    float ballSpeed = 400.0f;
    Color ballColor = { 255, 255, 255, 255 };

    float paddleSpeed = 300.0f;

    int32_t serveTurn = 0;
    int32_t player1Score = 0;
    int32_t player2Score = 0;
    int32_t maxScore = 7;
    State currentState = STATE_START;

    float prevTime = GetCurrentTime();
    while (!IsWindowClosed())
    {
        BeginFrame();

        float currentTime = GetCurrentTime();
        float deltaTime = currentTime - prevTime;
        prevTime = currentTime;

        if (currentState == STATE_START)
        {
            if (IsKeyPressed(KEY_SPACE))
            {
                serveTurn = 0;
                player1Score = 0;
                player2Score = 0;
                currentState = STATE_PREPARE;
            }
        }
        else if (currentState == STATE_PREPARE)
        {
            ballPos.x = 400.0f;
            ballPos.y = 300.0f;
            ballDirection.x = 0.0f;
            ballDirection.y = 0.0f;
            currentState = STATE_SERVE;
        }
        else if ((currentState == STATE_SERVE) || (currentState == STATE_GAME))
        {
            if (currentState == STATE_SERVE)
            {
                if (IsKeyPressed(KEY_SPACE))
                {
                    float ballDirY = GetRandomFloat(0.5f, 1.0f);
                    if (GetRandomInt(0, 1) == 0)
                    {
                        ballDirY *= -1.0f;
                    }
                    if (serveTurn == 0)
                    {
                        ballDirection.x = -1.0f;
                        ballDirection.y = ballDirY;
                    }
                    else
                    {
                        ballDirection.x = 1.0f;
                        ballDirection.y = ballDirY;
                    }

                    currentState = STATE_GAME;
                }
            }

            if (IsKeyDown(KEY_W))
            {
                paddle1Direction.y = 1.0f;
            }
            else if (IsKeyDown(KEY_S))
            {
                paddle1Direction.y = -1.0f;
            }
            else
            {
                paddle1Direction.y = 0.0f;
            }

            if (IsKeyDown(KEY_UP))
            {
                paddle2Direction.y = 1.0f;
            }
            else if (IsKeyDown(KEY_DOWN))
            {
                paddle2Direction.y = -1.0f;
            }
            else
            {
                paddle2Direction.y = 0.0f;
            }

            paddle1Pos.y += paddle1Direction.y * paddleSpeed * deltaTime;
            if (paddle1Pos.y < 0.0f)
            {
                paddle1Pos.y = 0.0f;
            }
            else if (paddle1Pos.y + paddleSize.y > 600.0f)
            {
                paddle1Pos.y = 600.0f - paddleSize.y;
            }

            paddle2Pos.y += paddle2Direction.y * paddleSpeed * deltaTime;
            if (paddle2Pos.y < 0.0f)
            {
                paddle2Pos.y = 0.0f;
            }
            else if (paddle2Pos.y + paddleSize.y > 600.0f)
            {
                paddle2Pos.y = 600.0f - paddleSize.y;
            }

            Vector2 ballVelocity = Vector2Scale(Vector2Normalize(ballDirection), ballSpeed * deltaTime);
            ballPos = Vector2Add(ballPos, ballVelocity);

            Rect paddle1Rect = { paddle1Pos.x, paddle1Pos.y, paddleSize.x, paddleSize.y };
            Rect paddle2Rect = { paddle2Pos.x, paddle2Pos.y, paddleSize.x, paddleSize.y };
            Rect ballRect = { ballPos.x, ballPos.y, ballSize.x, ballSize.y };
            if (CheckRectsOverlap(paddle1Rect, ballRect))
            {
                ballPos.x = paddle1Pos.x + paddleSize.x;
                ballDirection.x = -ballDirection.x;
            }
            else if (CheckRectsOverlap(paddle2Rect, ballRect))
            {
                ballPos.x = paddle2Pos.x - ballSize.x;
                ballDirection.x = -ballDirection.x;
            }

            if (ballPos.y <= 0.0f)
            {
                ballPos.y = 0.0f;
                ballDirection.y = -ballDirection.y;
            }
            if (ballPos.y + ballSize.y >= 600.0f)
            {
                ballPos.y = 600.0f - ballSize.y;
                ballDirection.y = -ballDirection.y;
            }

            if (ballPos.x <= 0.0f)
            {
                ++player2Score;
                serveTurn = 1;
                currentState = STATE_PREPARE;
            }
            else if (ballPos.x + ballSize.x >= 800.0f)
            {
                ++player1Score;
                serveTurn = 0;
                currentState = STATE_PREPARE;
            }

            if ((player1Score == maxScore) || (player2Score == maxScore))
            {
                currentState = STATE_GAMEOVER;
            }
        }
        else if (currentState == STATE_GAMEOVER)
        {
            if (IsKeyPressed(KEY_SPACE))
            {
                currentState = STATE_START;
            }
        }

        BeginDrawing();

        if (currentState == STATE_START)
        {
            Vector2 titlePos = { 230.0f, 290.0f };
            int titleFontSize = 128;
            Color titleColor = { 255, 255, 255, 255 };
            DrawText(titlePos, "PONG", font, titleFontSize, titleColor);

            Vector2 titleInstructionPos = { 220.0f, 250.0f };
            int titleInstructionFontSize = 30;
            DrawText(titleInstructionPos, "Press Space to play", font, titleInstructionFontSize, titleColor);
        }
        else if ((currentState == STATE_PREPARE) || (currentState == STATE_SERVE) || (currentState == STATE_GAME))
        {
            DrawRectangle(ballPos, ballSize, ballOrigin, ballColor);
            DrawRectangle(paddle1Pos, paddleSize, paddleOrigin, paddle1Color);
            DrawRectangle(paddle2Pos, paddleSize, paddleOrigin, paddle2Color);
            char buf[100];
            sprintf(buf, "%d", player1Score);
            DrawText(player1ScorePos, buf, font, 30, scoreTextColor);
            sprintf(buf, "%d", player2Score);
            DrawText(player2ScorePos, buf, font, 30, scoreTextColor);

            if (currentState == STATE_SERVE)
            {
                Vector2 p = { 300.0f, 20.0f };
                Color color = { 255, 255, 255, 255 };
                char buf2[100];
                sprintf(buf2, "Player %d serve", serveTurn + 1);
                DrawText(p, buf2, font, 24, color);
            }
        }
        else if (currentState == STATE_GAMEOVER)
        {
            Vector2 resultTextPos = { 310.0f, 325.0f };
            Vector2 instructionTextPos = { 160.0f, 275.0f };
            int fontSize = 24;
            Color color = { 255, 255, 255, 255 };

            int winningPlayer = (player1Score == maxScore) ? 1 : 2;
            char buf[100];
            sprintf(buf, "Player %d won!", winningPlayer);
            DrawText(resultTextPos, buf, font, fontSize, color);
            DrawText(instructionTextPos, "Press Space to go back to main menu", font, fontSize, color);
        }
        
        EndDrawing();
        Draw();

        EndFrame();
    }

    UnloadFont(&font);

    IgnisCleanup();

    return 0;
}
