#include "raylib.h" // Main game library
#include <stdio.h> // File I/O for saving leaderboard
#include <stdlib.h> // Standard library functions
#include <time.h> // Used for timestamps (saving scores with time)
#include <string.h> // String manipulation
#include <math.h> // Used for calculations like screen shake and explosion particles

// Screen dimensions and game constants
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 750
#define MAX_NUMBERS 10
#define MAX_BULLETS 10
#define BOX_SIZE 40
#define BULLET_SPEED 10
#define PLAYER_SPEED 6
#define COOLDOWN_TIME 0.3f
#define MAX_LEVEL 3
#define LEVEL_UP_SCORE 10
#define MAX_STARS 100
#define MAX_PARTICLES 50
#define MAX_LEADERBOARD_ENTRIES 100
#define LEADERBOARD_FILE "leaderboard.txt"
#ifndef SILVER
#define SILVER (Color){ 192, 192, 192, 255 }
#define BRONZE (Color){ 205, 127, 50, 255 }
#define MAX_HEALTH 5
#endif

// Data Structures
typedef struct {
    char playerName[32];
    int score;
    time_t timestamp;  // For leaderboard sorting
} LeaderboardEntry;

typedef struct {
    Vector2 position;
    float speed;
    float size;
} Star;

typedef struct {
    Vector2 position;
    Vector2 velocity;
    float lifetime;
    Color color;
} Particle;

typedef struct {
    int value;
    Vector2 position;
    Vector2 speed;
    bool active;
} FallingNumber;

typedef struct {
    Vector2 position;
    Vector2 speed;
    bool active;
} Bullet;

typedef struct {
    Vector2 position;
    Vector2 speed;
    bool active;
} ShieldPowerUp;

typedef struct {
    Vector2 position;
    int score;
    int health;
    int level;
    int nextLevelScore;
    // Shield
    int shieldTimeRemaining; // in frames or seconds
    bool shieldActive;
    float shieldTimer; // to track how long the shield has been active
} Player;

typedef enum { MENU, ENTER_NAME, INSTRUCTIONS, GAMEPLAY, LEADERBOARD, GAMEOVER } GameState;

// Main game state structure 
typedef struct {
    // Game objects
    FallingNumber numbers[MAX_NUMBERS];
    Bullet bullets[MAX_BULLETS];
    Player player;
    Star stars[MAX_STARS];
    Particle particles[MAX_PARTICLES];
    LeaderboardEntry leaderboard[MAX_LEADERBOARD_ENTRIES];

    // Game state
    int leaderboardCount;
    float bulletTimer;
    float spawnTimer;
    float spawnInterval;
    bool gameOver;
    float shakeAmount;
    Vector2 cameraOffset;
    char playerName[32];
    GameState gameState;
    bool shieldSpawnedThisLevel;
    int shotsThisLevel;
    bool gameWon;

    // Sounds
    Sound hitEvenSound;
    Sound hitOddSound;
    Sound gameOverSound;
    Sound winSound;
    Music bgMusic;

    // Fonts
    Font gameFont;

    // Shield Power
    ShieldPowerUp shieldPowerUp;
    Texture2D shieldTexture; // image for shield power-up

    // Heart
    Texture2D heartFull;
    Texture2D heartEmpty;

} GameData;

// Function declarations - now all take GameData* parameter
void LoadLeaderboard(GameData* game);
int main(void);
void InitGame(GameData* game);
void InitStars(GameData* game);
void UpdateGame(GameData* game);
void ShootBullet(GameData* game);
void UpdateBullets(GameData* game);
void SpawnNumber(GameData* game);
void UpdateNumbers(GameData* game);
void CheckCollisions(GameData* game);
void DrawGame(GameData* game);
void SaveScore(GameData* game);
void DrawMenu(GameData* game);
void DrawInstructions(GameData* game);
int CompareScores(const void* a, const void* b);
void DrawLeaderboard(GameData* game);
void DrawEnterName(GameData* game);
void DrawGameOver(GameData* game);
void CreateExplosion(GameData* game, Vector2 position, Color color);
void DrawTextExCustom(Font font, const char* text, Vector2 position, float fontSize, float spacing, Color tint);

void LoadLeaderboard(GameData* game) {
    FILE* file = fopen("leaderboard.dat", "rb");
    if (file) {
        fread(game->leaderboard, sizeof(LeaderboardEntry), MAX_LEADERBOARD_ENTRIES, file);
        fclose(file);
    }
}

