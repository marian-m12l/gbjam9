#include <gb/console.h>
#include <gb/gb.h>
#include <gb/metasprites.h>
#include <rand.h>
#include <stdio.h>
#include <stdlib.h>

#include "metasprites/bird.h"
#include "metasprites/food.h"

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

#define BIRD_SPRITE_GLIDING 5
#define BIRD_SPRITE_FLAPPING_START 2
#define BIRD_SPRITE_FLAPPING_END 9
#define BIRD_SPRITE_DIVING_START 10
#define BIRD_SPRITE_DIVING_END 12

#define INITIAL_SPEED_X 1
#define MAX_SPEED_X 12
#define INITIAL_SPEED_Y 0
#define MAX_SPEED_Y_GLIDING 8
#define MAX_SPEED_Y_DIVING 32
#define MAX_SPEED_Y_UPWARDS -32
#define SPEED_Y_BOOST_FLAPPING -32
#define SPEED_Y_BOOST_DIVING 32

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
uint64_t frame = 0;
uint8_t vblanks = 0;
#if SHOW_FPS
    uint64_t lastVBlankFrame = 0;
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
int16_t speedX = 0, speedY = 0;
uint8_t scrollY = 0;
uint8_t charSpriteIdx = 0;
uint64_t animationLastFrame = 0;
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
typedef struct food_t {
    uint8_t enabled;
    uint8_t spriteOffset;
    uint8_t spriteIdx;
    uint64_t animationLastFrame;
    int16_t posX, posY;
    int16_t speedX, speedY;
    int8_t value;
} food_t;
food_t food[MAX_FOOD];


void initScreen() {
    DISPLAY_OFF;

    // Init palettes
    BGP_REG = 0xE4;     // BG Palette : Black, Dark gray, Light gray, White
    OBP0_REG = 0xE4;    // Sprite palette 0 : Black, Dark gray, Light gray, Transparent
    OBP1_REG = 0xE0;    // Sprite palette 1 : Black, Dark gray, White, Transparent

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
            set_sprite_data(FOOD_TILE_NUM_START, sizeof(food_data) >> 4, food_data);

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
            speedX = INITIAL_SPEED_X;
            speedY = INITIAL_SPEED_Y;
            scrollY = 0;
            charSpriteIdx = BIRD_SPRITE_GLIDING;
            score = 0;
            countdown = countdownSetting * 60;
            frame = 0;
            vblanks = 0;
            paused = 0;
            for (int slot=0; slot<MAX_FOOD; slot++) {
                food[slot].enabled = 0;
            }
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
    uint64_t maxAvailable = 1 + (frame >> 8);
    if (maxAvailable > MAX_FOOD)    maxAvailable = MAX_FOOD;
    uint8_t slot = 0;
    while (slot < maxAvailable && food[slot].enabled != 0) {
        slot++;
    }
    if (slot < maxAvailable) return slot;
    else return -1;
}

