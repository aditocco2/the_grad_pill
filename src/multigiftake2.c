#include <pico/stdlib.h>
#include <pico/rand.h>
#include <pico/time.h>
#include <stdio.h>
#include "hub75.h"
#include "sd_card.h"

#define MEDIA_SWITCH_INTERVAL_MS 5000

uint8_t control_buffer[512];
uint8_t image_buffer[64 * 64 * 2];

uint32_t media_addr;
uint32_t num_frames;
uint16_t frame_time;
uint32_t frame_num;
uint32_t media_num, num_media;

_Bool sd_init_flag = false;
_Bool data_ready_flag = false;

int64_t switch_media_cb(alarm_id_t id, void *data);
int64_t read_frame_cb(alarm_id_t id, void *data);
_Bool switch_media();
_Bool read_frame();

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

    add_alarm_in_ms(1, switch_media_cb, NULL, false);
    add_alarm_in_ms(1, read_frame_cb, NULL, false);

    while(1){

        hub75_refresh();

        if(!sd_init_flag){
            sd_init_flag = sd_card_init();
            continue;
        }
        
    }    
}


int64_t switch_media_cb(alarm_id_t id, void *data){
    if(!switch_media()){
        add_alarm_in_ms(5, switch_media_cb, NULL, false);
    }
    else{
        add_alarm_in_ms(MEDIA_SWITCH_INTERVAL_MS, switch_media_cb, NULL, false);
    }
    return 0;
}

int64_t read_frame_cb(alarm_id_t id, void *data){
    if(!read_frame()){
        add_alarm_in_ms(5, read_frame_cb, NULL, false);
    }
    else{
        add_alarm_in_ms(frame_time, read_frame_cb, NULL, false);
    }
    return 0;
}

_Bool switch_media(){

    data_ready_flag = false;

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
    frame_time = *(uint16_t *)&control_buffer[table_row_index + 8];


    frame_num = 0;

    data_ready_flag = true;

    return true;
}

_Bool read_frame(){
    if(!data_ready_flag){
        return false;
    }

    // Read pixel data from SD card into image buffer each frametime
    if(!sd_card_read_blocks((16 * frame_num + media_addr), 16, image_buffer)){
        sd_init_flag = false;
        return false;
    }

    frame_num++;
    if(frame_num == num_frames){
        frame_num = 0;
    }

    hub75_push();

    return true;
}
