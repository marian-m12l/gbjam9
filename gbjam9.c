#include <gb/console.h>
#include <gb/gb.h>
#include <gb/metasprites.h>
#include <rand.h>
#include <stdio.h>
#include <stdlib.h>

#include "metasprites/bird.h"

#include "tilesets/food_tiles.h"
#include "tilesets/title_tiles.h"
#include "tilesets/title_map.h"
#include "tilesets/instructions_tiles.h"
#include "tilesets/instructions_map.h"
#include "tilesets/sky_tiles.h"
#include "tilesets/sky_map.h"
#include "tilesets/winning_tiles.h"
#include "tilesets/winning_map.h"
#include "tilesets/font_tiles.h"
#include "tilesets/pause_tiles.h"


#define SHOW_FPS 0


// Pause sprite tiles are loaded into VRAM
#define PAUSE_TILE_NUM_START 0
// Pause sprite will be built starting with hardware sprite 0
#define PAUSE_SPR_NUM_START 0

// Character metasprite tiles are loaded into VRAM after pause tiles
#define BIRD_TILE_NUM_START (PAUSE_TILE_NUM_START + 5)
// Character metasprite will be built starting with hardware sprite 5 (after pause sprites)
#define BIRD_SPR_NUM_START (PAUSE_SPR_NUM_START + 5)

#define BIRD_SPRITE_GLIDING 3
#define BIRD_SPRITE_FLAPPING_START 0
#define BIRD_SPRITE_FLAPPING_END 7
#define BIRD_SPRITE_DIVING_START 8
#define BIRD_SPRITE_DIVING_END 10

#define INITIAL_SPEED_X 1
#define MAX_SPEED_X 12
#define INITIAL_SPEED_Y 0
#define MAX_SPEED_Y_GLIDING 4
#define MAX_SPEED_Y_DIVING 24
#define MAX_SPEED_Y_UPWARDS -32
#define SPEED_Y_BOOST_FLAPPING -32
#define SPEED_Y_BOOST_DIVING 28

#define MIN_POS_X 20
#define MAX_POS_X 156
#define MIN_POS_Y 16
#define MAX_POS_Y 144

#define MAX_FOOD 8
// Food metasprite tiles are loaded into VRAM after character tiles
#define FOOD_TILE_NUM_START (BIRD_TILE_NUM_START + (sizeof(bird_data) >> 4))
// Food metasprite will be built starting with hardware sprite 9 (after character sprites)
#define FOOD_SPR_NUM_START (BIRD_SPR_NUM_START + 4)

#define INITIAL_COUNTDOWN_SETTING 3

joypads_t joypads, prevJoypads;
uint16_t frame = 0;
uint16_t lastInputFrame = 0;
uint16_t lastAudioLoopFrame = 0;
uint8_t vblanks = 0;
#if SHOW_FPS
    uint16_t lastVBlankFrame = 0;
    uint8_t fps = 0;
#endif
uint8_t paused = 0;

// Screens state machine
typedef enum screen_t {
    TITLE_SCREEN,
    INSTRUCTIONS_SCREEN,
    GAME_SCREEN,
    WINNING_SCREEN
} screen_t;
screen_t screen = TITLE_SCREEN;

// Character position
int16_t posX = 0, posY = 0;
int16_t pixelPosX = 0, pixelPosY = 0;
int16_t speedX = 0, speedY = 0;
uint8_t scrollY = 0;
uint8_t charSpriteIdx = 0;
uint16_t animationLastFrame = 0;
uint32_t score = 0;
uint8_t scoreTiles[5] = { sky_tiles_count, sky_tiles_count, sky_tiles_count, sky_tiles_count, sky_tiles_count };
uint8_t countdownSetting = 0;
uint16_t countdown = 0;
uint8_t countdownTiles[3] = { sky_tiles_count, sky_tiles_count, sky_tiles_count };
#if SHOW_FPS
    uint8_t fpsTiles[2] = { sky_tiles_count, sky_tiles_count };
#endif

// State machine
typedef enum status_t {
    IDLING,
    GLIDING,
    FLAPPING,
    DIVING
} status_t;
status_t charStatus = GLIDING;

// Food
typedef enum food_type_t {
    DANDELION,
    BERRY
} food_type_t;
typedef struct food_t {
    uint8_t enabled;
    food_type_t type;
    uint8_t spriteOffset;
    uint8_t spriteCount;
    uint8_t spriteHeight;
    uint8_t spriteIdx;
    uint16_t animationLastFrame;
    int16_t posX, posY;
    int16_t pixelPosX, pixelPosY;
    int16_t speedX, speedY;
    int8_t value;
} food_t;
food_t food[MAX_FOOD];

