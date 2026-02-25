#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "hub75.h"
#include "images/test_gradient.h"
#include "images/ecd1015.h"

uint32_t refresh_count = 0;
absolute_time_t ts;

void refresh_cb(){
    refresh_count++;
}

void main(void){
    stdio_init_all();
    hub75_configure();
    hub75_set_refresh_cb(refresh_cb);
    hub75_load_image(test_gradient);
    hub75_update();
    while(1){
        // print refresh count every second
        if (absolute_time_diff_us(ts, get_absolute_time()) > 1000000){
            ts = get_absolute_time();
            printf("Refresh rate: %d Hz\n", refresh_count);
            refresh_count = 0;
        }
    }
}