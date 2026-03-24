hub75#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hub75.h"
#include "hub75_text.h"
#include "rgb565_colors.h"
#include "sd_card.h"

#define SECTORS_PER_FRAME (WIDTH * HEIGHT * 2 / BLOCK_SIZE)

#define NUM_MEDIA_INDEX 0

#define MEDIA_SECTOR_ADDR_INDEX 0
#define N_FRAMES_INDEX 4
#define FRAME_TIME_INDEX 8

#define TABLE_ROW_WIDTH 16

static uint32_t media_sector_addr;
static uint32_t num_frames;
static uint16_t frame_time_ms;

static uint32_t num_media;

static int32_t media_index;
static uint32_t frame_num;

static uint8_t control_buffer[512];
static uint8_t *pixel_buffer;

struct repeating_timer frame_read_timer;
static _Bool read_new_frame_flag;

_Bool player_init();
_Bool player_get_num_media();
_Bool player_load_media(uint32_t index);
void player_pause();
void player_play();
_Bool player_update();
static _Bool read_new_frame();
static _Bool read_new_frame_cb(__unused repeating_timer_t *rt);

_Bool player_init(){
    hub75_configure();
    hub75_set_brightness(20);
    pixel_buffer = hub75_get_back_buffer();

    if(!sd_card_init()){
        hub75_write_large_text("Insert", 32, 32, ALIGN_CENTER, ALIGN_BOTTOM, RGB565_Red);
        hub75_write_large_text("SD Card", 32, 32, ALIGN_CENTER, ALIGN_TOP, RGB565_Red);
        hub75_update();
        return false;
    }
    if(!sd_card_read_block(0, control_buffer, BLOCK_SIZE)){
        return false;
    }
    num_media = *(uint32_t *)&control_buffer[NUM_MEDIA_INDEX];

    return true;
}

_Bool player_get_num_media(){
    return num_media;
}

_Bool player_load_media(uint32_t index){
    if(index >= num_media()){
        index = num_media;
    }

    player_pause();

    uint32_t table_row = index + 1;
    uint16_t sector = table_row / (BLOCK_SIZE / TABLE_ROW_WIDTH);
    uint16_t table_row_index = table_row * TABLE_ROW_WIDTH % BLOCK_SIZE;

    if(!sd_card_read_block(sector, control_buffer, BLOCK_SIZE)){
        return false;
    }

    media_sector_addr = *(uint32_t *)&control_buffer[table_row_index + MEDIA_SECTOR_ADDR_INDEX];
    num_frames = *(uint32_t *)&control_buffer[table_row_index + N_FRAMES_INDEX];
    uint16_t frame_time_ms = *(uint16_t *)&control_buffer[table_row_index + FRAME_TIME_INDEX];

    frame_num = 0;

    player_play();

    return true;
}

void player_play(){
    if(num_frames > 1){   
        add_repeating_timer_ms(-1 * frame_time_ms, read_new_frame_cb, NULL, &frame_read_timer);
        read_new_frame_flag = true;
    }
}

void player_pause(){
    cancel_repeating_timer(&frame_read_timer);
}

_Bool player_update(){
    if(read_new_frame_flag){
        if(read_new_frame()){ 
            hub75_update();
        }
        else{
            return false;
        }
    }
    return true;
}

static _Bool read_new_frame(){
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

static _Bool read_new_frame_cb(__unused repeating_timer_t *rt){
    read_new_frame_flag = true;
}