#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hub75.h"
#include "rgb565_colors.h"
#include "hub75_text.h"
#include "images/test_gradient.h"
#include "images/ecd1015.h"

/*
Shortcuts:
w/s: increase/decrease brightness
f: enter fullbright
e: leave fullbright
m: write medium text
l: write large text
*/

uint32_t refresh_count = 0;
uint32_t refresh_rate = 0;
uint8_t brightness = 10;
uint8_t old_brightness;
absolute_time_t ts;

void print_info();
void print_refresh_rate();
void print_brightness();
void refresh_cb();

void main(void){
    stdio_init_all();
    hub75_configure();
    hub75_set_refresh_cb(refresh_cb);
    hub75_set_brightness(brightness);
    hub75_load_image(test_gradient);
    hub75_update();
    while(1){
        // print refresh count every second
        if (absolute_time_diff_us(ts, get_absolute_time()) > 1000000){
            ts = get_absolute_time();
            refresh_rate = refresh_count;
            refresh_count = 0;
            print_info();
        }

        int c = getchar_timeout_us(0);
        if(c == 'w'){
            brightness++;
            hub75_set_brightness(brightness);
            print_info();
        }
        else if (c == 's'){
            brightness--;
            hub75_set_brightness(brightness);
            print_info();
        }
        else if (c == 'f'){        
            old_brightness = brightness;
            brightness = 255;
            hub75_set_brightness(255);
            print_info();
        }
        else if (c == 'e'){
            brightness = old_brightness;
            hub75_set_brightness(brightness);
            print_info();
        }
        else if (c == 'l'){
            hub75_write_large_text("big text", 0, 25, ALIGN_LEFT, ALIGN_TOP, RGB565_Black_bean);
            hub75_update();
        }
        else if (c == 'm'){
            hub75_write_medium_text("Medium text", 0, 25, ALIGN_LEFT, ALIGN_TOP, RGB565_Black_bean);
            hub75_update();
        }
    }
}

void refresh_cb(){
    refresh_count++;
}

void print_info(){
    hub75_load_image(test_gradient);
    print_brightness();
    print_refresh_rate();
    hub75_update(); 
}

void print_brightness(){
    char bright[4];
    hub75_write_small_text("Brightness: ", 2, 4, ALIGN_LEFT, ALIGN_TOP, RGB565_Black);
    hub75_write_small_text(itoa(brightness, bright, 10), 2 + 12 * 4, 4, ALIGN_LEFT, ALIGN_TOP, RGB565_Black);
}

void print_refresh_rate(){
    char refresh[4];
    hub75_write_small_text("Refresh: ", 2, 56, ALIGN_LEFT, ALIGN_TOP, RGB565_Black);
    hub75_write_small_text(itoa(refresh_rate, refresh, 10), 2 + 9 * 4, 56, ALIGN_LEFT, ALIGN_TOP, RGB565_Black);
}