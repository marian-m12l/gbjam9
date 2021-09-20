#include <gb/gb.h>
#include <stdint.h>
#include <stdio.h>

#include <gb/bgb_emu.h>


void main() {
    printf("Hello, world!");
    
    while (1) {
        wait_vbl_done();
    }
}
