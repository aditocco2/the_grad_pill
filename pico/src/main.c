#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/rand.h"
#include "pico/time.h"
#include "sd_card.h"
#include "player.h"

#define SWITCH_INTERVAL_INDEX 4
#define USE_STATIC_MODE_INDEX 6
#define RANDOMIZE_INDEX 7

#define MEDIA_SECTOR_ADDR_INDEX 0
#define N_FRAMES_INDEX 4
#define FRAME_TIME_INDEX 8

#define TABLE_ROW_WIDTH 16

int32_t media_index;
uint32_t num_media;

uint16_t switch_interval_s;
_Bool use_static_mode;
_Bool randomize;

_Bool mode_switch_flag;
_Bool media_switch_flag;

void top_level_fsm();

_Bool init_fsm();
_Bool static_mode_fsm();
_Bool slideshow_mode_fsm();

int32_t get_new_media_index();

struct repeating_timer media_switch_timer;
_Bool switch_media_cb(__unused repeating_timer_t *rt);

int main(){
    stdio_init_all();

    while(1){
        top_level_fsm();
    }
}

void top_level_fsm(){
    static enum {INIT, STATIC_MODE, SLIDESHOW_MODE} state, return_state = STATIC_MODE;

    _Bool success;

    switch(state){
        case INIT:
            _Bool done = init_fsm();
            if(done && use_static_mode && return_state == STATIC_MODE){
                state = STATIC_MODE;
            }
            else if(done){
                state = SLIDESHOW_MODE;
            }
            break;
        
        case STATIC_MODE:
            success = !static_mode_fsm();
            if(!success){
                return_state = state;
                state = INIT;
            }
            else if mode_switch_flag{
                mode_switch_flag = false;
                state = SLIDESHOW_MODE;
            }
            break;

        case SLIDESHOW_MODE:
            success = !slideshow_mode_fsm();
            if(!success){
                return_state = state;
                state = INIT;
            }
            else if mode_switch_flag{
                mode_switch_flag = false;
                state = STATIC_MODE;
            }
            break;
    }
}

_Bool init_fsm(){
    static enum{SD_INIT, GET_INITIAL_DATA} state = SD_INIT;

    static _Bool done = true;
    _Bool success;

    uint8_t table_header[16];

    switch(state){
        case SD_INIT:
            success = player_init();
            if(success){
                state = GET_INITIAL_DATA;
            }
            break;

        case GET_INITIAL_DATA:
            success = sd_card_read_block(0, table_header, 16);
            if(success){
                num_media = player_get_num_media();
                media_index = -1; // so any media can be picked at first
                switch_interval_s = *(uint16_t *)&table_header[SWITCH_INTERVAL_INDEX];
                use_static_mode = *(_Bool *)&table_header[USE_STATIC_MODE_INDEX];
                randomize = *(_Bool *)&table_header[RANDOMIZE_INDEX];
                done = true;
            }
            else{
                state = SD_INIT;
            }
            break;
    }
    return done;
}

_Bool static_mode_fsm(){
    static enum {START, WAIT} state;

    static _Bool exit_and_reset = true;
    _Bool success;

    if(reset){
        state = START;
        reset = false;
    }

    switch(state){
        case START:
            // static mode image is the last one on the SD card
            media_index = num_media - 1;
            success = player_load_media(media_index);
            if(success){
                state = WAIT;
            }
            else{
                exit_and_reset = true;
            }
            break;

        case WAIT:
            success = player_update();
            success = sd_card_check_status();
            if(!success){
                exit_and_reset = true;
            }
            break;
    }
    return exit_and_reset;
}

_Bool slideshow_mode_fsm(){

}

int32_t get_new_media_index(){

    uint32_t num_slideshow_media = num_media; 
    if(use_static_mode){
        num_slideshow_media--;
    }

    if(!randomize){
        media_index = (media_index + 1) % num_slideshow_media;
    }
    else{
        // get a new number that's not the previous one
        uint32_t random_number = (get_rand_32() >> 8 & 0xFFFF) % (num_media - 1);
        if(random_number >= media_index){
            media_index = random_number + 1;
        }
        else{
            media_index = random_number;
        }   
    }
}

_Bool switch_media_cb(__unused repeating_timer_t *rt){
    media_switch_flag = true;
}