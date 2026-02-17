#include "FreeRTOS.h"
#include "task.h"
#include "time.h"
#include "FreeRTOSConfig.h"
#include <pico/stdlib.h>
#include <pico/rand.h>
#include <stdio.h>
#include "hub75.h"
#include "sd_card.h"

#define BUTTON_PIN 20

#define GIF_SWITCH_INTERVAL_MS 5000

void hub75_task(__unused void *params);
void frame_read_task(__unused void *params);
void gif_switch_task(__unused void *params);
void button_task(__unused void *params);
void sd_init_task(__unused void *params);

_Bool hub75_on = true;
uint8_t control_buffer[512];
uint8_t image_buffer[64 * 64 * 2];

uint32_t gif_addr;
uint32_t num_frames;
uint16_t frame_time;
uint32_t frame_num;
uint32_t gif_num, num_gifs;

_Bool sd_init_flag = false;
_Bool data_ready_flag = false;

void main(void){
    stdio_init_all();

    xTaskCreate(hub75_task, "", 256, NULL, 1, NULL);
    xTaskCreate(frame_read_task, "", 256, NULL, 1, NULL);
    xTaskCreate(gif_switch_task, "", 256, NULL, 2, NULL);
    xTaskCreate(button_task, "", 256, NULL, 1, NULL);
    xTaskCreate(sd_init_task, "", 256, NULL, 2, NULL);
    

    vTaskStartScheduler();
}

void hub75_task(__unused void *params){
    hub75_configure();

    hub75_load_image((uint16_t *)image_buffer);

    while(1){
        if(hub75_on){
            hub75_refresh();
        }
        vTaskDelay(1);
    }
}

void button_task(__unused void *params){
    static bool current_state = false, previous_state = false;

    gpio_init(BUTTON_PIN);
    gpio_pull_up(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    
    TickType_t last_time = xTaskGetTickCount();

    while(1){
        previous_state = current_state;
        current_state = !gpio_get(BUTTON_PIN);
    
        if(current_state && !previous_state){
            hub75_on = !hub75_on;
        }

        vTaskDelayUntil(&last_time, pdMS_TO_TICKS(5));
    }
}

void sd_init_task(__unused void *params){

    // init sd card once and re-init if it gets lost
    while(!sd_init_flag){
        vTaskSuspendAll();
        sd_init_flag = sd_card_init();
        xTaskResumeAll();
    }

    while(1){

        if(!sd_init_flag){
            vTaskSuspendAll();
            sd_init_flag = sd_card_init();
            xTaskResumeAll();
        }
        vTaskDelay(1);
    }
}

void frame_read_task(__unused void *params){

    // wait for SD init and data ready
    while(!(sd_init_flag && data_ready_flag)){
        vTaskDelay(1);
    }

    TickType_t last_frame_time = xTaskGetTickCount();

    while(1){

        printf("f");
        
        if(!data_ready_flag){
            vTaskDelay(1);
            continue;
        }

        // Read pixel data from SD card into image buffer each frametime
        if(sd_card_read_blocks((16 * frame_num + gif_addr), 16, image_buffer)){
            sd_init_flag = false;
            vTaskDelay(1);
            continue;
        }

        frame_num++;
        if(frame_num == num_frames){
            frame_num = 0;
        }
        vTaskDelayUntil(&last_frame_time, pdMS_TO_TICKS(frame_time));
    }

}

void gif_switch_task(__unused void *params){

    // wait for SD init
    while(!sd_init_flag){
        vTaskDelay(1);
    }
    
    while(!sd_card_read_block(0, control_buffer, 512)){
        sd_init_flag = false;
        vTaskDelay(1);
    }

    num_gifs = (uint32_t)control_buffer[0];

    TickType_t last_switch_time = xTaskGetTickCount();

    while(1){

        printf("s");

        data_ready_flag = false;

        // Pick a random gif and load its data (address/length/frametime)
        gif_num = get_rand_32() % (num_gifs - 1);
        if(gif_num >= num_gifs){
            gif_num = gif_num + 1;
        }

        uint32_t table_row = gif_num + 1;
        uint16_t sector = table_row / (512 / 16); // 16 bytes per row
        uint16_t table_row_index = table_row * 16 % 512;
        
        if(!sd_card_read_block(sector, control_buffer, 512)){
            printf("Sf");
            sd_init_flag = false;
            vTaskDelay(1);
            continue;
        }

        // table row byte assignment:
        // 0 through 3: sector address
        // 4 through 7: number of frames
        // 8 through 9: frame duration
        // 10 through 15: unused
        gif_addr = (uint32_t)control_buffer[table_row_index];
        num_frames = (uint32_t)control_buffer[table_row_index + 4];
        frame_time = (uint16_t)control_buffer[table_row_index + 8];

        frame_num = 0;

        data_ready_flag = true;

        vTaskDelayUntil(&last_switch_time, pdMS_TO_TICKS(GIF_SWITCH_INTERVAL_MS));
    }
    
}
