#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <pico/stdlib.h>

#include "hub75.h"
#include "hub75_text.h"
#include "rgb565_colors.h"

#define CHAR_WIDTH 6

uint16_t color_cycle_rgb565(_Bool reset);

int main(){
    hub75_configure();
    hub75_set_brightness(10);

    const char message[] = "let's build platform component";

    int16_t start_x = 64;
    int16_t msg_len_pixels = strlen(message) * 6;
    int16_t end_x = start_x + msg_len_pixels;

    int16_t color;
    
    absolute_time_t start = get_absolute_time();

    while(1){
        // main loop delay
        absolute_time_t end = get_absolute_time();
        if(absolute_time_diff_us(start, end) < 83333){
            continue;
        }
        start = get_absolute_time();
        
        // move text one pixel left
        start_x--;
        end_x--;

        // if text goes off screen, reset the position
        if(end_x < 0){
            start_x = 64;
            end_x = start_x + msg_len_pixels;
        }

        color = color_cycle_rgb565(false);

        // set screen black
        for(uint8_t x = 0; x < WIDTH; x++){
            for(uint8_t y = 0; y < HEIGHT; y++){
                hub75_set_pixel(x, y, 0);
            }
        }

        hub75_write_medium_text(message, start_x, 32, ALIGN_LEFT, ALIGN_TOP, color);

        hub75_update();
    }
}


uint16_t color_cycle_rgb565(_Bool reset){

    static enum {INCREASE_G, DECREASE_R, INCREASE_B, DECREASE_G, INCREASE_R, DECREASE_B} state;

    // we are using rgb555 for simplicity here, will convert to rgb565 on the way out
    static uint8_t r = 31;
    static uint8_t g = 0;
    static uint8_t b = 0;

    if(reset){
        state = INCREASE_G;
        r = 31;
        g = 0;
        b = 0;
    }

    switch(state){
        case INCREASE_G:
            g++;
            if(g == 31){
                state = DECREASE_R;
            }
            break;
            
        case DECREASE_R:
            r--;
            if(r == 0){
                state = INCREASE_B;
            }
            break;

        case INCREASE_B:
            b++;
            if(b == 31){
                state = DECREASE_G;
            }
            break;
        
        case DECREASE_G:
            g--;
            if(g == 0){
                state = INCREASE_R;
            }
            break;

        case INCREASE_R:
            r++;
            if(r == 31){
                state = DECREASE_B;
            }
            break;

        case DECREASE_B:
            b--;
            if(b == 0){
                state = INCREASE_G;
            }
            break;   
    }

    // build the rgb565 pixel by shifting the green pixel 1 up
    uint16_t pix = (r << 11) | (g << 6) | (b << 0);
    return pix;
}