// Music
#define MUSIC_TONES_COUNT 64
#define MUSIC_DELAY 20
#define SIXTEENTH_NOTE_DURATION 8
#define MUSIC_LOOP_AFTER (SIXTEENTH_NOTE_DURATION * 208)
typedef enum note_t {
    NONE,
    A2, A2s, B2,
    C3, C3s, D3, D3s, E3, F3, F3s, G3, G3s, A3, A3s, B3,
    C4, C4s, D4, D4s, E4, F4, F4s, G4, G4s, A4, A4s, B4,
    C5, C5s, D5, D5s, E5, F5, F5s, G5, G5s, A5, A5s, B5,
    C6, C6s, D6, D6s, E6, F6, F6s, G6, G6s, A6, A6s, B6,
    C7, C7s, D7, D7s, E7, F7, F7s, G7, G7s, A7, A7s, B7
} note_t;
typedef struct tone_t {
    uint16_t position;
    note_t note;
    uint8_t duration;
} tone_t;
tone_t music[MUSIC_TONES_COUNT] = {
    /* Bach's Minuet in G major */
    {.position=0, .note=B4, .duration=4},
    {.position=4, .note=G4, .duration=2},
    {.position=6, .note=A4, .duration=2},
    {.position=8, .note=B4, .duration=2},
    {.position=10, .note=G4, .duration=2},
    {.position=12, .note=A4, .duration=4},
    {.position=16, .note=D4, .duration=2},
    {.position=18, .note=E4, .duration=2},
    {.position=20, .note=F4s, .duration=2},
    {.position=22, .note=D4, .duration=2},
    {.position=24, .note=G4, .duration=4},
    {.position=28, .note=E4, .duration=2},
    {.position=30, .note=F4s, .duration=2},
    {.position=32, .note=G4, .duration=2},
    {.position=34, .note=D4, .duration=2},
    {.position=36, .note=C4s, .duration=4},
    {.position=40, .note=B3, .duration=2},
    {.position=42, .note=C4s, .duration=2},
    {.position=44, .note=A3, .duration=4},
    {.position=48, .note=A3, .duration=2},
    {.position=50, .note=B3, .duration=2},
    {.position=52, .note=C4s, .duration=2},
    {.position=54, .note=D4, .duration=2},
    {.position=56, .note=E4, .duration=2},
    {.position=58, .note=F4s, .duration=2},
    {.position=60, .note=G4, .duration=4},
    {.position=64, .note=F4s, .duration=4},
    {.position=68, .note=E4, .duration=4},
    {.position=72, .note=F4s, .duration=4},
    {.position=76, .note=A3, .duration=4},
    {.position=80, .note=C4s, .duration=4},
    {.position=84, .note=D4, .duration=12},
    {.position=96, .note=NONE, .duration=4},
    {.position=100, .note=D4, .duration=4},
    {.position=104, .note=G3, .duration=2},
    {.position=106, .note=F3, .duration=2},
    {.position=108, .note=G3, .duration=4},
    {.position=112, .note=E4, .duration=4},
    {.position=116, .note=G3, .duration=2},
    {.position=118, .note=F3, .duration=2},
    {.position=120, .note=G3, .duration=4},
    {.position=124, .note=D4, .duration=4},
    {.position=128, .note=C4, .duration=4},
    {.position=132, .note=B3, .duration=4},
    {.position=136, .note=A3, .duration=2},
    {.position=138, .note=G3, .duration=2},
    {.position=140, .note=F3, .duration=2},
    {.position=142, .note=G3, .duration=2},
    {.position=144, .note=A3, .duration=4},
    {.position=148, .note=D3, .duration=2},
    {.position=150, .note=E3, .duration=2},
    {.position=152, .note=F3, .duration=2},
    {.position=154, .note=G3, .duration=2},
    {.position=156, .note=A3, .duration=2},
    {.position=158, .note=B3, .duration=2},
    {.position=160, .note=C4, .duration=4},
    {.position=164, .note=B3, .duration=4},
    {.position=168, .note=A3, .duration=4},
    {.position=172, .note=B3, .duration=2},
    {.position=174, .note=D4, .duration=2},
    {.position=176, .note=G3, .duration=4},
    {.position=180, .note=F3, .duration=4},
    {.position=184, .note=G3, .duration=12},
    {.position=196, .note=NONE, .duration=12}
};
uint16_t nextSound = 0;


