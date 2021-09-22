#include <gb/console.h>
#include <gb/gb.h>
#include <gb/metasprites.h>
#include <stdlib.h>

#include "metasprites/bird.h"

#include "tilesets/sky_tiles.h"
#include "tilesets/sky_map.h"

// Character metasprite tiles are loaded into VRAM starting at tile number 0
#define BIRD_TILE_NUM_START 0
// Character metasprite will be built starting with hardware sprite 0
#define BIRD_SPR_NUM_START 0

joypads_t joypads, prevJoypads;
uint64_t frame = 0;

// Character position
uint16_t posX, posY;
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


uint8_t justPressed() {
    return (joypads.joy0 ^ prevJoypads.joy0) & joypads.joy0;
}
uint8_t justReleased() {
    return (joypads.joy0 ^ prevJoypads.joy0) & prevJoypads.joy0;
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

    // Show background and sprites
    SHOW_BKG; SHOW_SPRITES;
    
    // Use 8x8 sprites/tiles
    SPRITES_8x8;

    // Init joypad
    joypad_init(1, &joypads);
 
    // Set initial position
    posX = posY = 64 << 4;
    speedX = 10;
    speedY = 0;
    charSpriteIdx = 5;    // FIXME Animation sprites

    while(1) {        
        // Poll joypad
        prevJoypads = joypads;
        joypad_ex(&joypads);
        uint8_t pressed = justPressed();
        uint8_t pushed = joypads.joy0;
        uint8_t released = justReleased();
        
        // Switch direction, but speed stays constant
        // FIXME Should speed be slowed down when making a U-turn?
        if (pressed & J_LEFT) {
            speedX = -abs(speedX);
        } else if (pressed & J_RIGHT) {
            speedX = abs(speedX);
        }

        switch (charStatus) {
            case IDLING:
                // TODO When resting on a platform???
                break;
            case GLIDING:
                // TODO Handle buttons: flapping with A, diving with DOWN
                if (pressed & J_A) {
                    // Push upwards
                    speedY -= 32;
                    charSpriteIdx = 5;    // TODO Handle animation reset ???
                    charStatus = FLAPPING;
                    break;
                }
                if (pushed & J_DOWN) {
                    // Dive
                    speedY += 32;
                    charSpriteIdx = 10;    // TODO Handle animation reset ???
                    charStatus = DIVING;
                    break;
                }
                // FIXME Should horizontal speed increase slightly while gliding?
                break;
            case FLAPPING:
                // TODO Display flapping animation FIXME Multiple frames per sprite !!!
                if (frame - animationLastFrame > 2) {
                    animationLastFrame = frame;
                    charSpriteIdx++;
                    if (charSpriteIdx > 9) {
                        charSpriteIdx = 2;    // TODO Handle animation reset ???
                    } else if (charSpriteIdx == 5) {
                        // Go back to gliding when animation finishes
                        charStatus = GLIDING;
                    }
                }
                break;
            case DIVING:
                // TODO Display diving animation FIXME Multiple frames per sprite !!!
                if (frame - animationLastFrame > 5) {
                    animationLastFrame = frame;
                    charSpriteIdx++;
                    if (charSpriteIdx > 12) {
                        charSpriteIdx = 12;    // TODO Handle animation reset ???
                    }
                }
                // TODO Handle collision (hurt enemies ???)
                // TODO Go back to gliding when DOWN button is released
                if (released & J_DOWN) {
                    charSpriteIdx = 5;    // TODO Handle animation reset ???
                    charStatus = GLIDING;
                }
                break;
        }

        // Gravity
        speedY += 1;
        int16_t maxSpeedY = charStatus == DIVING ? 32 : 8;
        if (speedY > maxSpeedY) speedY = maxSpeedY; // Max speed downwards
        if (speedY < -32) speedY = -32; // Max speed upwards

        // Move character
        posX += speedX;
        posY += speedY;

        // Scroll background
        if ((posY >> 4) > 144/2) {
            uint8_t scrollY = (posY >> 4) - (144/2);
            move_bkg(0, scrollY);
        } else {
            move_bkg(0, 0);
        }

        uint8_t hiwater = 0;
        // FIXME Should only be called when something changed --> Character moved or changed direction or animation frame changed
        rot = speedX > 0;
        switch (rot) {
            case 0: hiwater = move_metasprite       (bird_metasprites[charSpriteIdx], BIRD_TILE_NUM_START, BIRD_SPR_NUM_START, (posX >> 4), (posY >> 4)); break;
            case 1: hiwater = move_metasprite_vflip (bird_metasprites[charSpriteIdx], BIRD_TILE_NUM_START, BIRD_SPR_NUM_START, (posX >> 4), (posY >> 4)); break;
        };

        // Hide rest of the hardware sprites, because amount of sprites differs between animation frames.
        for (uint8_t i = hiwater; i < 40; i++) shadow_OAM[i].y = 0;

        // Wait for VBlank
        wait_vbl_done();
        frame++;
    }
}

