#ifndef HUB75_H
#define HUB75_H

#include <stdint.h>

//-----------User Config-----------

#define WIDTH 64
#define HEIGHT 64

#define RED_0 0
#define ROW_A 6
#define NUM_ROW_PINS 5
#define CLK 11
#define LATCH 12
#define OEN 13

#define PIO_BLOCK pio0
#define DATA_SM 0
#define ROW_SM 1

//---------------------------------

void hub75_configure();
void hub75_set_refresh_cb(void (*callback)());
void hub75_set_update_cb(void (*callback)());
uint16_t *hub75_get_back_buffer();
void hub75_load_image();
void hub75_set_pixel(uint8_t x, uint8_t y, uint16_t rgb565);
void hub75_set_brightness(uint8_t b);
void hub75_update();

#endif