void initScreen() {
    DISPLAY_OFF;

    // Init palettes
    BGP_REG = 0xE4;     // BG Palette : Black, Dark gray, Light gray, White
    OBP0_REG = 0xE4;    // Sprite palette 0 : Black, Dark gray, Light gray, Transparent
    OBP1_REG = 0xE4;    // Sprite palette 1 : Black, Dark gray, Light gray, Transparent

    switch (screen) {
        case TITLE_SCREEN: {
            // Background tilemap
            set_bkg_data(0, title_tiles_count, title_tiles);
            set_bkg_tiles(0, 0, title_map_width, title_map_height, title_map);

            // Font tiles
            set_bkg_data(title_tiles_count, font_tiles_count, font_tiles);

            // Press start
            const unsigned char pressTiles[5] = { title_tiles_count + 11, title_tiles_count + 12, title_tiles_count + 13, title_tiles_count + 14, title_tiles_count + 14 };
            set_bkg_tiles(2, 12, 5, 1, pressTiles);
            const unsigned char startTiles[5] = { title_tiles_count + 14, title_tiles_count + 15, title_tiles_count + 16, title_tiles_count + 12, title_tiles_count + 15 };
            set_bkg_tiles(3, 14, 5, 1, startTiles);

            // Reset background scroll
            move_bkg(0, 0);

            // Show background
            SHOW_BKG; HIDE_WIN; HIDE_SPRITES;

            // Disable sound
            NR52_REG = 0x00; // All sound channels OFF
        
            // Set initial state
            frame = 0;
            break;
        }
        case INSTRUCTIONS_SCREEN: {
            // Background tilemap
            set_bkg_data(0, instructions_tiles_count, instructions_tiles);
            set_bkg_tiles(0, 0, instructions_map_width, instructions_map_height, instructions_map);

            // Font tiles
            set_sprite_data(0, font_tiles_count, font_tiles);

            // Reset background scroll
            move_bkg(0, 0);

            // Reset hardware sprites
            for (uint8_t i = 0; i < 40; i++) {
                shadow_OAM[i].y = 0;
            }

            // Show background and sprites
            SHOW_BKG; HIDE_WIN; SHOW_SPRITES;
            
            // Use 8x8 sprites/tiles
            SPRITES_8x8;

            // Disable sound
            NR52_REG = 0x00; // All sound channels OFF
        
            // Set initial state
            if (countdownSetting == 0) {
                countdownSetting = INITIAL_COUNTDOWN_SETTING;
            }
            frame = 0;
            break;
        }
        case GAME_SCREEN: {
            // Background tilemap
            set_bkg_data(0, sky_tiles_count, sky_tiles);
            set_bkg_tiles(0, 0, sky_map_width, sky_map_height, sky_map);
            
            // HUD
            set_win_data(sky_tiles_count, font_tiles_count, font_tiles);
            set_win_tiles(14, 0, 5, 1, scoreTiles);
            set_win_tiles(1, 0, 3, 1, countdownTiles);
            #if SHOW_FPS
                set_win_tiles(9, 0, 2, 1, fpsTiles);
            #endif
            move_win(7, 136);

            // Load character metasprite tile data into VRAM
            set_sprite_data(BIRD_TILE_NUM_START, sizeof(bird_data) >> 4, bird_data);

            // Load food metasprites tile data into VRAM
            set_sprite_data(FOOD_TILE_NUM_START, food_tiles_count, food_tiles);

            // Load pause sprites tile data into VRAM
            set_sprite_data(PAUSE_TILE_NUM_START, pause_tiles_count, pause_tiles);

            // Reset background scroll
            move_bkg(0, 0);

            // Reset hardware sprites
            for (uint8_t i = 0; i < 40; i++) {
                shadow_OAM[i].y = 0;
            }

            // Show background, window and sprites
            SHOW_BKG; SHOW_WIN; SHOW_SPRITES;
            
            // Use 8x8 sprites/tiles
            SPRITES_8x8;

            // Enable sound
            NR52_REG = 0x80; // All sound channels ON
            NR50_REG = 0x77; // Max volume on L/R outputs
            NR51_REG = 0xFF; // All 4 channels to both L/R outputs
        
            // Set initial state
            posX = posY = 64 << 4;
            pixelPosX = pixelPosY = 64;
            speedX = INITIAL_SPEED_X;
            speedY = INITIAL_SPEED_Y;
            scrollY = 0;
            charSpriteIdx = BIRD_SPRITE_GLIDING;
            score = 0;
            countdown = countdownSetting * 60;
            frame = 0;
            lastInputFrame = 0;
            lastAudioLoopFrame = 0;
            vblanks = 0;
            paused = 0;
            for (int slot=0; slot<MAX_FOOD; slot++) {
                food[slot].enabled = 0;
            }
            nextSound = 0;

            // Initialize random number generator
            initrand(DIV_REG);
            break;
        }
        case WINNING_SCREEN: {
            // Background tilemap
            set_bkg_data(0, winning_tiles_count, winning_tiles);
            set_bkg_tiles(0, 0, winning_map_width, winning_map_height, winning_map);

            // Font tiles
            set_bkg_data(winning_tiles_count, font_tiles_count, font_tiles);

            // Score
            unsigned char finalScoreTiles[5] = { winning_tiles_count, winning_tiles_count, winning_tiles_count, winning_tiles_count, winning_tiles_count };
            const char str[6];  // Maximum 5 characters + null terminator
            sprintf(str, "%d", score);
            // Pad with zeros
            int c = -1;
            while(str[++c] != '\0');
            for (int ch=0; ch<(5-c); ch++) {
                finalScoreTiles[ch] = winning_tiles_count;
            }
            for (int ch=(5-c); ch<5; ch++) {
                finalScoreTiles[ch] = winning_tiles_count + str[ch-(5-c)] - 0x30;
            }
            set_bkg_tiles(5, 14, 5, 1, finalScoreTiles);

            // Reset background scroll
            move_bkg(0, 0);

            // Show background
            SHOW_BKG; HIDE_WIN; HIDE_SPRITES;

            // Disable sound
            NR52_REG = 0x00; // All sound channels OFF
        
            // Set initial state
            frame = 0;
            break;
        }
    }
    DISPLAY_ON;
}

