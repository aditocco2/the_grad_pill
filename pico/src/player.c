#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/rand.h"
#include "pico/time.h"
#include "hub75.h"
#include "hub75_text.h"
#include "rgb565_colors.h"
#include "sd_card.h"
#include "images/sd_error.h"

#define SECTORS_PER_FRAME (WIDTH * HEIGHT * 2 / BLOCK_SIZE)

#define NUM_MEDIA_INDEX 0
#define SWITCH_INTERVAL_INDEX 4
#define RANDOMIZE_INDEX 7

#define MEDIA_SECTOR_ADDR_INDEX 0
#define N_FRAMES_INDEX 4
#define FRAME_TIME_INDEX 8

#define TABLE_ROW_WIDTH 16

static uint32_t num_media;
static uint16_t switch_interval_s;
static _Bool randomize;

static int32_t media_index;
static uint32_t media_sector_addr;
static uint32_t frame_num, num_frames;
static uint16_t frame_time_ms;

static uint16_t control_sector;
static uint8_t control_buffer[512];

static uint8_t *pixel_buffer;
static _Bool switch_media_flag, read_new_frame_flag;

struct repeating_timer media_switch_timer, frame_read_timer;

void player_fsm();

_Bool get_initial_data();
_Bool switch_media();
_Bool read_new_frame();
_Bool switch_media_cb(__unused repeating_timer_t *rt);
_Bool read_new_frame_cb(__unused repeating_timer_t *rt);

int main(){
    stdio_init_all();

    hub75_configure();
    hub75_set_brightness(20);

    pixel_buffer = (uint8_t*)hub75_get_back_buffer();

    while(1){
        player_fsm();
    }
}

void player_fsm(){

    static _Bool success;
    
    static enum {SD_INIT, GET_INITIAL_DATA, SWITCH_MEDIA, READ_FRAME, WAIT} state;
    switch(state){
        case SD_INIT:
            success = sd_card_init(); 
            if(success){
                state = GET_INITIAL_DATA;
            }
            else{
                hub75_write_large_text("Insert", 32, 32, ALIGN_CENTER, ALIGN_BOTTOM, RGB565_Red);
                hub75_write_large_text("SD Card", 32, 32, ALIGN_CENTER, ALIGN_TOP, RGB565_Red);
                hub75_update();
            }
            break;

        case GET_INITIAL_DATA:
            success = get_initial_data();
            if(success){
                state = SWITCH_MEDIA;
            }
            else{
                state = SD_INIT;
            }
            break;

        case SWITCH_MEDIA:
            success = switch_media();
            if(success){
                state = READ_FRAME;
            }
            else{
                state = SD_INIT;
            } 
            break;

        case READ_FRAME:
            success = read_new_frame();
            if(success){
                hub75_update();
                state = WAIT;
            }
            else{
                state = SD_INIT;
            }
            break;

        case WAIT:
            if(switch_media_flag){
                switch_media_flag = false;
                state = SWITCH_MEDIA;
            }
            else if(read_new_frame_flag){
                read_new_frame_flag = false;
                state = READ_FRAME;
            }

            if(!sd_card_check_status()){
                state = SD_INIT;
            }
        
            break;
    }

}


_Bool get_initial_data(){

    cancel_repeating_timer(&media_switch_timer);

    if(!sd_card_read_block(0, control_buffer, BLOCK_SIZE)){
        return false;
    }

    control_sector = 0;

    num_media = *(uint32_t *)&control_buffer[NUM_MEDIA_INDEX];
    switch_interval_s = *(uint16_t *)&control_buffer[SWITCH_INTERVAL_INDEX];
    if(control_buffer[RANDOMIZE_INDEX]){
        randomize = true;
    }

    add_repeating_timer_ms(-1000 * switch_interval_s, switch_media_cb, NULL, &media_switch_timer);

    return true;
}


_Bool switch_media(){

    cancel_repeating_timer(&frame_read_timer);

    // if there's only one image, do nothing
    if(num_media == 1){
        return true;
    }

    // if randomize selection off, just move on sequentially to the next image
    else if(!randomize){
        media_index = (media_index + 1) % num_media;
    }
    else{
        // get a new random media index that's not the current one
        uint32_t random_number = (get_rand_32() >> 8) % (num_media - 1);
        if(random_number >= media_index){
            media_index = random_number + 1;
        }
        else{
            media_index = random_number;
        }   
    }
    
    uint32_t table_row = media_index + 1;
    uint16_t sector = table_row / (BLOCK_SIZE / TABLE_ROW_WIDTH);
    uint16_t table_row_index = table_row * TABLE_ROW_WIDTH % BLOCK_SIZE;

    if(!sd_card_read_block(sector, control_buffer, BLOCK_SIZE)){
        return false;
    }

    media_sector_addr = *(uint32_t *)&control_buffer[table_row_index + MEDIA_SECTOR_ADDR_INDEX];
    num_frames = *(uint32_t *)&control_buffer[table_row_index + N_FRAMES_INDEX];
    uint16_t frame_time_ms = *(uint16_t *)&control_buffer[table_row_index + FRAME_TIME_INDEX];

    frame_num = 0;

    if(num_frames > 1){   
        add_repeating_timer_ms(-1 * frame_time_ms, read_new_frame_cb, NULL, &frame_read_timer);
    }

    return true;
}

_Bool read_new_frame(){
    if(!sd_card_read_blocks(media_sector_addr + (frame_num * SECTORS_PER_FRAME), 
                            SECTORS_PER_FRAME, pixel_buffer)){
        return false;
    }

    frame_num++;
    if(frame_num == num_frames){
        frame_num = 0;
    }
    return true;
}

_Bool read_new_frame_cb(__unused repeating_timer_t *rt){
    read_new_frame_flag = true;
}

_Bool switch_media_cb(__unused repeating_timer_t *rt){
    switch_media_flag = true;
}