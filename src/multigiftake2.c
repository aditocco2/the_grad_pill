#include <pico/stdlib.h>
#include <pico/rand.h>
#include <pico/time.h>
#include <hardware/sync.h>
#include <stdio.h>
#include "hub75.h"
#include "sd_card.h"

#define MEDIA_SWITCH_INTERVAL_MS 5000

uint8_t control_buffer[512];
uint8_t image_buffer[64 * 64 * 2];

// No, you don't understand, we NEED all these variables to be file scope
uint32_t media_addr;
uint32_t frame_num, num_frames;
uint8_t block_num_within_frame;
uint16_t frame_time_ms, pixel_read_interval_us;
uint32_t media_num, num_media;

_Bool sd_init_flag, switch_media_flag, read_pixel_data_flag;

_Bool switch_media_cb(__unused repeating_timer_t *rt);
_Bool read_pixel_data_cb(__unused repeating_timer_t *rt);
_Bool switch_media();
_Bool read_pixel_data();

struct repeating_timer media_switch_timer, pixel_read_timer;

void main(void){
    stdio_init_all();

    hub75_configure();
    hub75_load_image((uint16_t *)image_buffer);

    // try to load number of gifs until success
    while(!sd_init_flag){
        sd_init_flag = sd_card_init();
        if(sd_init_flag){  
            sd_init_flag = sd_card_read_block(0, control_buffer, 512);
        }
    }
    num_media = (uint32_t)control_buffer[0];

    switch_media_flag = true;
    add_repeating_timer_ms(-1 * MEDIA_SWITCH_INTERVAL_MS, switch_media_cb, NULL, &media_switch_timer);

    while(1){

        hub75_refresh();

        uint32_t primask;
        if(!sd_init_flag){
            primask = save_and_disable_interrupts();
            sd_init_flag = sd_card_init();
            continue;
        }
        restore_interrupts(primask);

        if(switch_media_flag){
            switch_media();
        }
    }    
}

_Bool switch_media_cb(__unused repeating_timer_t *rt){
    switch_media_flag = true;
    return true;
}

_Bool read_pixel_data_cb(__unused repeating_timer_t *rt){
    if(read_pixel_data_flag){    
        read_pixel_data();
    }
    return true;
}

_Bool switch_media(){

    read_pixel_data_flag = false;

    // Pick a random gif and load its data (address/length/frametime)
    if(num_media > 1){ 
        uint32_t random_number = get_rand_32() % (num_media - 1);
        if(random_number >= media_num){
            media_num = random_number + 1;
        }
        else{
            media_num = random_number;
        }
    }
    // handle div/0 case
    else{
        media_num = 0;
    }
    
    uint32_t table_row = media_num + 1;
    uint16_t sector = table_row / (512 / 16); // 16 bytes per row
    uint16_t table_row_index = table_row * 16 % 512;
    
    if(!sd_card_read_block(sector, control_buffer, 512)){
        sd_init_flag = false;
        
        return false;
    }

    // table row byte assignment:
    // 0 through 3: sector address
    // 4 through 7: number of frames
    // 8 through 9: frame duration
    // 10 through 15: unused
    media_addr = *(uint32_t *)&control_buffer[table_row_index];
    num_frames = *(uint32_t *)&control_buffer[table_row_index + 4];
    frame_time_ms = *(uint16_t *)&control_buffer[table_row_index + 8];

    // Change the pixel reading interval to 1/16 of the gif's frametime
    pixel_read_interval_us = frame_time_ms * 1000 / 16;
    cancel_repeating_timer(&pixel_read_timer);
    add_repeating_timer_us(pixel_read_interval_us, read_pixel_data_cb, NULL, &pixel_read_timer);

    frame_num = 0;
    block_num_within_frame = 0;

    switch_media_flag = false;
    read_pixel_data_flag = true;

    return true;
}

_Bool read_pixel_data(){

    // Reads one SD card block at a time into image buffer
    // When all 16 blocks have been read, push the SD buffer to the HUB75 driver

    // (cursed math here, replace with something more sensible later)
    if(!sd_card_read_block((media_addr + 16 * frame_num + block_num_within_frame), 
                            image_buffer + 512 * block_num_within_frame, 512)){
        sd_init_flag = false;
        return false;
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

    return true;
}
