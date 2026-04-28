#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "pico/rand.h"
#include "pico/time.h"

#include "sd_card.h"
#include "hub75.h"
#include "hub75_text.h"
#include "rgb565_colors.h"

#define SWITCH_INTERVAL_INDEX 4
#define USE_STATIC_MODE_INDEX 6
#define RANDOMIZE_INDEX 7

#define MEDIA_SECTOR_ADDR_INDEX 0
#define N_FRAMES_INDEX 4
#define FRAME_TIME_INDEX 8

#define TABLE_ROW_WIDTH 16

#define MAX_MEDIA 146

#define SECTORS_PER_FRAME (WIDTH * HEIGHT * 2 / BLOCK_SIZE)

#define NUM_MEDIA_INDEX 0

#define MEDIA_SECTOR_ADDR_INDEX 0
#define N_FRAMES_INDEX 4
#define FRAME_TIME_INDEX 8

#define TABLE_ROW_WIDTH 16

#define MODESEL 14
#define STATIC_SWITCH_VALUE 0
#define SLIDESHOW_SWITCH_VALUE 1

typedef enum {TEXT, IMAGE, VIDEO} media_type_t;
typedef enum {STATIC, SLIDESHOW} mode_t;

typedef struct{
    uint16_t switch_interval_s;
    _Bool use_static_mode;
    _Bool randomize;

    mode_t current_mode;

    media_type_t current_media_type;
    uint32_t media_address;
    uint16_t frame_duration_ms;
    uint32_t current_media_index;
    uint32_t current_frame_num;
    uint32_t num_frames_in_current_media;
    uint32_t pool_size;
} player_data_t;

void main_fsm();

void slideshow_media_index_fsm(player_data_t *ts, _Bool reset);
_Bool player_get_metadata(player_data_t *ts, uint8_t *temp_buffer);
_Bool player_load_media(player_data_t *ts, uint8_t *temp_buffer);
_Bool player_get_frame(player_data_t *ts, uint8_t *frame_buffer);
_Bool mode_switched(player_data_t *ts);

void fill_array_sequentially(uint32_t *arr, uint32_t len);
void shuffle_array(uint32_t *arr, uint32_t len);

// Basic timer callbacks that just set flags
struct repeating_timer media_switch_timer, frame_switch_timer, main_loop_timer;
_Bool media_switch_flag, frame_switch_flag, main_loop_flag;
_Bool media_switch_cb(__unused repeating_timer_t *rt) {media_switch_flag = true;}
_Bool frame_switch_cb(__unused repeating_timer_t *rt) {frame_switch_flag = true;}
_Bool main_loop_cb(__unused repeating_timer_t *rt) {main_loop_flag = true;}

int main(){
    stdio_init_all();

    hub75_configure();
    hub75_set_brightness(20);

    gpio_init(MODESEL);
    gpio_pull_up(MODESEL);
    gpio_set_dir(MODESEL, GPIO_IN);

    while(67){
        main_fsm();
    }
}

void main_fsm(){
    static enum {INIT, GET_METADATA, SWITCH_MODE, LOAD_MEDIA, LOAD_FRAME, WAIT} state;

    static player_data_t ts;
    
    static _Bool sd_succ = true;

    uint8_t temp_buffer[BLOCK_SIZE];
    uint8_t *frame_buffer = (uint8_t *)hub75_get_back_buffer();

    if(!sd_succ){
        for(uint8_t x = 0; x < WIDTH; x++){
            for(uint8_t y = 0; y < HEIGHT; y++){
                hub75_set_pixel(x, y, 0);
            }
        }
        hub75_write_large_text("Insert", 32, 32, ALIGN_CENTER, ALIGN_BOTTOM, RGB565_Red);
        hub75_write_large_text("SD Card", 32, 32, ALIGN_CENTER, ALIGN_TOP, RGB565_Red);
        hub75_update();
        state = INIT;
    }

    switch(state){
        case INIT:
            sd_succ = sd_card_init();
            state = GET_METADATA;
            break;

        case GET_METADATA:
            sd_succ = player_get_metadata(&ts, temp_buffer);
            slideshow_media_index_fsm(&ts, true);
            state = SWITCH_MODE;
            break;

        case SWITCH_MODE:
            cancel_repeating_timer(&media_switch_timer);
            if(gpio_get(MODESEL) == STATIC_SWITCH_VALUE && ts.use_static_mode){
                ts.current_mode = STATIC;
            }
            else{
                ts.current_mode = SLIDESHOW;
                add_repeating_timer_ms(ts.switch_interval_s * 1000, media_switch_cb, NULL, 
                                       &media_switch_timer);
            }
            state = LOAD_MEDIA;
            break;

        case LOAD_MEDIA:
            if(ts.current_mode == STATIC){
                ts.current_media_index = ts.pool_size;
            }
            else{
                slideshow_media_index_fsm(&ts, false);
            }
            sd_succ = player_load_media(&ts, temp_buffer);
            state = LOAD_FRAME;
            break;
        
        case LOAD_FRAME:
            sd_succ = player_get_frame(&ts, frame_buffer);
            hub75_update();
            state = WAIT;
            break;

        case WAIT:
            if(media_switch_flag){
                media_switch_flag = false;
                state = LOAD_MEDIA;
            }
            else if(frame_switch_flag){
                frame_switch_flag = false;
                state = LOAD_FRAME;
            }
            else if(mode_switched(&ts)){
                state = SWITCH_MODE;
            }
            else{
                sd_succ = sd_card_check_status();
            }
            break;
    }
}