int main(void) {
    // Initialize window and audio
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Even Number Shooter");
    InitAudioDevice();

    // Create single game data structure
    GameData game = {0}; // Initialize all members to 0
    
    // Load font 
    game.gameFont = LoadFontEx("fonts/arial.ttf", 32, 0, 250);
    if (game.gameFont.texture.id == 0) {
        TraceLog(LOG_WARNING, "Failed to load custom font, using default");
        game.gameFont = GetFontDefault();
    }

    // Load sounds/music
    game.hitEvenSound = LoadSound("sounds/hit_even.mp3");
    game.hitOddSound = LoadSound("sounds/hit_odd.mp3");
    game.gameOverSound = LoadSound("sounds/game_over.mp3");
    game.winSound = LoadSound("sounds/win.mp3");
    game.bgMusic = LoadMusicStream("sounds/bg_music.mp3");

    // Load Shield Image
    game.shieldTexture = LoadTexture("textures/shield.png");

    // Load heart
    game.heartFull = LoadTexture("textures/heart_full.png");
    game.heartEmpty = LoadTexture("textures/heart_empty.png");

    // Load leaderboard at startup
    LoadLeaderboard(&game);
    
    PlayMusicStream(game.bgMusic);
    SetTargetFPS(60);
    srand(time(NULL));
    
    game.gameState = MENU;
    InitStars(&game);
    
    while (!WindowShouldClose()) {
        UpdateMusicStream(game.bgMusic);
        
        // Dynamic background gradient
        static float colorTime = 0;
        colorTime += GetFrameTime() * 0.5f; // hue value increases each frame
        Color bgTop = ColorFromHSV(fmod(colorTime * 20, 360), 0.7f, 0.9f); // fmod: loop the color wheel between 0 to 360
        Color bgBottom = ColorFromHSV(fmod(colorTime * 20 + 60, 360), 0.7f, 0.6f); //0.7f = saturation, 0.9f = brightness, changes the Hue, cycle around the color wheel (0 to 360).
        
        BeginDrawing();
        DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, bgTop, bgBottom);

        switch (game.gameState) {
            case MENU:
                DrawMenu(&game);
                break;
            case ENTER_NAME:
                DrawEnterName(&game);
                break;
            case INSTRUCTIONS:
                DrawInstructions(&game);
                break;
            case GAMEPLAY:
                if (!game.gameOver) {
                    UpdateGame(&game);
                    DrawGame(&game);
                    
                    if (game.player.health <= 0) {
                        game.gameOver = true;
                        PlaySound(game.gameOverSound);
                        SaveScore(&game);
                    }
                } else {
                    DrawGameOver(&game);
                }
                break;
            case LEADERBOARD:
                DrawLeaderboard(&game);
                break;
        }
        
        EndDrawing();
    }
    
    // Cleanup
    UnloadSound(game.hitEvenSound);
    UnloadSound(game.hitOddSound);
    UnloadSound(game.gameOverSound);
    UnloadSound(game.winSound);
    UnloadMusicStream(game.bgMusic);
    UnloadTexture(game.shieldTexture);
    UnloadTexture(game.heartFull);
    UnloadTexture(game.heartEmpty);
    UnloadFont(game.gameFont);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}

void InitGame(GameData* game) {
    game->player.position = (Vector2){ SCREEN_WIDTH/2 - 20, SCREEN_HEIGHT - 50 };
    game->player.score = 0;
    game->player.health = 5;
    game->player.shieldTimeRemaining = 0;
    game->player.shieldActive = false;
    game->player.shieldTimer = 0.0f;
    game->shieldPowerUp.active = false;
    game->player.level = 1;
    game->player.nextLevelScore = LEVEL_UP_SCORE;
    game->shotsThisLevel = 0;
    game->gameWon = false;

    for (int i = 0; i < MAX_NUMBERS; i++) game->numbers[i].active = false;
    for (int i = 0; i < MAX_BULLETS; i++) game->bullets[i].active = false;
    for (int i = 0; i < MAX_PARTICLES; i++) game->particles[i].lifetime = 0;

    game->bulletTimer = 0.0f;
    game->spawnTimer = 0.0f;
    game->spawnInterval = 1.0f;
    game->gameOver = false;
    game->shieldSpawnedThisLevel = false;
    game->shakeAmount = 0;
    game->shieldPowerUp.active = false;
    
    InitStars(game);
}

void InitStars(GameData* game) {
    for (int i = 0; i < MAX_STARS; i++) {
        game->stars[i].position = (Vector2){
            GetRandomValue(0, SCREEN_WIDTH),
            GetRandomValue(0, SCREEN_HEIGHT)
        };
        game->stars[i].speed = GetRandomValue(20, 100)/100.0f;
        game->stars[i].size = GetRandomValue(1, 3)/2.0f;
    }
}

