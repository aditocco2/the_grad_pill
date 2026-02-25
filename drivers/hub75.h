#ifndef HUB75_H
#define HUB75_H

#include <stdint.h>

void hub75_configure();
void hub75_set_refresh_cb(void (*callback)());
uint16_t *hub75_get_back_buffer();
void hub75_load_image();
void hub75_set_pixel(uint8_t x, uint8_t y, uint16_t rgb565);
void hub75_update();

#endif