uint8_t collideWithChar(int16_t foodPosX, int16_t foodPosY) {
    // Metasprite origin is the pivot point
    int16_t charX = (posX >> 4) - bird_PIVOT_X;
    int16_t charY = (posY >> 4) - bird_PIVOT_Y;
    // FIXME Substract pivot if using metasprites for food!
    int16_t foodX = (foodPosX >> 4);
    int16_t foodY = (foodPosY >> 4) - scrollY;  // Need to account for background scroll
    return (foodX < (charX + 16) && (foodX + 8) > charX && foodY < (charY + 16) && (foodY + 8) > charY);
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
    int16_t prevX = posX;
    int16_t prevY = posY;
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
    
    // Switch direction, but speed stays constant
    // Speed is slowed down when making a U-turn
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
    uint8_t redraw = (prevX >> 4) != (posX >> 4) || (prevY >> 4) != (posY >> 4);

    // Automatic U-turn
    if ((posX >> 4) < MIN_POS_X && speedX < 0) {
        speedX = INITIAL_SPEED_X;
        redraw = 1;
    } else if ((posX >> 4) > MAX_POS_X && speedX > 0) {
        speedX = -INITIAL_SPEED_X;
        redraw = 1;
    }
    // Automatic flapping
    if ((posY >> 4) > MAX_POS_Y && speedY > 0 && charStatus != FLAPPING) {
        // Push upwards
        speedY += SPEED_Y_BOOST_FLAPPING;
        charStatus = FLAPPING;
    }
    // Upper border capping
    if ((posY >> 4) < MIN_POS_Y) {
        posY = MIN_POS_Y << 4;
    }

    // Scroll background
    if ((posY >> 4) > 144/2) {
        scrollY = (posY >> 4) - (144/2);
    } else {
        scrollY = 0;
    }
    if (scrollY != prevScrollY) {
        move_bkg(0, scrollY);
    }

    uint8_t hiwater = 0;
    if (redraw) {
        switch (speedX > 0) {
            case 0: hiwater = move_metasprite       (bird_metasprites[charSpriteIdx], BIRD_TILE_NUM_START, BIRD_SPR_NUM_START, (posX >> 4), (posY >> 4)); break;
            case 1: hiwater = move_metasprite_vflip (bird_metasprites[charSpriteIdx], BIRD_TILE_NUM_START, BIRD_SPR_NUM_START, (posX >> 4), (posY >> 4)); break;
        };
        // Hide rest of the hardware sprites, because amount of sprites differs between animation frames. Max sprites used by bird metasprite is 4.
        for (uint8_t i = hiwater; i < 4; i++) shadow_OAM[BIRD_SPR_NUM_START+i].y = 0;
    }

    // Spawn food randomly
    int8_t availableSlot = nextAvailableFoodSlot();
    if (availableSlot != -1 && rand() > 120) {  // FIXME Need to initialize randomizer with seed (for instance when the player presses start button on the title screen)
        food[availableSlot].enabled = 1;
        // TODO Random food type (sprite offset, value, speed range, sound fx, ...)
        food[availableSlot].spriteOffset = 0;
        food[availableSlot].spriteIdx = 0;
        food[availableSlot].animationLastFrame = frame;
        int8_t direction = rand() > 0;
        food[availableSlot].posX = (direction > 0) ? (168 << 4) : (0 << 4);
        food[availableSlot].posY = abs(rand()) << 4;
        food[availableSlot].speedX = (direction > 0) ? (-1 -(abs(rand()) / 32)) : (1 + (abs(rand()) / 32));
        food[availableSlot].speedY = rand() / 32;
        food[availableSlot].value = 10;
    }

    for (int slot=0; slot<MAX_FOOD; slot++) {
        if (food[slot].enabled != 0) {
            // TODO Speed changes ???
            // Food movements
            int16_t foodPrevX = food[slot].posX;
            int16_t foodPrevY = food[slot].posY;
            food[slot].posX += food[slot].speedX;
            food[slot].posY += food[slot].speedY;
            uint8_t moved = (foodPrevX >> 4) != (food[slot].posX >> 4) || (foodPrevY >> 4) != (food[slot].posY >> 4) || (scrollY != prevScrollY);

            // Destroy food when out of screen or caught by the character
            if (collideWithChar(food[slot].posX, food[slot].posY)) {
                food[slot].enabled = 0;
                shadow_OAM[FOOD_SPR_NUM_START + 2*slot].y = 0;
                shadow_OAM[FOOD_SPR_NUM_START + 2*slot + 1].y = 0;
                score += food[slot].value;
                // Play sound effect
                NR10_REG = 0x34;    // Channel 1 Sweep: Time 3/128Hz, Freq increases, Shift 4
                NR11_REG = 0x40;    // Channel 1 Wave Pattern and Sound Length: Duty 25%, Length 1/4 s
                NR12_REG = 0x83;    // Channel 1 Volume Envelope: Initial 8, Volume decreases, Steps 3
                NR13_REG = 0x00;    // Channel 1 Frequency LSB: (Part of) Freq 1280 Hz
                NR14_REG = 0xc5;    // Channel 1 Frequency MSB: No Repeat, (Part of) Freq 1280 Hz
                continue;
            } else if (((food[slot].posX >> 4) <= 0 && food[slot].speedX < 0)
                        || ((food[slot].posX >> 4) >= 168 && food[slot].speedX > 0)
                        || ((food[slot].posY >> 4) <= 0 && food[slot].speedY < 0)
                        || ((food[slot].posY >> 4) >= 232 && food[slot].speedY > 0)) {   // Max scrollY is 72
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
                if (food[slot].spriteIdx > 2) {    // FIXME animation frames number ???
                    food[slot].spriteIdx = 0;
                }
                animated = 1;
            }
            // Redraw only when required (sprite changed, position changed, scroll changed)
            if (moved || animated) {
                // FIXME Metasprite not working on real hardware ??? (character metasprite should be using the first 4 hardware sprites)
                //move_metasprite(food_metasprites[food[slot].spriteOffset + food[slot].spriteIdx], FOOD_TILE_NUM_START + food[slot].spriteOffset, FOOD_SPR_NUM_START + 2*slot, (food[slot].posX >> 4), (food[slot].posY >> 4) - scrollY);
                if (animated) {
                    set_sprite_tile(FOOD_SPR_NUM_START + 2*slot, FOOD_TILE_NUM_START + food[slot].spriteOffset + food[slot].spriteIdx*2);
                    set_sprite_tile(FOOD_SPR_NUM_START + 2*slot + 1, FOOD_TILE_NUM_START + food[slot].spriteOffset + food[slot].spriteIdx*2 + 1);
                }
                if (moved) {
                    move_sprite(FOOD_SPR_NUM_START + 2*slot, (food[slot].posX >> 4), (food[slot].posY >> 4) - scrollY);
                    move_sprite(FOOD_SPR_NUM_START + 2*slot + 1, (food[slot].posX >> 4), (food[slot].posY >> 4) - scrollY + 8);
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

