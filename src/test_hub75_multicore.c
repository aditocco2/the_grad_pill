#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hub75.h"
#include "hub75_text.h"
#include "rgb565_colors.h"

uint32_t frame_count = 0;
_Bool update_done = true;
absolute_time_t ts;

void print_frame_count();
void print_fps();
void update_cb();

void main(void){
    stdio_init_all();
    hub75_set_update_cb(update_cb);
    hub75_set_brightness(20);
    hub75_configure();
    ts = get_absolute_time();
    while(1){

        if (absolute_time_diff_us(ts, get_absolute_time()) > 1000000){
            ts = get_absolute_time();
            print_fps();
            frame_count = 0;
        }

        if(update_done){
            update_done = false;
            print_frame_count();
            hub75_update();
        }
    }
}

void update_cb(){
    frame_count++;
    update_done = true;
}

void print_frame_count(){
    char digits[4];
    for(uint8_t y = 50; y < 64; y++) for(uint8_t x = 0; x < 64; x++) hub75_set_pixel(x, y, 0);
    hub75_write_small_text("Frame ", 2, 56, ALIGN_LEFT, ALIGN_TOP, RGB565_Blue);
    hub75_write_small_text(itoa(frame_count, digits, 10), 2 + 6 * 4, 56, ALIGN_LEFT, ALIGN_TOP, RGB565_Blue);
}

void print_fps(){
    char digits[4];
    for(uint8_t y = 0; y < 10; y++) for(uint8_t x = 0; x < 64; x++) hub75_set_pixel(x, y, 0);
    hub75_write_small_text("FPS: ", 2, 2, ALIGN_LEFT, ALIGN_TOP, RGB565_Blue);
    hub75_write_small_text(itoa(frame_count, digits, 10), 2 + 5 * 4, 2, ALIGN_LEFT, ALIGN_TOP, RGB565_Blue);
}