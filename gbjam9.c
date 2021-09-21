#include <gb/console.h>
#include <gb/gb.h>
#include <gb/metasprites.h>

#include "assets/bird.h"

const unsigned char pattern[] = {0x80,0x80,0x40,0x40,0x20,0x20,0x10,0x10,0x08,0x08,0x04,0x04,0x02,0x02,0x01,0x01};

joypads_t joypads, prevJoypads;

// Character metasprite tiles are loaded into VRAM starting at tile number 0
#define BIRD_TILE_NUM_START 0
// Character metasprite will be built starting with hardware sprite 0
#define BIRD_SPR_NUM_START 0

// Character position
uint16_t posX, posY;
uint8_t idx, rot;


void main() {
    // Init palettes
    BGP_REG = 0xE4;     // BG Palette : Black, Dark gray, Light gray, White
    OBP0_REG = 0xE4;    // Sprite palette 0 : Black, Dark gray, Light gray, Transparent
    OBP1_REG = 0xE0;    // Sprite palette 1 : Black, Dark gray, White, Transparent

	// Fill the screen background with a single tile pattern
    fill_bkg_rect(0, 0, 20, 18, 0);
    set_bkg_data(0, 1, pattern);

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
    idx = 2; rot = 0;

    while(1) {        
        // Poll joypad
        prevJoypads = joypads;
        joypad_ex(&joypads);
        
        // Handle movements
        if (joypads.joy0 & J_UP) {
            posY -= 16;
        } else if (joypads.joy0 & J_DOWN) {
            posY += 16;
        }

        if (joypads.joy0 & J_LEFT) {
            posX -= 16;
            rot = 0;
        } else if (joypads.joy0 & J_RIGHT) {
            posX += 16;
            rot = 1;
        }


        // Press A button to show/hide metasprite
        if ((joypads.joy0 & J_A) && !(prevJoypads.joy0 & J_A)) {
            posX = posY = 64 << 4;
        }

        // TODO Handle all animation cycles
        // Press B button to cycle through metasprite animations FIXME Autmatic animation !!!
        if (joypads.joy0 & J_B) {
            idx++;
            if (idx > 9) idx=2;
        }

        uint8_t hiwater = 0;
        // FIXME Should only be called when something changed
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

