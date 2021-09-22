#include <gb/console.h>
#include <gb/gb.h>
#include <gb/metasprites.h>
#include <rand.h>
#include <stdlib.h>

#include "metasprites/bird.h"
#include "metasprites/food.h"

#include "tilesets/sky_tiles.h"
#include "tilesets/sky_map.h"

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

#define MAX_FOOD 16

joypads_t joypads, prevJoypads;
uint64_t frame = 0;

// Character position
int16_t posX, posY;
int16_t speedX, speedY;
uint8_t charSpriteIdx, rot;
uint64_t animationLastFrame;

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


uint8_t justPressed() {
    return (joypads.joy0 ^ prevJoypads.joy0) & joypads.joy0;
}
uint8_t justReleased() {
    return (joypads.joy0 ^ prevJoypads.joy0) & prevJoypads.joy0;
}

int8_t nextAvailableFoodSlot() {
    uint8_t slot = 0;
    while (slot < /*FIXME MAX_FOOD*/ 1 && food[slot].enabled != 0) {
        slot++;
    }
    if (slot < /*FIXME MAX_FOOD*/ 1) return slot;
    else return -1;
}

uint8_t collideWithChar(int16_t foodPosX, int16_t foodPosY) {
    // TODO Fix collision detection
    return (foodPosX < posX + (16 << 4) && foodPosX + (8 << 4) > posX && foodPosY < posY + (16 << 4) && foodPosY + (8 << 4) > posY);
}


void main() {
    // Init palettes
    BGP_REG = 0xE4;     // BG Palette : Black, Dark gray, Light gray, White
    OBP0_REG = 0xE4;    // Sprite palette 0 : Black, Dark gray, Light gray, Transparent
    OBP1_REG = 0xE0;    // Sprite palette 1 : Black, Dark gray, White, Transparent

    // Background tilemap
    set_bkg_data(0, sky_tiles_count, sky_tiles);
    set_bkg_tiles(0, 0, sky_map_width, sky_map_height, sky_map);

    // Load character metasprite tile data into VRAM
    set_sprite_data(BIRD_TILE_NUM_START, sizeof(bird_data) >> 4, bird_data);

    // Load food metasprites tile data into VRAM
    set_sprite_data(BIRD_TILE_NUM_START + sizeof(bird_data) >> 4, sizeof(food_data) >> 4, food_data);

    // Show background and sprites
    SHOW_BKG; SHOW_SPRITES;
    
    // Use 8x8 sprites/tiles
    SPRITES_8x8;

    // Init joypad
    joypad_init(1, &joypads);
 
    // Set initial position
    posX = posY = 64 << 4;
    speedX = INITIAL_SPEED_X;
    speedY = INITIAL_SPEED_Y;
    charSpriteIdx = BIRD_SPRITE_GLIDING;

    while(1) {        
        // Poll joypad
        prevJoypads = joypads;
        joypad_ex(&joypads);
        uint8_t pressed = justPressed();
        uint8_t pushed = joypads.joy0;
        uint8_t released = justReleased();
        
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
                // TODO Handle collision (hurt enemies ???)
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

        // Automatic U-turn
        if ((posX >> 4) < MIN_POS_X && speedX < 0) {
            speedX = INITIAL_SPEED_X;
        } else if ((posX >> 4) > MAX_POS_X && speedX > 0) {
            speedX = -INITIAL_SPEED_X;
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
        uint8_t scrollY = 0;
        if ((posY >> 4) > 144/2) {
            scrollY = (posY >> 4) - (144/2);
        }
        move_bkg(0, scrollY);

        uint8_t hiwater = 0;
        // FIXME Should only be called when something changed --> Character moved or changed direction or animation frame changed
        rot = speedX > 0;
        switch (rot) {
            case 0: hiwater = move_metasprite       (bird_metasprites[charSpriteIdx], BIRD_TILE_NUM_START, BIRD_SPR_NUM_START, (posX >> 4), (posY >> 4)); break;
            case 1: hiwater = move_metasprite_vflip (bird_metasprites[charSpriteIdx], BIRD_TILE_NUM_START, BIRD_SPR_NUM_START, (posX >> 4), (posY >> 4)); break;
        };

        // Hide rest of the hardware sprites, because amount of sprites differs between animation frames.
        for (uint8_t i = hiwater; i < 40; i++) shadow_OAM[i].y = 0;

        // TODO Spawn food randomly
        int8_t availableSlot = nextAvailableFoodSlot();
        if (availableSlot != -1 && rand() > 120) {  // FIXME Need to initialize randomizer with seed (for instance when the player presses start button on the title screen)
            food[availableSlot].enabled = 1;
            // TODO random position, speed, food type (sprite offset), value, ...
            food[availableSlot].spriteOffset = 0;
            food[availableSlot].spriteIdx = 0;
            food[availableSlot].animationLastFrame = frame;
            food[availableSlot].posX = 80 << 4;
            food[availableSlot].posY = 80 << 4;
            food[availableSlot].speedX = 4;
            food[availableSlot].speedY = 2;
            food[availableSlot].value = 10;
        }

        for (int slot=0; slot<16; slot++) {
            if (food[slot].enabled != 0) {
                // TODO Speed changes ???
                // Food movements
                food[slot].posX += food[slot].speedX;
                food[slot].posY += food[slot].speedY;

                // Destroy food when out of screen or caught by the character
                if (collideWithChar(food[slot].posX, food[slot].posY)) {
                    food[slot].enabled = 0;
                    // FIXME score += food[slot].value;
                } else if ((food[slot].posX >> 4) < -20 && food[slot].speedX < 0) {
                    food[slot].enabled = 0;
                } else if ((food[slot].posX >> 4) > 180 && food[slot].speedX > 0) {
                    food[slot].enabled = 0;
                }

                // Display and animate food sprites
                if (frame - food[slot].animationLastFrame > 15) {
                    food[slot].animationLastFrame = frame;
                    food[slot].spriteIdx++;
                    if (food[slot].spriteIdx > 2) {    // FIXME animation frames number ???
                        food[slot].spriteIdx = 0;
                    }
                }
                move_metasprite(food_metasprites[food[slot].spriteOffset + food[slot].spriteIdx], BIRD_TILE_NUM_START + sizeof(bird_data) >> 4 + food[slot].spriteOffset, BIRD_SPR_NUM_START + 4, (food[slot].posX >> 4), (food[slot].posY >> 4) - scrollY);
            } // FIXME Otherwise hide metasprites ???
        }

        // Wait for VBlank
        wait_vbl_done();
        frame++;
    }
}

