#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/rand.h"
#include "pico/time.h"

#include "buttons.h"
#include "sd_card.h"
#include "hub75.h"

#include "player.h"

#define SWITCH_INTERVAL_INDEX 4
#define USE_STATIC_MODE_INDEX 6
#define RANDOMIZE_INDEX 7

#define MEDIA_SECTOR_ADDR_INDEX 0
#define N_FRAMES_INDEX 4
#define FRAME_TIME_INDEX 8

#define TABLE_ROW_WIDTH 16

#define MAX_MEDIA 146

#define MODESEL 14

_Bool mode_switch_flag;
_Bool media_switch_flag;

void top_level_fsm();

uint32_t get_new_media_index(uint32_t current_index, uint32_t num_media, 
                            _Bool use_static_mode, _Bool randomize);

void fill_array(uint32_t *arr, uint32_t len);
void shuffle_array(uint32_t *arr, uint32_t len);

struct repeating_timer media_switch_timer;
_Bool media_switch_cb(__unused repeating_timer_t *rt);
void mode_switch_cb();

int main(){
    stdio_init_all();

    hub75_configure();
    hub75_set_brightness(20);

    configure_button(MODESEL);
    attach_press(MODESEL, mode_switch_cb);

    while(1){
        sleep_ms(5);
        top_level_fsm();
    }
}

void top_level_fsm(){

    static enum {INIT, SWITCH_MODE, GET_STATIC_MEDIA, SWITCH_MEDIA, WAIT} state;
    static enum {STATIC_MODE, SLIDESHOW_MODE} mode;

    static uint32_t media_index;
    static uint32_t num_media;

    static uint16_t switch_interval_s;
    static _Bool use_static_mode;
    static _Bool randomize;

    _Bool success;

    static uint8_t table_header[TABLE_ROW_WIDTH];

    switch(state){
        case INIT:
            cancel_repeating_timer(&media_switch_timer);
            success = player_init();
            if(success){    
                success = sd_card_read_block(0, table_header, TABLE_ROW_WIDTH);
            }
            if(success){
                num_media = player_get_num_media();
                media_index = -1; // so any media can be picked at first
                switch_interval_s = *(uint16_t *)&table_header[SWITCH_INTERVAL_INDEX];
                use_static_mode = *(_Bool *)&table_header[USE_STATIC_MODE_INDEX];
                randomize = *(_Bool *)&table_header[RANDOMIZE_INDEX];

                if(use_static_mode){
                    mode = STATIC_MODE;
                    state = GET_STATIC_MEDIA;
                }
                else{
                    mode = SLIDESHOW_MODE;
                    state = SWITCH_MEDIA;
                }
            }
            break;

        case SWITCH_MODE:
            if(mode == STATIC_MODE){
                mode = SLIDESHOW_MODE;
                state = SWITCH_MEDIA;
                add_repeating_timer_ms(-1000 * switch_interval_s,
                                       media_switch_cb, NULL, &media_switch_timer);
            }
            else if(mode == SLIDESHOW_MODE && use_static_mode){
                mode = STATIC_MODE;
                state = GET_STATIC_MEDIA;
                cancel_repeating_timer(&media_switch_timer);
            }
            else{
                state = SWITCH_MEDIA;
            }
            break;  

        case GET_STATIC_MEDIA:
            media_index = num_media - 1; // static mode uses last image on the card
            success = player_load_media(media_index);
            if(success){
                state = WAIT;
            }
            else{
                state = INIT;
            }
            break;

        case SWITCH_MEDIA:
            media_index = get_new_media_index(media_index, num_media, use_static_mode, randomize);
            success = player_load_media(media_index);
            if(success){
                state = WAIT;
            }
            else{
                state = INIT;
            }
            break;

        case WAIT:
            success = player_update();
            success = sd_card_check_status();
            if(media_switch_flag){
                media_switch_flag = false;
                state = SWITCH_MEDIA;
            }
            if(mode_switch_flag){
                mode_switch_flag = false;
                state = SWITCH_MODE;
            }
            if(!success){
                state = INIT;
            }
            break;
    }       
}

uint32_t get_new_media_index(uint32_t current_index, uint32_t num_media, 
                            _Bool use_static_mode, _Bool randomize){

    uint32_t media_index;

    uint32_t num_slideshow_media = num_media; 
    if(use_static_mode){
        num_slideshow_media--;
    }

    if(!randomize){
        media_index = (current_index + 1) % num_slideshow_media;
    }
    else if(num_slideshow_media == 1){
        media_index = 0;
    }
    else{
        // get a new number that's not the previous one
        uint32_t random_number = (get_rand_32() >> 8 & 0xFFFF) % (num_slideshow_media - 1);
        if(random_number >= current_index){
            media_index = random_number + 1;
        }
        else{
            media_index = random_number;
        }   
    }

    return media_index;
}

void fill_array(uint32_t *arr, uint32_t len){
    for(uint32_t i = 0; i < len; i++){
        arr[i] = i;
    }
}

void shuffle_array(uint32_t *arr, uint32_t len){
    for(uint32_t i = len - 1; i > 0; i--){
        uint32_t j = get_rand_32() % (i + 1);

        uint32_t temp = arr[j];
        arr[j] = arr[i];
        arr[i] = temp;
    }
}

_Bool media_switch_cb(__unused repeating_timer_t *rt){
    media_switch_flag = true;
    return true;
}

void mode_switch_cb(){
    mode_switch_flag = true;
}