void UpdateGame(GameData* game) {
    // Player movement
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) game->player.position.x -= PLAYER_SPEED;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) game->player.position.x += PLAYER_SPEED;

    if (game->player.position.x < 0) game->player.position.x = 0;
    if (game->player.position.x > SCREEN_WIDTH - 40) game->player.position.x = SCREEN_WIDTH - 40;

    // Shooting bullets
    game->bulletTimer += GetFrameTime();
    if (IsKeyPressed(KEY_SPACE) && game->bulletTimer >= COOLDOWN_TIME) {
        ShootBullet(game);
        game->bulletTimer = 0.0f;
        game->shotsThisLevel++;
    }

    // Spawning numbers
    game->spawnTimer += GetFrameTime();
    if (game->spawnTimer >= game->spawnInterval) {
        SpawnNumber(game);
        game->spawnTimer = 0.0f;
    }

    // Spawn the falling power-up
    if (!game->shieldPowerUp.active && !game->shieldSpawnedThisLevel && GetRandomValue(0, 1000) < 2) {
        game->shieldPowerUp.active = true;
        game->shieldSpawnedThisLevel = true;
        game->shieldPowerUp.position = (Vector2){ GetRandomValue(50, SCREEN_WIDTH - 50), -40 };
        game->shieldPowerUp.speed = (Vector2){ 0, 2.0f };
    }

    // Move shield power-up
    if (game->shieldPowerUp.active) {
        game->shieldPowerUp.position.y += game->shieldPowerUp.speed.y;
        if (game->shieldPowerUp.position.y > SCREEN_HEIGHT) {
            game->shieldPowerUp.active = false;
        }
    }

    // Update stars
    for (int i = 0; i < MAX_STARS; i++) {
        game->stars[i].position.y += game->stars[i].speed;
        if (game->stars[i].position.y > SCREEN_HEIGHT) {
            game->stars[i].position = (Vector2){
                GetRandomValue(0, SCREEN_WIDTH),
                -10
            };
        }
    }

    // Update particles
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (game->particles[i].lifetime > 0) {
            game->particles[i].position.x += game->particles[i].velocity.x;
            game->particles[i].position.y += game->particles[i].velocity.y;
            game->particles[i].lifetime -= GetFrameTime();
        }
    }

    if (game->player.shieldActive) {
        game->player.shieldTimer += GetFrameTime();
        if (game->player.shieldTimer >= 10.0f) {
            game->player.shieldActive = false;
            game->player.shieldTimer = 0.0f;
        }
    }

    // Check for win condition: 10 shots in level 3
    if (game->player.level == 3 && game->player.score >= 30) {
        game->gameWon = true;
        game->gameOver = true;
        PlaySound(game->winSound);
        SaveScore(game);
        PlayMusicStream(game->bgMusic); // Ensures it's playing
    }

    UpdateBullets(game);
    UpdateNumbers(game);
    CheckCollisions(game);
}

void ShootBullet(GameData* game) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!game->bullets[i].active) {
            game->bullets[i].position = (Vector2){ game->player.position.x + 20, game->player.position.y };
            game->bullets[i].speed = (Vector2){ 0, -BULLET_SPEED };
            game->bullets[i].active = true;
            break;
        }
    }
}

void UpdateBullets(GameData* game) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (game->bullets[i].active) {
            game->bullets[i].position.y += game->bullets[i].speed.y;
            if (game->bullets[i].position.y < 0) game->bullets[i].active = false;
        }
    }
}

void SpawnNumber(GameData* game) { //  create the falling number boxes randomly
    for (int i = 0; i < MAX_NUMBERS; i++) {
        if (!game->numbers[i].active) {
            game->numbers[i].value = GetRandomValue(1, 99);
            game->numbers[i].position = (Vector2){ GetRandomValue(50, SCREEN_WIDTH - 50), -BOX_SIZE };
            game->numbers[i].speed = (Vector2){ 0, 1.5f };
            game->numbers[i].active = true;
            break;
        }
    }
}

void UpdateNumbers(GameData* game) { // move them down the screen
    for (int i = 0; i < MAX_NUMBERS; i++) {
        if (game->numbers[i].active) {
            // Increase speed based on level
            float levelSpeedFactor = 1.0f + (game->player.level * 0.3f);
            game->numbers[i].position.y += game->numbers[i].speed.y * levelSpeedFactor;

            if (game->numbers[i].position.y > SCREEN_HEIGHT) {
                if (game->numbers[i].value % 2 == 0) {
                    if (game->player.shieldActive) {
                        // shield absorbs damage
                    } else {
                        game->player.health--;
                        game->shakeAmount = 5.0f;
                    }
                }
                game->numbers[i].active = false;
            }
        }
    }
}

