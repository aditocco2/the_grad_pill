#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOSConfig.h"
#include <pico/stdlib.h>
#include <stdio.h>
#include "hub75.h"
#include "images/ecd1015.h"

#define BUTTON_PIN 20

#define HUB75_STACK_SIZE 20000
#define BUTTON_STACK_SIZE 128

static bool hub75_on = true;

void hub75_task(__unused void *params);
void button_task(__unused void *params);

StaticTask_t hub75_tcb;
StackType_t hub75_stack[HUB75_STACK_SIZE];
StaticTask_t button_tcb;
StackType_t button_stack[BUTTON_STACK_SIZE];

void main(void){
    stdio_init_all();

    xTaskCreateStatic(hub75_task, "HUB75Thread", HUB75_STACK_SIZE, NULL, 1, hub75_stack, &hub75_tcb);
    xTaskCreateStatic(button_task, "ButtonThread", BUTTON_STACK_SIZE, NULL, 1, button_stack, &button_tcb);

    vTaskStartScheduler();
}

void hub75_task(__unused void *params){
    hub75_configure();
    hub75_load_image((uint16_t*)ecd1015);

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
    
    while(1){
        previous_state = current_state;
        current_state = !gpio_get(BUTTON_PIN);
    
        if(current_state && !previous_state){
            hub75_on = !hub75_on;
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }

}