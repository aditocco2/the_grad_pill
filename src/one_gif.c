#include "FreeRTOS.h"
#include "task.h"
#include "time.h"
#include "FreeRTOSConfig.h"
#include <pico/stdlib.h>
#include <stdio.h>
#include "hub75.h"
#include "sd_card.h"

#define BUTTON_PIN 20

#define HUB75_STACK_SIZE 20000
#define BUTTON_STACK_SIZE 128
#define SD_CARD_STACK_SIZE 256

static bool hub75_on = true;
static uint8_t sd_buffer[64 * 64 * 2];

void hub75_task(__unused void *params);
void sd_card_task(__unused void *params);
void button_task(__unused void *params);

StaticTask_t hub75_tcb;
StackType_t hub75_stack[HUB75_STACK_SIZE];
StaticTask_t button_tcb;
StackType_t button_stack[BUTTON_STACK_SIZE];
StaticTask_t sd_card_tcb;
StackType_t sd_card_stack[SD_CARD_STACK_SIZE];

void main(void){
    stdio_init_all();

    xTaskCreateStatic(hub75_task, "", HUB75_STACK_SIZE, NULL, 1, hub75_stack, &hub75_tcb);
    xTaskCreateStatic(sd_card_task, "", SD_CARD_STACK_SIZE, NULL, 1, sd_card_stack, &sd_card_tcb);
    xTaskCreateStatic(button_task, "", BUTTON_STACK_SIZE, NULL, 1, button_stack, &button_tcb);

    vTaskStartScheduler();
}

void hub75_task(__unused void *params){
    hub75_configure();

    hub75_load_image((uint16_t *)sd_buffer);

    while(1){
        if(hub75_on){
            hub75_refresh();
        }
        taskYIELD();
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

void sd_card_task(__unused void *params){
    
    _Bool success;
    do{
        success = sd_card_init();
        vTaskDelay(1);
    } while(!success);

    // temporarily use the sd buffer for reading the control block
    sd_card_read_block(0, sd_buffer, 512);
    uint32_t num_frames = (uint32_t)sd_buffer[0];


    TickType_t last_time = xTaskGetTickCount();

    uint32_t frame_num = 0;

    while(1){

        vTaskSuspendAll();
        if(!sd_card_read_blocks((16 * frame_num + 1), 16, sd_buffer)){
            _Bool success;
            do{
                success = sd_card_init();
                vTaskDelay(1);
            } while(!success);
        }
        xTaskResumeAll();

        frame_num++;
        if(frame_num == num_frames){
            frame_num = 0;
        }

        vTaskDelayUntil(&last_time, pdMS_TO_TICKS(33));
    }

}