void CheckCollisions(GameData* game) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!game->bullets[i].active) continue;

        for (int j = 0; j < MAX_NUMBERS; j++) {
            if (!game->numbers[j].active) continue;

            Rectangle boxRect = { game->numbers[j].position.x, game->numbers[j].position.y, BOX_SIZE, BOX_SIZE };
            Vector2 bulletPos = game->bullets[i].position;

            // Bullet hits shield image power-up
            if (game->shieldPowerUp.active) {
                Rectangle shieldRect = { game->shieldPowerUp.position.x, game->shieldPowerUp.position.y, BOX_SIZE, BOX_SIZE };
                if (CheckCollisionPointRec(bulletPos, shieldRect)) {
                    game->shieldPowerUp.active = false;
                    game->player.shieldActive = true;
                    game->player.shieldTimer = 0.0f;
                    CreateExplosion(game, game->shieldPowerUp.position, SKYBLUE);
                    game->bullets[i].active = false;
                    break;
                }
            }

            if (CheckCollisionPointRec(bulletPos, boxRect)) {
                if (game->numbers[j].value % 2 == 0) {
                    game->player.score += 1;
                    PlaySound(game->hitEvenSound);
                    CreateExplosion(game, game->numbers[j].position, GREEN);
                
                    // Check for level up
                    if (game->player.score >= game->player.nextLevelScore && game->player.level < MAX_LEVEL) {
                        game->player.level++;
                        game->player.nextLevelScore += LEVEL_UP_SCORE;
                        game->shieldSpawnedThisLevel = false;
                        game->shotsThisLevel = 0;
                        game->spawnInterval = fmax(0.3f, 1.0f - (game->player.level * 0.2f));
                        // Add 1 health when level up, but only if it is not full
                        if (game->player.health < MAX_HEALTH) {
                            game->player.health++;
                        }
                        game->shieldSpawnedThisLevel = false;
                        game->shotsThisLevel = 0;  // Reset shots for new level
                    }
                } else {
                    game->player.score -= game->player.level;
                    PlaySound(game->hitOddSound); 
                    CreateExplosion(game, game->numbers[j].position, RED);
                    game->shakeAmount = 3.0f;
                }

                game->bullets[i].active = false;
                game->numbers[j].active = false;
                break;
            }
        }
    }
}

void DrawGame(GameData* game) {
    // Calculate screen shake
    Vector2 shakeOffset = {0};
    if (game->shakeAmount > 0) {
        shakeOffset = (Vector2){
            GetRandomValue(-game->shakeAmount, game->shakeAmount),
            GetRandomValue(-game->shakeAmount, game->shakeAmount)
        };
        game->shakeAmount *= 0.9f;
    }

    // Draw starfield background
    ClearBackground(BLACK);
    for (int i = 0; i < MAX_STARS; i++) {
        DrawCircleV(game->stars[i].position, game->stars[i].size, 
            ColorAlpha(WHITE, game->stars[i].speed));
    }

    // Draw particles (with shake)
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (game->particles[i].lifetime > 0) {
            DrawCircleV((Vector2){game->particles[i].position.x + shakeOffset.x,
                                 game->particles[i].position.y + shakeOffset.y}, 
                       game->particles[i].lifetime*2, 
                       ColorAlpha(game->particles[i].color, game->particles[i].lifetime));
        }
    }
    
    // Draw player rectangle (with shake)
    DrawRectangleV((Vector2){game->player.position.x + shakeOffset.x,
                           game->player.position.y + shakeOffset.y},
                 (Vector2){40, 20}, BLACK);

    // Draw bullets
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (game->bullets[i].active) {
            DrawCircleV((Vector2){game->bullets[i].position.x + shakeOffset.x,
                                game->bullets[i].position.y + shakeOffset.y},
                      5, RED);
        }
    }

    // Draw falling numbers
    for (int i = 0; i < MAX_NUMBERS; i++) {
        if (game->numbers[i].active) {
            Color boxColor = game->player.level >= 2 ? SKYBLUE : LIGHTGRAY;
            boxColor = game->player.level >= 3 ? MAGENTA : boxColor;
            
            DrawRectangleV((Vector2){game->numbers[i].position.x + shakeOffset.x,
                                   game->numbers[i].position.y + shakeOffset.y},
                         (Vector2){BOX_SIZE, BOX_SIZE}, boxColor);
            DrawText(TextFormat("%d", game->numbers[i].value),
                    (int)(game->numbers[i].position.x + 10 + shakeOffset.x),
                    (int)(game->numbers[i].position.y + 10 + shakeOffset.y),
                    20, BLACK);
        }
    }

    // Draw shield power-up image
    if (game->shieldPowerUp.active) {
        DrawTexture(game->shieldTexture,
            (int)(game->shieldPowerUp.position.x + shakeOffset.x),
            (int)(game->shieldPowerUp.position.y + shakeOffset.y),
            WHITE);
    }

    // UI elements (no shake)
    DrawText(TextFormat("Player: %s", game->playerName), 10, 10, 20, DARKGRAY);
    DrawText(TextFormat("Score: %d", game->player.score), 10, 40, 20, BLACK);
    for (int i = 0; i < 5; i++) {
        Texture2D tex = (i < game->player.health) ? game->heartFull : game->heartEmpty;
        DrawTexture(tex, 10 + i * 40, 70, WHITE);
    }
    DrawText(TextFormat("Level: %d/%d", game->player.level, MAX_LEVEL), 10, 110, 20, DARKBLUE);

    if (game->player.level <= MAX_LEVEL) {
        // Level progress bar: always based on 10 points per level
        int levelBaseScore = (game->player.level - 1) * LEVEL_UP_SCORE;
        float progress = (float)(game->player.score - levelBaseScore) / (float)LEVEL_UP_SCORE;

        DrawRectangle(10, 140, (int)(200 * progress), 10, GREEN);
        DrawRectangleLines(10, 140, 200, 10, DARKGRAY);
    }

    // Draw time-based shield timer
    if (game->player.shieldActive) {
        float timeLeft = 10.0f - game->player.shieldTimer;
        DrawText(TextFormat("Shield: %.1f sec", timeLeft), 10, 160, 20, BLUE);
    }

    // Controls hint
    DrawText("Move: A/D or Left/Right Arrow   Shoot: SPACE", 10, SCREEN_HEIGHT - 30, 20, DARKGRAY);
}

