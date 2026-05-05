// just one more state machine bro, that's all i need

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/rand.h"
#include "pico/time.h"

#include "sd_card.h"
#include "hub75.h"
#include "hub75_text.h"
#include "rgb565_colors.h"

#define MAX_MEDIA 146
#define TEXT_SCROLLING_PIXELS_PER_SECOND 40

#define TEXT_INST_SEPARATION 16

#define SECTORS_PER_FRAME (WIDTH * HEIGHT * 2 / BLOCK_SIZE)

#define NUM_MEDIA_INDEX 0
#define SWITCH_INTERVAL_INDEX 4
#define USE_STATIC_MODE_INDEX 6
#define RANDOMIZE_INDEX 7

#define TABLE_ROW_WIDTH 16

#define MEDIA_SECTOR_ADDR_INDEX 0
#define N_FRAMES_INDEX 4
#define FRAME_TIME_INDEX 8
#define MEDIA_TYPE_INDEX 15

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
    uint32_t num_frames_in_video;
    uint32_t pool_size;
} player_data_t;

void main_fsm();

void slideshow_media_index_fsm(player_data_t *ts, _Bool reset);
_Bool player_get_metadata(player_data_t *ts, uint8_t *temp_buffer);
_Bool player_load_media(player_data_t *ts, uint8_t *temp_buffer);
_Bool player_get_frame(player_data_t *ts, uint8_t *frame_buffer);
_Bool text_scrolling_fsm(player_data_t *ts, char *str, _Bool reset);
uint16_t color_cycle_rgb565_fsm(_Bool reset);
_Bool mode_switched(player_data_t *ts);
void fill_array_sequentially(uint32_t *arr, uint32_t len);
void shuffle_array(uint32_t *arr, uint32_t len);
void clear_screen();

