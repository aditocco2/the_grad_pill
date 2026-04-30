#ifndef HUB75_TEXT_H
#define HUB75_TEXT_H

#include <stdint.h>

#define SMALL_FONT_WIDTH 4
#define SMALL_FONT_HEIGHT 6

#define MEDIUM_FONT_WIDTH 6
#define MEDIUM_FONT_HEIGHT 8

#define LARGE_FONT_WIDTH 8
#define LARGE_FONT_HEIGHT 16

typedef enum {ALIGN_CENTER, ALIGN_LEFT, ALIGN_RIGHT, ALIGN_TOP, ALIGN_BOTTOM} alignment;

void hub75_write_small_text(const char *text, int16_t x, int16_t y, 
                            alignment h_align, alignment v_align, uint16_t color_565);

void hub75_write_medium_text(const char *text, int16_t x, int16_t y, 
                            alignment h_align, alignment v_align, uint16_t color_565);

void hub75_write_large_text(const char *text, int16_t x, int16_t y, 
                            alignment h_align, alignment v_align, uint16_t color_565);
#endif