void SaveScore(GameData* game) {
    // Save to leaderboard file
    FILE *leaderboardFile = fopen(LEADERBOARD_FILE, "a");
    if (leaderboardFile) {
        time_t t = time(NULL);
        fprintf(leaderboardFile, "%s %d %ld\n", game->playerName, game->player.score, t);
        fclose(leaderboardFile);
    }

    // Add to in-memory leaderboard
    if (game->leaderboardCount < MAX_LEADERBOARD_ENTRIES) {
        strcpy(game->leaderboard[game->leaderboardCount].playerName, game->playerName);
        game->leaderboard[game->leaderboardCount].score = game->player.score;
        game->leaderboard[game->leaderboardCount].timestamp = time(NULL);
        game->leaderboardCount++;
    }
}

void DrawMenu(GameData* game) {
    const char* title = "Even Number Shooter";
    float titleSize = 40;
    Vector2 textSize = MeasureTextEx(game->gameFont, title, titleSize, 2);
    DrawTextExCustom(game->gameFont, title, 
                   (Vector2){SCREEN_WIDTH/2 - textSize.x/2, 100}, 
                   titleSize, 2, DARKBLUE);

    // Buttons
    float btnWidth = 350;
    float btnHeight = 60;
    float btnX = SCREEN_WIDTH/2 - btnWidth/2;
    float btnSpacing = 80;
    float btnTextSize = 32;
    
    Rectangle playBtn = { btnX, 200, btnWidth, btnHeight };
    Rectangle instrBtn = { btnX, 200 + btnSpacing, btnWidth, btnHeight };
    Rectangle leadBtn = { btnX, 200 + btnSpacing*2, btnWidth, btnHeight };

    // Play Button
    DrawRectangleRec(playBtn, LIGHTGRAY);
    DrawRectangleLinesEx(playBtn, 3, DARKGRAY);
    const char* playText = "PLAY";
    Vector2 playTextSize = MeasureTextEx(game->gameFont, playText, btnTextSize, 1);
    DrawTextExCustom(game->gameFont, playText,
                   (Vector2){playBtn.x + (playBtn.width - playTextSize.x)/2,
                            playBtn.y + (playBtn.height - btnTextSize)/2},
                   btnTextSize, 1, BLACK);

    // Instructions Button
    DrawRectangleRec(instrBtn, LIGHTGRAY);
    DrawRectangleLinesEx(instrBtn, 3, DARKGRAY);
    const char* instrText = "INSTRUCTIONS";
    Vector2 instrTextSize = MeasureTextEx(game->gameFont, instrText, btnTextSize, 1);
    DrawTextExCustom(game->gameFont, instrText,
                   (Vector2){instrBtn.x + (instrBtn.width - instrTextSize.x)/2,
                            instrBtn.y + (instrBtn.height - btnTextSize)/2},
                   btnTextSize, 1, BLACK);

    // Leaderboard Button
    DrawRectangleRec(leadBtn, LIGHTGRAY);
    DrawRectangleLinesEx(leadBtn, 3, DARKGRAY);
    const char* leadText = "LEADERBOARD";
    Vector2 leadTextSize = MeasureTextEx(game->gameFont, leadText, btnTextSize, 1);
    DrawTextExCustom(game->gameFont, leadText,
                   (Vector2){leadBtn.x + (leadBtn.width - leadTextSize.x)/2,
                            leadBtn.y + (leadBtn.height - btnTextSize)/2},
                   btnTextSize, 1, BLACK);

    // Button interactions
    Vector2 mousePos = GetMousePosition();
    if (CheckCollisionPointRec(mousePos, playBtn)) {
        DrawRectangleRec(playBtn, ColorAlpha(SKYBLUE, 0.3f));
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            game->gameState = ENTER_NAME;
            memset(game->playerName, 0, sizeof(game->playerName));
        }
    }
    if (CheckCollisionPointRec(mousePos, instrBtn)) {
        DrawRectangleRec(instrBtn, ColorAlpha(SKYBLUE, 0.3f));
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            game->gameState = INSTRUCTIONS;
        }
    }
    if (CheckCollisionPointRec(mousePos, leadBtn)) {
        DrawRectangleRec(leadBtn, ColorAlpha(SKYBLUE, 0.3f));
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            game->gameState = LEADERBOARD;
        }
    }
}

