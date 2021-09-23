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
#include "tilesets/sky_tiles.h"
#include "tilesets/sky_map.h"
#include "tilesets/font_tiles.h"

// Character metasprite tiles are loaded into VRAM starting at tile number 0
#define BIRD_TILE_NUM_START 0
// Character metasprite will be built starting with hardware sprite 0
#define BIRD_SPR_NUM_START 0

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
#define FOOD_TILE_NUM_START (BIRD_TILE_NUM_START + sizeof(bird_data) >> 4)
// Food metasprite will be built starting with hardware sprite 4 (after character sprites)
#define FOOD_SPR_NUM_START 4

joypads_t joypads, prevJoypads;
uint64_t frame = 0;

// Screens state machine
typedef enum screen_t {
    TITLE_SCREEN,
    GAME_SCREEN,
    WINNING_SCREEN
} screen_t;
screen_t screen = TITLE_SCREEN;

// Character position
int16_t posX, posY;
int16_t speedX, speedY;
uint8_t scrollY;
uint8_t charSpriteIdx;
uint64_t animationLastFrame;
uint32_t score;
uint8_t scoreTiles[5] = { sky_tiles_count, sky_tiles_count, sky_tiles_count, sky_tiles_count, sky_tiles_count };

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

            // Show background
            SHOW_BKG; HIDE_WIN; HIDE_SPRITES;
        
            // Set initial state
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
            move_win(7, 136);

            // Load character metasprite tile data into VRAM
            set_sprite_data(BIRD_TILE_NUM_START, sizeof(bird_data) >> 4, bird_data);

            // Load food metasprites tile data into VRAM
            set_sprite_data(FOOD_TILE_NUM_START, sizeof(food_data) >> 4, food_data);

            // Show background, window and sprites
            SHOW_BKG; SHOW_WIN; SHOW_SPRITES;
            
            // Use 8x8 sprites/tiles
            SPRITES_8x8;
        
            // Set initial state
            posX = posY = 64 << 4;
            speedX = INITIAL_SPEED_X;
            speedY = INITIAL_SPEED_Y;
            scrollY = 0;
            charSpriteIdx = BIRD_SPRITE_GLIDING;
            score = 0;
            frame = 0;
            break;
        }
        case WINNING_SCREEN: {
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


void titleScreen() {
    // Poll joypad
    prevJoypads = joypads;
    joypad_ex(&joypads);
    uint8_t pressed = justPressed();
    uint8_t pushed = joypads.joy0;
    uint8_t released = justReleased();

    if ((pressed & J_START)) {
        screen = GAME_SCREEN;   // FIXME Transition
        initScreen();
    }

    // TODO Blink press start ???
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
    uint16_t prevScore = score;
    
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
    // FIXME Should only be called when something changed --> Character moved or changed direction or animation frame changed
    if (redraw) {
        switch (speedX > 0) {
            case 0: hiwater = move_metasprite       (bird_metasprites[charSpriteIdx], BIRD_TILE_NUM_START, BIRD_SPR_NUM_START, (posX >> 4), (posY >> 4)); break;
            case 1: hiwater = move_metasprite_vflip (bird_metasprites[charSpriteIdx], BIRD_TILE_NUM_START, BIRD_SPR_NUM_START, (posX >> 4), (posY >> 4)); break;
        };
        // Hide rest of the hardware sprites, because amount of sprites differs between animation frames. Max sprites used by bird metasprite is 4.
        for (uint8_t i = hiwater; i < 4; i++) shadow_OAM[i].y = 0;
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
            uint8_t redraw = (foodPrevX >> 4) != (food[slot].posX >> 4) || (foodPrevY >> 4) != (food[slot].posY >> 4);

            // Destroy food when out of screen or caught by the character
            if (collideWithChar(food[slot].posX, food[slot].posY)) {
                food[slot].enabled = 0;
                shadow_OAM[FOOD_SPR_NUM_START + 2*slot].y = 0;
                shadow_OAM[FOOD_SPR_NUM_START + 2*slot + 1].y = 0;
                score += food[slot].value;
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
            if (frame - food[slot].animationLastFrame > 15) {
                food[slot].animationLastFrame = frame;
                food[slot].spriteIdx++;
                if (food[slot].spriteIdx > 2) {    // FIXME animation frames number ???
                    food[slot].spriteIdx = 0;
                }
                redraw = 1;
            }
            // TODO Redraw only when required (sprite changed, position changed, scroll changed)
            if (redraw || (scrollY != prevScrollY)) {   // FIXME Also redraw if scrollY changed!!!
                // FIXME Metasprite not working on real hardware ??? (character metasprite should be using the first 4 hardware sprites)
                //move_metasprite(food_metasprites[food[slot].spriteOffset + food[slot].spriteIdx], FOOD_TILE_NUM_START + food[slot].spriteOffset, FOOD_SPR_NUM_START + 2*slot, (food[slot].posX >> 4), (food[slot].posY >> 4) - scrollY);
                set_sprite_tile(FOOD_SPR_NUM_START + 2*slot, FOOD_TILE_NUM_START + food[slot].spriteOffset + food[slot].spriteIdx*2);
                move_sprite(FOOD_SPR_NUM_START + 2*slot, (food[slot].posX >> 4), (food[slot].posY >> 4) - scrollY);
                set_sprite_tile(FOOD_SPR_NUM_START + 2*slot + 1, FOOD_TILE_NUM_START + food[slot].spriteOffset + food[slot].spriteIdx*2 + 1);
                move_sprite(FOOD_SPR_NUM_START + 2*slot + 1, (food[slot].posX >> 4), (food[slot].posY >> 4) - scrollY + 8);
            }
        }
    }

    // Print score in window layer
    if (score != prevScore) {
        const char scoreStr[6];
        sprintf(scoreStr, "%5d", score);
        // Pad score with zeros
        int c = -1;
        while(scoreStr[++c] != '\0');
        for (int ch=0; ch<(5-c); ch++) {
            scoreTiles[ch] = sky_tiles_count;
        }
        for (int ch=(5-c); ch<5; ch++) {
            scoreTiles[ch] = sky_tiles_count + scoreStr[ch-(5-c)] - 0x30;
        }
        set_win_tiles(14, 0, 5, 1, scoreTiles);
    }
}


void winningScreen() {

}


void main() {
    // Init palettes
    BGP_REG = 0xE4;     // BG Palette : Black, Dark gray, Light gray, White
    OBP0_REG = 0xE4;    // Sprite palette 0 : Black, Dark gray, Light gray, Transparent
    OBP1_REG = 0xE0;    // Sprite palette 1 : Black, Dark gray, White, Transparent

    initScreen();

    // Init joypad
    joypad_init(1, &joypads);

    while(1) {
        switch (screen) {
            case TITLE_SCREEN:
                titleScreen();
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
        frame++;
    }
}

