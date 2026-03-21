#ifndef HUB75_TEXT_H
#define HUB75_TEXT_H

#include <stdint.h>

typedef enum {ALIGN_CENTER, ALIGN_LEFT, ALIGN_RIGHT, ALIGN_TOP, ALIGN_BOTTOM} alignment;

// Small text is 4x6
void hub75_write_small_text(const char *text, int16_t x, int16_t y, 
                            alignment h_align, alignment v_align, uint16_t color_565);

// Medium text is 6x8
void hub75_write_medium_text(const char *text, int16_t x, int16_t y, 
                            alignment h_align, alignment v_align, uint16_t color_565);

// Large text is 8x16
void hub75_write_large_text(const char *text, int16_t x, int16_t y, 
                            alignment h_align, alignment v_align, uint16_t color_565);
#endif