void DrawInstructions(GameData* game) {
    // Title
    const char* title = "Instructions";
    float titleSize = 48;
    Vector2 titleMeasure = MeasureTextEx(game->gameFont, title, titleSize, 2);
    DrawTextExCustom(game->gameFont, title,
                   (Vector2){SCREEN_WIDTH/2 - titleMeasure.x/2, 50},
                   titleSize, 2, DARKBLUE);

    // Instructions text
    float textSize = 28;
    float yPos = 150;
    float lineSpacing = 40;
    
    const char* instructions[] = {
        "Shoot falling even numbers to gain points.",
        "Avoid shooting odd numbers or you lose points.",
        "If an even number reaches the bottom, you lose health.",
        "Use the shield for missed even numbers to avoid lose health.",
        "Use A/D or Left/Right arrow keys to move.",
        "Press SPACE to shoot."
    };
    
    for (int i = 0; i < 5; i++) {
        Vector2 textMeasure = MeasureTextEx(game->gameFont, instructions[i], textSize, 1);
        DrawTextExCustom(game->gameFont, instructions[i],
                       (Vector2){SCREEN_WIDTH/2 - textMeasure.x/2, yPos + (i * lineSpacing)},
                       textSize, 1, BLACK);
    }

    // Back Button (same as in Leaderboard)
    Rectangle backBtn = { SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT - 80, 200, 50 };
    DrawRectangleRec(backBtn, LIGHTGRAY);
    DrawRectangleLinesEx(backBtn, 3, DARKGRAY);
    
    const char* backText = "Back";
    Vector2 backTextMeasure = MeasureTextEx(game->gameFont, backText, 28, 1);
    DrawTextExCustom(game->gameFont, backText,
                   (Vector2){backBtn.x + (backBtn.width - backTextMeasure.x)/2,
                            backBtn.y + (backBtn.height - 28)/2},
                   28, 1, BLACK);

    // Button interaction
    Vector2 mousePos = GetMousePosition();
    if (CheckCollisionPointRec(mousePos, backBtn)) {
        DrawRectangleRec(backBtn, ColorAlpha(SKYBLUE, 0.3f));
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            game->gameState = MENU;
        }
    }
}

// Comparison function for qsort 
int CompareScores(const void* a, const void* b) {
    LeaderboardEntry* entryA = (LeaderboardEntry*)a;
    LeaderboardEntry* entryB = (LeaderboardEntry*)b;
    
    // First sort by score (descending)
    if (entryA->score != entryB->score) {
        return entryB->score - entryA->score;
    }
    // If scores are equal, sort by timestamp (ascending - older entries first)
    return (int)(entryA->timestamp - entryB->timestamp);
}