uint8_t justPressed() {
    return (joypads.joy0 ^ prevJoypads.joy0) & joypads.joy0;
}
uint8_t justReleased() {
    return (joypads.joy0 ^ prevJoypads.joy0) & prevJoypads.joy0;
}

// Number of concurrent food increases with time
int8_t nextAvailableFoodSlot() {
    uint16_t maxAvailable = 1 + (frame >> 8);
    if (maxAvailable > MAX_FOOD)    maxAvailable = MAX_FOOD;
    uint8_t slot = 0;
    while (slot < maxAvailable && food[slot].enabled != 0) {
        slot++;
    }
    if (slot < maxAvailable) return slot;
    else return -1;
}

uint8_t collideWithChar(int16_t foodPixelPosX, int16_t foodPixelPosY, uint8_t foodSpriteHeight) {
    // Metasprite origin is the pivot point
    int16_t charX = pixelPosX - bird_PIVOT_X;
    int16_t charY = pixelPosY - bird_PIVOT_Y;
    int16_t foodY = foodPixelPosY - scrollY;  // Need to account for background scroll
    return (foodPixelPosX < (charX + 16) && (foodPixelPosX + 8) > charX && foodY < (charY + 16) && (foodY + 8*foodSpriteHeight) > charY);
}

void printInWindowHeader(uint8_t* tiles, uint8_t x, uint8_t characters, uint32_t value) {
    const char str[6];  // Maximum 5 characters + null terminator
    sprintf(str, "%d", value);
    // Pad with zeros
    int c = -1;
    while(str[++c] != '\0');
    for (int ch=0; ch<(characters-c); ch++) {
        tiles[ch] = sky_tiles_count;
    }
    for (int ch=(characters-c); ch<characters; ch++) {
        tiles[ch] = sky_tiles_count + str[ch-(characters-c)] - 0x30;
    }
    set_win_tiles(x, 0, characters, 1, tiles);
}


void titleScreen() {
    // Poll joypad
    prevJoypads = joypads;
    joypad_ex(&joypads);
    uint8_t pressed = justPressed();
    //uint8_t pushed = joypads.joy0;
    //uint8_t released = justReleased();

    if (frame >= 30 && (pressed & J_START)) {
        screen = INSTRUCTIONS_SCREEN;   // FIXME Transition
        initScreen();
        return;
    }

    // TODO Blink press start ???

    frame++;
}


void instructionsScreen() {
    // Poll joypad
    prevJoypads = joypads;
    joypad_ex(&joypads);
    uint8_t pressed = justPressed();
    //uint8_t pushed = joypads.joy0;
    //uint8_t released = justReleased();

    // D-pad sets game duration
    if ((pressed & J_LEFT) || (pressed & J_DOWN)) {
        countdownSetting--;
        if (countdownSetting < 1) {
            countdownSetting = 1;
        }
    } else if ((pressed & J_RIGHT) || (pressed & J_UP)) {
        countdownSetting++;
        if (countdownSetting > 9) {
            countdownSetting = 9;
        }
    }

    // Start, A, or B goes to the game screen
    if (frame >= 30 && ((pressed & J_START) || (pressed & J_A) || (pressed & J_B))) {
        screen = GAME_SCREEN;   // FIXME Transition
        initScreen();
        return;
    }

    // Display game duration setting
    set_sprite_tile(0, countdownSetting);
    if (frame == 0) {   // Move only once
        move_sprite(0, 69, 112);
    }

    frame++;
}


