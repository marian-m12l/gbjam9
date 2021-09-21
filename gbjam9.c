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

// Character position
uint16_t posX, posY;
int16_t speedX, speedY;
uint8_t idx, rot;


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
    idx = 2;    // FIXME Animation sprites

    while(1) {        
        // Poll joypad
        prevJoypads = joypads;
        joypad_ex(&joypads);
        
        // Handle movements
        if ((joypads.joy0 & J_UP) && !(prevJoypads.joy0 & J_UP)) {
            // TODO Push upwards
            speedY -= 16;
            // FIXME Full flapping animation
            idx++;
            if (idx > 9) idx=2;
        } else if ((joypads.joy0 & J_DOWN) && !(prevJoypads.joy0 & J_DOWN)) {
            // TODO Dive
            speedY += 32;
            // FIXME Diving animation
        } else {
            // TODO Glide
            // FIXME Should horizontal speed increase slightly while gliding?
        }

        // Switch direction, but speed stays constant
        // FIXME Should speed be slowed donw when making a U-turn?
        if (joypads.joy0 & J_LEFT) {
            speedX = -abs(speedX);
        } else if (joypads.joy0 & J_RIGHT) {
            speedX = abs(speedX);
        }

        // TODO Handle all animation cycles

        // Gravity (gliding) FIXME Use state-machine to handle flapping/gliding/diving/idling
        speedY += 1;
        if(joypads.joy0 & J_DOWN) {
            if (speedY > 32) speedY = 32;   // FIXME Max speed when diving?
        } else {
            if (speedY > 8) speedY = 8;   // FIXME Max speed when gliding?
        }

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
        // FIXME Should only be called when something changed
        rot = speedX > 0;
        switch (rot) {
            case 0: hiwater = move_metasprite       (bird_metasprites[idx], BIRD_TILE_NUM_START, BIRD_SPR_NUM_START, (posX >> 4), (posY >> 4)); break;
            case 1: hiwater = move_metasprite_vflip (bird_metasprites[idx], BIRD_TILE_NUM_START, BIRD_SPR_NUM_START, (posX >> 4), (posY >> 4)); break;
        };

        // Hide rest of the hardware sprites, because amount of sprites differs between animation frames.
        for (uint8_t i = hiwater; i < 40; i++) shadow_OAM[i].y = 0;

        // Wait for VBlank
        wait_vbl_done();
    }
}