void DrawLeaderboard(GameData* game) {
    // Title
    const char* title = "LEADERBOARD";
    float titleSize = 40;
    Vector2 titleMeasure = MeasureTextEx(game->gameFont, title, titleSize, 2);
    DrawTextEx(game->gameFont, title,
             (Vector2){SCREEN_WIDTH/2 - titleMeasure.x/2, 30},
             titleSize, 2, DARKBLUE);

    // Sort the leaderboard
    qsort(game->leaderboard, game->leaderboardCount, sizeof(LeaderboardEntry), CompareScores);

    // Display entries
    float yPos = 100;
    float entryHeight = 30;
    int entriesPerPage = 10;
    static int page = 0;
    
    if (game->leaderboardCount == 0) {
        DrawTextEx(game->gameFont, "No entries yet!", 
                 (Vector2){50, yPos}, 24, 1, BLACK);
    } else {
        int startIdx = page * entriesPerPage;
        int endIdx = (page + 1) * entriesPerPage;
        if (endIdx > game->leaderboardCount) endIdx = game->leaderboardCount;

        for (int i = startIdx; i < endIdx; i++) {
        char entryText[128];
        struct tm *timeinfo = localtime(&game->leaderboard[i].timestamp);
        strftime(entryText, sizeof(entryText), "%Y-%m-%d %H:%M:%S", timeinfo);
        
        char fullText[256];
        // Use i+1 for display position (not affected by page)
        snprintf(fullText, sizeof(fullText), "%d. %s - %d (%s)",
                i+1, 
                game->leaderboard[i].playerName, 
                game->leaderboard[i].score, 
                entryText);

        // Highlight top 3 scores
        Color textColor = BLACK;
        if (i == 0) textColor = GOLD;
        else if (i == 1) textColor = SILVER;
        else if (i == 2) textColor = BRONZE;

        DrawTextEx(game->gameFont, fullText,
                 (Vector2){50, yPos + (i - startIdx) * entryHeight},
                 24, 1, textColor);
    }

        // Pagination controls 
        Rectangle prevBtn = {50, SCREEN_HEIGHT - 60, 100, 40};
        Rectangle nextBtn = {SCREEN_WIDTH - 150, SCREEN_HEIGHT - 60, 100, 40};

        // Check button clicks
        if (page > 0 && CheckCollisionPointRec(GetMousePosition(), prevBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            page--;
        }
        if (endIdx < game->leaderboardCount && CheckCollisionPointRec(GetMousePosition(), nextBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            page++;
        }
    }

    // Back Button 
    Rectangle backBtn = { SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT - 80, 200, 50 };
    DrawRectangleRec(backBtn, LIGHTGRAY);
    DrawRectangleLinesEx(backBtn, 3, DARKGRAY);
    
    const char* backText = "Back";
    Vector2 backTextMeasure = MeasureTextEx(game->gameFont, backText, 28, 1);
    DrawTextExCustom(game->gameFont, backText,
                   (Vector2){backBtn.x + (backBtn.width - backTextMeasure.x)/2,
                            backBtn.y + (backBtn.height - 28)/2},
                   28, 1, BLACK);

    // Button interaction
    Vector2 mousePos = GetMousePosition();
    if (CheckCollisionPointRec(mousePos, backBtn)) {
        DrawRectangleRec(backBtn, ColorAlpha(SKYBLUE, 0.3f));
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            game->gameState = MENU;
        }
    }
}

void DrawEnterName(GameData* game) {
    // Title
    const char* title = "Enter Your Name (max 31 chars)";
    float titleSize = 32;
    Vector2 titleMeasure = MeasureTextEx(game->gameFont, title, titleSize, 1);
    DrawTextExCustom(game->gameFont, title,
                   (Vector2){SCREEN_WIDTH/2 - titleMeasure.x/2, SCREEN_HEIGHT/2 - 80},
                   titleSize, 1, DARKBLUE);

    // Input box
    Rectangle inputBox = { SCREEN_WIDTH/2 - 160, SCREEN_HEIGHT/2 - 20, 320, 50 };
    DrawRectangleRec(inputBox, LIGHTGRAY);
    DrawRectangleLinesEx(inputBox, 3, DARKGRAY);
    
    // Player name text
    if (strlen(game->playerName) > 0) {
        Vector2 nameMeasure = MeasureTextEx(game->gameFont, game->playerName, 28, 1);
        DrawTextExCustom(game->gameFont, game->playerName,
                       (Vector2){inputBox.x + (inputBox.width - nameMeasure.x)/2,
                                inputBox.y + (inputBox.height - 28)/2},
                       28, 1, BLACK);
    }

    // Back Button
    Rectangle backBtn = { SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2 + 60, 200, 50 };
    DrawRectangleRec(backBtn, LIGHTGRAY);
    DrawRectangleLinesEx(backBtn, 3, DARKGRAY);
    
    const char* backText = "Back";
    Vector2 backTextMeasure = MeasureTextEx(game->gameFont, backText, 28, 1);
    DrawTextExCustom(game->gameFont, backText,
                   (Vector2){backBtn.x + (backBtn.width - backTextMeasure.x)/2,
                            backBtn.y + (backBtn.height - 28)/2},
                   28, 1, BLACK);

    // Prompt
    const char* prompt = "Press ENTER to start the game";
    float promptSize = 24;
    Vector2 promptMeasure = MeasureTextEx(game->gameFont, prompt, promptSize, 1);
    DrawTextExCustom(game->gameFont, prompt,
                   (Vector2){SCREEN_WIDTH/2 - promptMeasure.x/2, SCREEN_HEIGHT/2 + 120},
                   promptSize, 1, DARKGRAY);

    // Button interactions
    Vector2 mousePos = GetMousePosition();
    if (CheckCollisionPointRec(mousePos, backBtn)) {
        DrawRectangleRec(backBtn, ColorAlpha(SKYBLUE, 0.3f));
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            game->gameState = MENU;
        }
    }

    // Input handling
    int key = GetCharPressed();
    while (key > 0) {
        int len = strlen(game->playerName);
        if ((key >= 32) && (key <= 125) && len < 31) {
            game->playerName[len] = (char)key;
            game->playerName[len + 1] = '\0';
        }
        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        int len = strlen(game->playerName);
        if (len > 0) {
            game->playerName[len - 1] = '\0';
        }
    }

    // Check if the player presses Enter
    if (IsKeyPressed(KEY_ENTER) && strlen(game->playerName) > 0) {
        game->gameState = GAMEPLAY;
        InitGame(game);
    }
}