void gameScreen() {
    // Poll joypad
    prevJoypads = joypads;
    joypad_ex(&joypads);
    uint8_t pressed = justPressed();
    uint8_t pushed = joypads.joy0;
    uint8_t released = justReleased();

    // Store previous values
    int16_t prevPixelPosX = pixelPosX;
    int16_t prevPixelPosY = pixelPosY;
    uint8_t prevScrollY = scrollY;
    uint32_t prevScore = score;
    uint16_t prevCountdown = countdown;

    if (pressed & J_START) {
        if (paused) {
            paused = 0;
            for (int c=0; c<5; c++) {
                shadow_OAM[PAUSE_SPR_NUM_START + c].y = 0;
            }
        } else {
            paused = 1;
            for (int c=0; c<5; c++) {
                set_sprite_tile(PAUSE_SPR_NUM_START + c, PAUSE_TILE_NUM_START + c);
                move_sprite(PAUSE_SPR_NUM_START + c, 68 + c*8, 84);
            }
        }
    }

    // Show pause
    if (paused) {
        return;
    }

    if (pressed || pushed) {
        lastInputFrame = frame;
    }
    
    // Switch direction, speed is slowed down
    if ((pressed & J_LEFT) && speedX > 0) {
        speedX = -INITIAL_SPEED_X;
    } else if ((pressed & J_RIGHT) && speedX < 0) {
        speedX = INITIAL_SPEED_X;
    }

    switch (charStatus) {
        case IDLING:
            // TODO When resting on a platform???
            break;
        case GLIDING:
            // Handle buttons: flapping with A, diving with DOWN
            if (pressed & J_A) {
                // Push upwards
                speedY += SPEED_Y_BOOST_FLAPPING;
                charStatus = FLAPPING;
                break;
            } else if (pushed & J_DOWN) {
                // Dive
                speedY += SPEED_Y_BOOST_DIVING;
                charStatus = DIVING;
                break;
            } else if (charSpriteIdx != BIRD_SPRITE_GLIDING) {
                charSpriteIdx = BIRD_SPRITE_GLIDING;
            }
            // Horizontal speed increases slightly while gliding
            if (abs(speedX) < MAX_SPEED_X) {
                speedX = speedX > 0 ? speedX + 1 : speedX - 1;
            }
            break;
        case FLAPPING:
            // Display flapping animation
            if (charSpriteIdx < BIRD_SPRITE_FLAPPING_START || charSpriteIdx > BIRD_SPRITE_FLAPPING_END) {
                animationLastFrame = frame;
                charSpriteIdx = BIRD_SPRITE_GLIDING;
            } else if (frame - animationLastFrame > 2) {
                animationLastFrame = frame;
                charSpriteIdx++;
                if (charSpriteIdx > BIRD_SPRITE_FLAPPING_END) {
                    charSpriteIdx = BIRD_SPRITE_FLAPPING_START;
                } else if (charSpriteIdx == BIRD_SPRITE_GLIDING) {
                    // Go back to gliding when animation finishes
                    charStatus = GLIDING;
                }
            }
            break;
        case DIVING:
            // Display diving animation
            if (charSpriteIdx < BIRD_SPRITE_DIVING_START || charSpriteIdx > BIRD_SPRITE_DIVING_END) {
                animationLastFrame = frame;
                charSpriteIdx = BIRD_SPRITE_DIVING_START;
            } else if (charSpriteIdx < BIRD_SPRITE_DIVING_END && frame - animationLastFrame > 2) {
                animationLastFrame = frame;
                charSpriteIdx++;
            }
            // Horizontal speed increases faster while diving
            if (abs(speedX) < MAX_SPEED_X) {
                speedX = speedX > 0 ? speedX + 4 : speedX - 4;
            }
            // Go back to gliding when DOWN button is released
            if (released & J_DOWN) {
                charStatus = GLIDING;
            }
            break;
    }

    // Gravity
    speedY += 1;
    int16_t maxSpeedY = charStatus == DIVING ? MAX_SPEED_Y_DIVING : MAX_SPEED_Y_GLIDING;
    if (speedY > maxSpeedY) speedY = maxSpeedY; // Max speed downwards
    if (speedY < MAX_SPEED_Y_UPWARDS) speedY = MAX_SPEED_Y_UPWARDS; // Max speed upwards

    // Move character
    posX += speedX;
    posY += speedY;
    pixelPosX = posX >> 4;
    pixelPosY = posY >> 4;
    uint8_t redraw = prevPixelPosX != pixelPosX || prevPixelPosY != pixelPosY;

    // Automatic U-turn
    if (pixelPosX < MIN_POS_X && speedX < 0) {
        speedX = INITIAL_SPEED_X;
        redraw = 1;
    } else if (pixelPosX > MAX_POS_X && speedX > 0) {
        speedX = -INITIAL_SPEED_X;
        redraw = 1;
    }
    // Automatic flapping
    if (pixelPosY > MAX_POS_Y && speedY > 0 && charStatus != FLAPPING) {
        // Push upwards
        speedY += SPEED_Y_BOOST_FLAPPING;
        charStatus = FLAPPING;
    }
    // Upper border capping
    if (pixelPosY < MIN_POS_Y) {
        posY = MIN_POS_Y << 4;
    }

    // Scroll background
    if (pixelPosY > 72) {
        scrollY = pixelPosY - 72;
    } else {
        scrollY = 0;
    }
    if (scrollY != prevScrollY) {
        move_bkg(0, scrollY);
    }

    uint8_t hiwater = 0;
    if (redraw) {
        switch (speedX > 0) {
            case 0: hiwater = move_metasprite       (bird_metasprites[charSpriteIdx], BIRD_TILE_NUM_START, BIRD_SPR_NUM_START, pixelPosX, pixelPosY); break;
            case 1: hiwater = move_metasprite_vflip (bird_metasprites[charSpriteIdx], BIRD_TILE_NUM_START, BIRD_SPR_NUM_START, pixelPosX, pixelPosY); break;
        };
        // Hide rest of the hardware sprites, because amount of sprites differs between animation frames. Max sprites used by bird metasprite is 4.
        for (uint8_t i = hiwater; i < 4; i++) shadow_OAM[BIRD_SPR_NUM_START+i].y = 0;
    }

    // Spawn food randomly
    int8_t availableSlot = nextAvailableFoodSlot();
    if (availableSlot != -1 && rand() > 120) {
        food[availableSlot].enabled = 1;
        // Random food type (defines sprite offset, value, speed range, sound fx, ...)
        food_type_t type = (rand() < -110) ? BERRY : DANDELION;    // Around 1 berry every 15 dandelions
        food[availableSlot].type = type;
        food[availableSlot].spriteOffset = (type == BERRY) ? 6 : 0;
        food[availableSlot].spriteCount = (type == BERRY) ? 8 : 3;
        food[availableSlot].spriteHeight = (type == BERRY) ? 1 : 2;
        food[availableSlot].spriteIdx = 0;
        food[availableSlot].animationLastFrame = frame;
        int8_t direction = rand() > 0;
        food[availableSlot].pixelPosX = (direction > 0) ? 168 : 0;
        food[availableSlot].posX = food[availableSlot].pixelPosX << 4;
        food[availableSlot].pixelPosY = (type == BERRY) ? 232 : abs(rand());
        food[availableSlot].posY = food[availableSlot].pixelPosY << 4;
        food[availableSlot].speedX = (direction > 0) ? (-1 -(abs(rand()) >> 5)) : (1 + (abs(rand()) >> 5));
        if (type == BERRY)  food[availableSlot].speedX *= 4;
        food[availableSlot].speedY = (type == BERRY) ? (-20 -(abs(rand()) >> 3)) : rand() >> 5;
        food[availableSlot].value = (type == BERRY) ? 100 : 10;
        // Play a sound effect on berry appearance
        if (type == BERRY) {
            NR10_REG = 0xb7;    // Channel 1 Sweep: Time 3/128Hz, Freq increases, Shift 7
            NR11_REG = 0x80;    // Channel 1 Wave Pattern and Sound Length: Duty 50%, Length 1/4 s
            NR12_REG = 0x4e;    // Channel 1 Volume Envelope: Initial 4, Volume increases, Steps 6
            NR13_REG = 0xf1;    // Channel 1 Frequency LSB: (Part of) Freq 889 Hz
            NR14_REG = 0x86;    // Channel 1 Frequency MSB: Repeat, (Part of) Freq 889 Hz
        }
    }

    for (int slot=0; slot<MAX_FOOD; slot++) {
        if (food[slot].enabled != 0) {
            // Speed changes
            if (food[slot].type == BERRY && !(frame & 0x03)) {
                // Apply gravity
                food[slot].speedY += 1;
            }
            // Food movements
            int16_t foodPrevPixelPosX = food[slot].pixelPosX;
            int16_t foodPrevPixelPosY = food[slot].pixelPosY;
            food[slot].posX += food[slot].speedX;
            food[slot].posY += food[slot].speedY;
            food[slot].pixelPosX = food[slot].posX >> 4;
            food[slot].pixelPosY = food[slot].posY >> 4;
            uint8_t moved = foodPrevPixelPosX != food[slot].pixelPosX || foodPrevPixelPosY != food[slot].pixelPosY || (scrollY != prevScrollY);

            // Destroy food when out of screen or caught by the character
            if (collideWithChar(food[slot].pixelPosX, food[slot].pixelPosY, food[slot].spriteHeight)) {
                food[slot].enabled = 0;
                shadow_OAM[FOOD_SPR_NUM_START + 2*slot].y = 0;
                shadow_OAM[FOOD_SPR_NUM_START + 2*slot + 1].y = 0;
                // Score increases when inputs were not used since many frames
                uint16_t bonus = ((frame - lastInputFrame) >> 1);
                if (bonus > 150)    bonus = 150;    // Bonus is capped at 5 seconds / 150 points
                if (bonus < 15)     bonus = 0;      // No bonus under a half-second / 15 points
                score += food[slot].value + bonus;
                // Play sound effect (emphasized when bonus is >= 50 points)
                NR10_REG = 0x34 + (bonus >= 50 ? 1 : 0);    // Channel 1 Sweep: Time 3/128Hz, Freq increases, Shift 4
                NR11_REG = (food[slot].type == BERRY) ? 0x80 : 0x40;    // Channel 1 Wave Pattern and Sound Length: Duty 50% or 25%, Length 1/4 s
                NR12_REG = 0x83 + (bonus >= 50 ? 0x40 : 0);    // Channel 1 Volume Envelope: Initial 8, Volume decreases, Steps 3
                NR13_REG = 0x00;    // Channel 1 Frequency LSB: (Part of) Freq 1280 Hz
                NR14_REG = 0xc5 - (bonus >= 50 ? 0x40 : 0);    // Channel 1 Frequency MSB: No Repeat, (Part of) Freq 1280 Hz
                continue;
            } else if ((food[slot].pixelPosX <= 0 && food[slot].speedX < 0)
                        || (food[slot].pixelPosX >= 168 && food[slot].speedX > 0)
                        || (food[slot].pixelPosY <= 0 && food[slot].speedY < 0)
                        || (food[slot].pixelPosY >= 232 && food[slot].speedY > 0)) {   // Max scrollY is 72
                food[slot].enabled = 0;
                shadow_OAM[FOOD_SPR_NUM_START + 2*slot].y = 0;
                shadow_OAM[FOOD_SPR_NUM_START + 2*slot + 1].y = 0;
                continue;
            }

            // Display and animate food sprites
            uint8_t animated = 0;
            if (frame - food[slot].animationLastFrame > 15) {
                food[slot].animationLastFrame = frame;
                food[slot].spriteIdx++;
                if (food[slot].spriteIdx >= food[slot].spriteCount) {
                    food[slot].spriteIdx = 0;
                }
                animated = 1;
            }
            // Redraw only when required (sprite changed, position changed, scroll changed)
            if (moved || animated) {
                if (animated) {
                    for (int s=0; s<food[slot].spriteHeight; s++) {
                        set_sprite_tile(FOOD_SPR_NUM_START + 2*slot + s, FOOD_TILE_NUM_START + food[slot].spriteOffset + food[slot].spriteIdx*food[slot].spriteHeight + s);
                    }
                }
                if (moved) {
                    for (int s=0; s<food[slot].spriteHeight; s++) {
                        move_sprite(FOOD_SPR_NUM_START + 2*slot + s, food[slot].pixelPosX, food[slot].pixelPosY - scrollY + 8*s);
                    }
                }
            }
        }
    }

    // Print score in window layer
    if (score != prevScore || score == 0) {
        printInWindowHeader(scoreTiles, 14, 5, score);
    }

    // Update countdown
    if (vblanks >= 60) {
        vblanks = 0;
        countdown--;
        #if SHOW_FPS
            // Compute FPS
            fps = frame - lastVBlankFrame;
            lastVBlankFrame = frame;
        #endif
        // Handle end of countdown / game
        if (countdown == 0) {
            screen = WINNING_SCREEN;    // FIXME Transition
            initScreen();
            return;
        }
    }

    // Print countdown in window layer
    if (countdown != prevCountdown || countdown == (countdownSetting * 60)) {
        printInWindowHeader(countdownTiles, 1, 3, countdown);
    }

    // Show countdown and blink palette 0 during the last 9 seconds
    if (countdown < 10) {
        if (vblanks < 10) {
            OBP0_REG = 0xE4;
        } else if (vblanks < 20) {
            OBP0_REG = 0x90;
        } else if (vblanks < 30) {
            OBP0_REG = 0x40;
        } else if (vblanks < 40) {
            OBP0_REG = 0x00;
        } else if (vblanks < 50) {
            OBP0_REG = 0x40;
        } else if (vblanks < 60) {
            OBP0_REG = 0x90;
        } else {
            OBP0_REG = 0xE4;
        }
        if (countdown != prevCountdown) {
            set_sprite_tile(39, sky_tiles_count + countdown);
            if (countdown == 9) {   // Move only once
                move_sprite(39, 84, 28);
            }
        }
    }

    #if SHOW_FPS
        // Print FPS in window layer
        if (lastVBlankFrame == frame) {
            printInWindowHeader(fpsTiles, 9, 2, fps);
        }
    #endif

    // Play music tones
    uint16_t musicFrame = frame - lastAudioLoopFrame;
    if (nextSound < MUSIC_TONES_COUNT && musicFrame >= (MUSIC_DELAY + music[nextSound].position*SIXTEENTH_NOTE_DURATION)) {
        uint16_t freq;
        switch (music[nextSound].note) {
            case C3:    freq = 1046 /*131Hz*/; break;
            case C3s:   freq = 1102 /*139Hz*/; break;
            case D3:    freq = 1155 /*147Hz*/; break;
            case D3s:   freq = 1205 /*156Hz*/; break;
            case E3:    freq = 1253 /*165Hz*/; break;
            case F3:    freq = 1297 /*175Hz*/; break;
            case F3s:   freq = 1339 /*185Hz*/; break;
            case G3:    freq = 1379 /*196Hz*/; break;
            case G3s:   freq = 1417 /*208Hz*/; break;
            case A3:    freq = 1452 /*220Hz*/; break;
            case A3s:   freq = 1486 /*233Hz*/; break;
            case B3:    freq = 1517 /*247Hz*/; break;
            case C4:    freq = 1546 /*262Hz*/; break;
            case C4s:   freq = 1575 /*277Hz*/; break;
            case D4:    freq = 1602 /*294Hz*/; break;
            case D4s:   freq = 1627 /*311Hz*/; break;
            case E4:    freq = 1650 /*330Hz*/; break;
            case F4:    freq = 1673 /*349Hz*/; break;
            case F4s:   freq = 1694 /*370Hz*/; break;
            case G4:    freq = 1714 /*392Hz*/; break;
            case G4s:   freq = 1732 /*415Hz*/; break;
            case A4:    freq = 1750 /*440Hz*/; break;
            case A4s:   freq = 1767 /*466Hz*/; break;
            case B4:    freq = 1783 /*494Hz*/; break;
            case C5:    freq = 1798 /*523Hz*/; break;
            case C5s:   freq = 1812 /*554Hz*/; break;
            case D5:    freq = 1825 /*587Hz*/; break;
            case D5s:   freq = 1837 /*622Hz*/; break;
            case E5:    freq = 1849 /*659Hz*/; break;
            case F5:    freq = 1860 /*698Hz*/; break;
            case F5s:   freq = 1871 /*740Hz*/; break;
            case G5:    freq = 1881 /*784Hz*/; break;
            case G5s:   freq = 1890 /*831Hz*/; break;
            case A5:    freq = 1899 /*880Hz*/; break;
            case A5s:   freq = 1907 /*932Hz*/; break;
            case B5:    freq = 1915 /*988Hz*/; break;
            case C6:    freq = 1923 /*1047Hz*/; break;
            case C6s:   freq = 1930 /*1109Hz*/; break;
            case D6:    freq = 1936 /*1175Hz*/; break;
            case D6s:   freq = 1943 /*1245Hz*/; break;
            case E6:    freq = 1949 /*1319Hz*/; break;
            case F6:    freq = 1954 /*1397Hz*/; break;
            case F6s:   freq = 1959 /*1480Hz*/; break;
            case G6:    freq = 1964 /*1568Hz*/; break;
            case G6s:   freq = 1969 /*1661Hz*/; break;
            case A6:    freq = 1974 /*1760Hz*/; break;
            case A6s:   freq = 1978 /*1865Hz*/; break;
            case B6:    freq = 1982 /*1975Hz*/; break;
            default:    freq = 0; break;
        }
        // Turn channel 2 off for silences
        if (freq == 0) {
            NR51_REG = 0xDD;
        } else {
            NR51_REG = 0xFF;
            // Play tone on channel 2
            NR21_REG = 0x80;
            NR22_REG = 0x30;
            NR23_REG = freq & 0xff;
            NR24_REG = 0x80 | (freq >> 8);
        }
        nextSound++;
    }
    // Handle music loop
    if ((frame - lastAudioLoopFrame) > MUSIC_LOOP_AFTER) {
        lastAudioLoopFrame = frame;
        nextSound = 0;
    }
    

    frame++;
}


void winningScreen() {
    // Poll joypad
    prevJoypads = joypads;
    joypad_ex(&joypads);
    uint8_t pressed = justPressed();
    //uint8_t pushed = joypads.joy0;
    //uint8_t released = justReleased();

    if (frame >= 60 && pressed) {
        screen = TITLE_SCREEN;   // FIXME Transition
        initScreen();
        return;
    }

    frame++;
}


void vblank_isr() {
    if (!paused) {
        vblanks++;
    }
}


void main() {

    CRITICAL {
        STAT_REG = 0x10;    // Enable VBlank interrupt
        add_VBL(vblank_isr);
    }
    set_interrupts(VBL_IFLAG);

    initScreen();

    // Init joypad
    joypad_init(1, &joypads);

    while(1) {
        switch (screen) {
            case TITLE_SCREEN:
                titleScreen();
                break;
            case INSTRUCTIONS_SCREEN:
                instructionsScreen();
                break;
            case GAME_SCREEN:
                gameScreen();
                break;
            case WINNING_SCREEN:
                winningScreen();
                break;
        }

        // Wait for VBlank
        wait_vbl_done();
    }
}

