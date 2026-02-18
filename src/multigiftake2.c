#include <pico/stdlib.h>
#include <pico/rand.h>
#include <pico/time.h>
#include <hardware/sync.h>
#include <stdio.h>
#include "hub75.h"
#include "sd_card.h"
#include "images/sd_error.h"

#define MEDIA_SWITCH_INTERVAL_MS 10000

uint8_t control_buffer[512];
uint8_t *image_buffer;

uint32_t media_index, num_media;
uint32_t media_addr;
uint32_t num_frames;
uint16_t pixel_read_interval_us;

uint32_t frame_num;
uint8_t block_num_within_frame;

// Control flow flags
_Bool sd_success, sd_success_prev, time_to_switch_media, media_switch_in_progress;

_Bool read_pixel_data_cb(__unused repeating_timer_t *rt);
_Bool switch_media_cb(__unused repeating_timer_t *rt);
void switch_media();
void read_pixel_data();
void get_num_media();
void load_error_message();

struct repeating_timer media_switch_timer, pixel_read_timer;

void main(void){
    stdio_init_all();

    hub75_configure();
    image_buffer = (uint8_t *)hub75_get_back_buffer();

    add_repeating_timer_ms(-1 * MEDIA_SWITCH_INTERVAL_MS, switch_media_cb, NULL, &media_switch_timer);

    while(1){

        hub75_refresh();

        if(sd_success_prev && !sd_success){
            load_error_message();
        }

        if(!sd_success){
            sd_success = sd_card_init_fsm();
            if(sd_success){
                get_num_media();
            }
            continue;
        }

        sd_success_prev = true;

        if(time_to_switch_media){
            switch_media();
        }
    }    
}

_Bool read_pixel_data_cb(__unused repeating_timer_t *rt){
    if(sd_success){  
        read_pixel_data();
    }
    return true;
}

_Bool switch_media_cb(__unused repeating_timer_t *rt){
    if(sd_success){  
        time_to_switch_media = true;
    }
    return true;
}

void get_num_media(){
    sd_success = sd_card_read_block(0, control_buffer, 512);
    if(sd_success){
        num_media = *(uint32_t *)&control_buffer[0];
        time_to_switch_media = true;
    }
}

void switch_media(){

    cancel_repeating_timer(&pixel_read_timer);

    // Pick random media and load its data
    // Be sure to pick media that is not currently being played
    if(num_media > 1){ 
        uint32_t random_number = get_rand_32() % (num_media - 1);
        if(random_number >= media_index){
            media_index = random_number + 1;
        }
        else{
            media_index = random_number;
        }
    }
    // handle div/0 case
    else{
        media_index = 0;
    }

    // Now get this media's address/length/frametime from the table

    // Figure out what block the appropriate table row is in
    uint32_t table_row = media_index + 1;
    uint16_t sector = table_row / (512 / 16); // 16 bytes per row
    uint16_t table_row_index = table_row * 16 % 512;

    // Load it from the SD card
    sd_success = sd_card_read_block(sector, control_buffer, 512);
    if(!sd_success){
        return;
    }

    // table row byte assignment:
    // 0 through 3: sector address
    // 4 through 7: number of frames
    // 8 through 9: frame duration
    // 10 through 15: unused
    media_addr = *(uint32_t *)&control_buffer[table_row_index];
    num_frames = *(uint32_t *)&control_buffer[table_row_index + 4];
    uint16_t frame_time_ms = *(uint16_t *)&control_buffer[table_row_index + 8];

    pixel_read_interval_us = frame_time_ms * 1000 / 16;

    frame_num = 0;
    block_num_within_frame = 0;

    add_repeating_timer_us(-pixel_read_interval_us, read_pixel_data_cb, NULL, &pixel_read_timer);

    time_to_switch_media = false;
            
}

void read_pixel_data(){

    // Reads one SD card block at a time into image buffer
    // When all 16 blocks have been read, push the SD buffer to the HUB75 driver

    // (cursed math here, replace with something more sensible later)
    sd_success = sd_card_read_block((media_addr + (frame_num * 16) + block_num_within_frame), 
                                    image_buffer + 512 * block_num_within_frame, 512);
    if(!sd_success){
        return;
    }

    // move onto the next block
    block_num_within_frame++;
    // if all blocks in frame have been read, move to next frame
    if(block_num_within_frame == 16){
        frame_num++;
        block_num_within_frame = 0;
        hub75_push();
    }    
    if(frame_num == num_frames){
        frame_num = 0;
    }
}

void load_error_message(){
    hub75_load_image(sd_error);
    hub75_push();
}
    