void DrawGameOver(GameData* game) {
    // Center the "GAME OVER" text
    const char* gameOverText;
    Color gameOverColor;

    if (game->gameWon) {
        gameOverText = "YOU WIN!";
        gameOverColor = GREEN;
    } else {
        gameOverText = "GAME OVER";
        gameOverColor = RED;
    }

    float gameOverSize = 40;
    Vector2 gameOverMeasure = MeasureTextEx(game->gameFont, gameOverText, gameOverSize, 1);
    DrawTextExCustom(game->gameFont, gameOverText,
                (Vector2){SCREEN_WIDTH / 2 - gameOverMeasure.x / 2, SCREEN_HEIGHT / 2 - 80},
                gameOverSize, 1, gameOverColor);

    // Center the "Final Score" text
    const char* finalScoreText = TextFormat("Final Score: %d", game->player.score);
    float finalScoreSize = 30;
    Vector2 finalScoreMeasure = MeasureTextEx(game->gameFont, finalScoreText, finalScoreSize, 1);
    DrawTextExCustom(game->gameFont, finalScoreText,
                   (Vector2){SCREEN_WIDTH / 2 - finalScoreMeasure.x / 2, SCREEN_HEIGHT / 2 - 10},
                   finalScoreSize, 1, BLACK);

    // Center the "Press R to Restart" text
    const char* restartText = "Press R to Restart";
    float restartSize = 20;
    Vector2 restartMeasure = MeasureTextEx(game->gameFont, restartText, restartSize, 1);
    DrawTextExCustom(game->gameFont, restartText,
                   (Vector2){SCREEN_WIDTH / 2 - restartMeasure.x / 2, SCREEN_HEIGHT / 2 + 50},
                   restartSize, 1, DARKGRAY);

    // Center the "Back" button
    float backButtonWidth = 100;
    float backButtonHeight = 40;
    Rectangle backButton = { SCREEN_WIDTH / 2 - backButtonWidth / 2, SCREEN_HEIGHT / 2 + 100, backButtonWidth, backButtonHeight };
    Color backColor = LIGHTGRAY;

    if (CheckCollisionPointRec(GetMousePosition(), backButton)) {
        backColor = GRAY;
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            game->gameState = MENU;
            game->gameOver = false;
            PlayMusicStream(game->bgMusic);  // Resume background music
        }
    }

    DrawRectangleRec(backButton, backColor);
    DrawRectangleLinesEx(backButton, 2, DARKGRAY);
    DrawText("Back", backButton.x + 25, backButton.y + 10, 20, BLACK);

    if (IsKeyPressed(KEY_R)) {
        InitGame(game);
        game->gameOver = false;
        PlayMusicStream(game->bgMusic);  // Resume background music
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        game->gameState = MENU;
        game->gameOver = false;
        PlayMusicStream(game->bgMusic);  // Resume background music
    }

    // UI elements with custom font
    float uiFontSize = 24;
    float uiSpacing = 10;
    
    // Player name
    const char* nameText = TextFormat("Player: %s", game->playerName);
    DrawTextExCustom(game->gameFont, nameText, 
                   (Vector2){20, 20}, uiFontSize, 1, DARKGRAY);

    // Score
    const char* scoreText = TextFormat("Score: %d", game->player.score);
    DrawTextExCustom(game->gameFont, scoreText,
                   (Vector2){20, 50}, uiFontSize, 1, BLACK);

    // Health
    const char* healthText = TextFormat("Health: %d", game->player.health);
    DrawTextExCustom(game->gameFont, healthText,
                   (Vector2){20, 80}, uiFontSize, 1, MAROON);

    // Level
    const char* levelText = TextFormat("Level: %d/%d", game->player.level, MAX_LEVEL);
    DrawTextExCustom(game->gameFont, levelText,
                   (Vector2){20, 110}, uiFontSize, 1, PURPLE);

    // Level progress bar (keep existing)
    if (game->player.level <= MAX_LEVEL) {
        float progress = (float)(game->player.score - (game->player.nextLevelScore - LEVEL_UP_SCORE * game->player.level)) / 
                        (float)(LEVEL_UP_SCORE * game->player.level);
        DrawRectangle(20, 140, (int)(200 * progress), 10, GREEN);
        DrawRectangleLines(20, 140, 200, 10, DARKGRAY);
    }
    
    // Controls hint
    const char* controlsText = "Move: A/D or Left/Right Arrow   Shoot: SPACE";
    DrawTextExCustom(game->gameFont, controlsText,
                   (Vector2){20, SCREEN_HEIGHT - 40}, 20, 1, DARKGRAY);
}

void CreateExplosion(GameData* game, Vector2 position, Color color) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (game->particles[i].lifetime <= 0) {
            game->particles[i].position = position;
            game->particles[i].velocity = (Vector2){
                GetRandomValue(-50, 50)/10.0f,
                GetRandomValue(-50, 50)/10.0f
            };
            game->particles[i].lifetime = GetRandomValue(10, 30)/10.0f;
            game->particles[i].color = color;
            break;
        }
    }
}

void DrawTextExCustom(Font font, const char* text, Vector2 position, float fontSize, float spacing, Color tint) {
    // Draw text twice with slight offset to create bold effect
    Vector2 shadowPos = {position.x + 1, position.y + 1};
    DrawTextEx(font, text, shadowPos, fontSize, spacing, BLACK);
    DrawTextEx(font, text, position, fontSize, spacing, tint);
}