// Basic timer callbacks that just set flags
struct repeating_timer media_switch_timer, frame_switch_timer;
_Bool media_switch_flag, frame_switch_flag;
_Bool media_switch_cb(__unused repeating_timer_t *rt) {media_switch_flag = true;}
_Bool frame_switch_cb(__unused repeating_timer_t *rt) {frame_switch_flag = true;}

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
    static enum {INIT, GET_METADATA, INVALID_CARD, SWITCH_MODE, 
                 LOAD_MEDIA, INITIALIZE_MEDIA, PLAY_MEDIA} state;

    static player_data_t ts;
    
    static _Bool sd_succ = true;

    uint8_t temp_buffer[BLOCK_SIZE];
    uint8_t *frame_buffer = (uint8_t *)hub75_get_back_buffer();

    if(!sd_succ){
        clear_screen();
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
            
            if((*(uint32_t *)&temp_buffer[12]) == 0xe3bedded){
                slideshow_media_index_fsm(&ts, true);
                state = SWITCH_MODE;
            }
            else{
                state = INVALID_CARD;
            }
            break;

        case INVALID_CARD:
            clear_screen();
            hub75_write_large_text("Invalid", 32, 32, ALIGN_CENTER, ALIGN_BOTTOM, RGB565_Red);
            hub75_write_large_text("SD Card", 32, 32, ALIGN_CENTER, ALIGN_TOP, RGB565_Red);
            hub75_update();
            sd_succ = sd_card_check_status();
            break;

        case SWITCH_MODE:
            cancel_repeating_timer(&media_switch_timer);
            if(gpio_get(MODESEL) == STATIC_SWITCH_VALUE && ts.use_static_mode){
                ts.current_mode = STATIC;
            }
            else{
                ts.current_mode = SLIDESHOW;
                if(ts.pool_size > 1){
                    add_repeating_timer_ms(ts.switch_interval_s * 1000, media_switch_cb, NULL, 
                                           &media_switch_timer);
                }
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

            state = INITIALIZE_MEDIA;
            break;
        
        case INITIALIZE_MEDIA:
            if (ts.current_media_type == VIDEO){
                sd_succ = player_get_frame(&ts, frame_buffer);
                hub75_update();
            }
            else if (ts.current_media_type == IMAGE){
                sd_succ = sd_card_read_blocks(ts.media_address, SECTORS_PER_FRAME, frame_buffer);
                hub75_update();
            }
            else if(ts.current_media_type == TEXT){
                sd_succ = text_scrolling_fsm(&ts, temp_buffer, true);
                hub75_update();
            }
            state = PLAY_MEDIA;
            break;

        case PLAY_MEDIA:
            if(media_switch_flag){
                media_switch_flag = false;
                state = LOAD_MEDIA;
            }
            else if(mode_switched(&ts)){
                state = SWITCH_MODE;
            }
            else if(frame_switch_flag && ts.current_media_type == VIDEO){
                frame_switch_flag = false;
                sd_succ = player_get_frame(&ts, frame_buffer);
                hub75_update();
            }
            else if(frame_switch_flag && ts.current_media_type == TEXT){
                frame_switch_flag = false;
                text_scrolling_fsm(&ts, temp_buffer, false);
                hub75_update();
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

    char media_type_char = *(char *)&temp_buffer[table_row_index + MEDIA_TYPE_INDEX];
    switch(media_type_char){
        case 'i':
            ts->current_media_type = IMAGE;
            break;
        case 'v':
            ts->current_media_type = VIDEO;
            ts->num_frames_in_video = *(uint32_t *)&temp_buffer[table_row_index + N_FRAMES_INDEX];
            ts->frame_duration_ms = *(uint16_t *)&temp_buffer[table_row_index + FRAME_TIME_INDEX];
            ts->current_frame_num = 0;
            add_repeating_timer_ms(ts->frame_duration_ms, frame_switch_cb, NULL, 
                                   &frame_switch_timer);
            break;
        case 't':
            ts->current_media_type = TEXT;
            add_repeating_timer_ms(1000 / TEXT_SCROLLING_PIXELS_PER_SECOND, frame_switch_cb, NULL, 
                                   &frame_switch_timer);
            break;
    }

    return true;
}

_Bool player_get_frame(player_data_t *ts, uint8_t *frame_buffer){
    if(!sd_card_read_blocks(ts->media_address + (ts->current_frame_num * SECTORS_PER_FRAME), 
                            SECTORS_PER_FRAME, frame_buffer)){
        return false;
    }

    ts->current_frame_num = ts->current_frame_num + 1;
    if(ts->current_frame_num == ts->num_frames_in_video){
        ts->current_frame_num = 0;
    }

    return true;
}

_Bool text_scrolling_fsm(player_data_t *ts, char *str, _Bool reset){
    static enum {INIT, SCROLL} state;

    static uint16_t color;
    static int16_t start_x;
    int16_t current_x;
    static uint16_t len;

    if(reset){
        state = INIT;
    }

    switch(state){
        case INIT:
            start_x = WIDTH;

            if(!sd_card_read_block(ts->media_address, str, BLOCK_SIZE)){
                return false;
            }

            len = strlen(str);

            state = SCROLL;
            break;
            
        case SCROLL:
            clear_screen();
            
            color = color_cycle_rgb565_fsm(false);

            start_x--;
            current_x = start_x;

            do{
                hub75_write_medium_text(str, current_x, HEIGHT/2, ALIGN_LEFT, ALIGN_CENTER, color);
                current_x += len * MEDIUM_FONT_WIDTH + TEXT_INST_SEPARATION;
            }while(current_x < WIDTH);

            break;
    }

    return true;
}

uint16_t color_cycle_rgb565_fsm(_Bool reset){

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

void clear_screen(){
    for(uint8_t x = 0; x < WIDTH; x++){
        for(uint8_t y = 0; y < HEIGHT; y++){
            hub75_set_pixel(x, y, RGB565_Black);
        }
    }
}