_Bool player_get_metadata(player_data_t *ts, uint8_t *temp_buffer){

    if(!sd_card_read_block(0, temp_buffer, BLOCK_SIZE)){
        return false;
    }

    ts->switch_interval_s = *(uint16_t *)&temp_buffer[SWITCH_INTERVAL_INDEX];
    ts->use_static_mode = *(_Bool *)&temp_buffer[USE_STATIC_MODE_INDEX];
    ts->randomize = *(_Bool *)&temp_buffer[RANDOMIZE_INDEX];

    ts->pool_size = *(uint32_t *)&temp_buffer[NUM_MEDIA_INDEX];
    if(ts->use_static_mode){
        ts->pool_size = ts->pool_size - 1;
    }

    return true;
}

void slideshow_media_index_fsm(player_data_t *ts, _Bool reset){
    static enum {INIT, SHUFFLE, TRAVERSE} state;

    static uint32_t media_index_array[MAX_MEDIA];
    static uint32_t tag = 0;

    if(reset){
        state = INIT;
    }

    switch(state){
        case INIT:
            fill_array_sequentially(media_index_array, ts->pool_size);
            tag = 0;
            if(ts->randomize){
                state = SHUFFLE;
            }
            else{
                state = TRAVERSE;
            }
            break;
        case SHUFFLE:
            shuffle_array(media_index_array, ts->pool_size);
            tag = 0;
            state = TRAVERSE;
            break;
        case TRAVERSE:
            tag++;
            if(ts->randomize && tag >= ts->pool_size - 1){
                state = SHUFFLE;
            }
            else if(tag >= ts->pool_size){
                tag = 0;
            }
            break;
    }

    ts->current_media_index = media_index_array[tag];

    // printf("[ ");
    // for(uint32_t i = 0; i < ts->pool_size; i++){
    //     printf("%d ", media_index_array[i]);
    // }
    // printf("] -> %d\n", ts->current_media_index);
}

_Bool player_load_media(player_data_t *ts, uint8_t *temp_buffer){
    
    cancel_repeating_timer(&frame_switch_timer);

    uint32_t table_row = ts->current_media_index + 1;
    uint16_t sector = table_row / (BLOCK_SIZE / TABLE_ROW_WIDTH);
    uint16_t table_row_index = table_row * TABLE_ROW_WIDTH % BLOCK_SIZE;

    if(!sd_card_read_block(sector, temp_buffer, BLOCK_SIZE)){
        return false;
    }

    ts->media_address = *(uint32_t *)&temp_buffer[table_row_index + MEDIA_SECTOR_ADDR_INDEX];
    ts->num_frames_in_current_media = *(uint32_t *)&temp_buffer[table_row_index + N_FRAMES_INDEX];
    ts->frame_duration_ms = *(uint16_t *)&temp_buffer[table_row_index + FRAME_TIME_INDEX];

    ts->current_frame_num = 0;

    // maybe replace with a new flag on the SD row, idk.
    // the current_media_type is not currently used, but it will be if I do scrolling text
    if(ts->num_frames_in_current_media > 1){
        ts->current_media_type = VIDEO;
        add_repeating_timer_ms(ts->frame_duration_ms, frame_switch_cb, NULL, &frame_switch_timer);
    }
    else{
        ts->current_media_type = IMAGE;
    }

    return true;
}

_Bool player_get_frame(player_data_t *ts, uint8_t *frame_buffer){
    if(!sd_card_read_blocks(ts->media_address + (ts->current_frame_num * SECTORS_PER_FRAME), 
                            SECTORS_PER_FRAME, frame_buffer)){
        return false;
    }

    ts->current_frame_num = ts->current_frame_num + 1;
    if(ts->current_frame_num == ts->num_frames_in_current_media){
        ts->current_frame_num = 0;
    }

    return true;
}

_Bool mode_switched(player_data_t *ts){
    if(ts->use_static_mode == false){
        return false;
    }
    else if(ts->current_mode == STATIC && gpio_get(MODESEL) == SLIDESHOW_SWITCH_VALUE){
        return true;
    }
    else if(ts->current_mode == SLIDESHOW && gpio_get(MODESEL) == STATIC_SWITCH_VALUE){
        return true;
    }
    else{
        return false;
    }
}

void fill_array_sequentially(uint32_t *arr, uint32_t len){
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