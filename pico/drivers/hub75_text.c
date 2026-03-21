#include <stdint.h>
#include <string.h>
#include "hub75.h"
#include "hub75_text.h"
#include "hub75_fonts.h"

#define SMALL_FONT_WIDTH 4
#define SMALL_FONT_HEIGHT 6

#define MEDIUM_FONT_WIDTH 6
#define MEDIUM_FONT_HEIGHT 8

#define LARGE_FONT_WIDTH 8
#define LARGE_FONT_HEIGHT 16

void hub75_write_small_text(const char *text, int16_t x, int16_t y, 
                            alignment h_align, alignment v_align, uint16_t color_565);

void hub75_write_medium_text(const char *text, int16_t x, int16_t y, 
                            alignment h_align, alignment v_align, uint16_t color_565);

void hub75_write_large_text(const char *text, int16_t x, int16_t y, 
                            alignment h_align, alignment v_align, uint16_t color_565);

static void write_text(const char *text, int16_t x, int16_t y, const uint8_t *font,
                uint8_t width, uint8_t height, alignment h_align, alignment v_align, 
                uint16_t color_565);

static void draw_char(char c, uint8_t x_left, uint8_t y_top, const uint8_t *font,
                      uint8_t width, uint8_t height, uint16_t color_565);

void hub75_write_small_text(const char *text, int16_t x, int16_t y, 
                            alignment h_align, alignment v_align, uint16_t color_565){

    write_text(text, x, y, small_font, SMALL_FONT_WIDTH, SMALL_FONT_HEIGHT,
               h_align, v_align, color_565);
}

void hub75_write_medium_text(const char *text, int16_t x, int16_t y, 
                            alignment h_align, alignment v_align, uint16_t color_565){

    write_text(text, x, y, medium_font, MEDIUM_FONT_WIDTH, MEDIUM_FONT_HEIGHT,
               h_align, v_align, color_565);
}

void hub75_write_large_text(const char *text, int16_t x, int16_t y, 
                            alignment h_align, alignment v_align, uint16_t color_565){
                                
    write_text(text, x, y, large_font, LARGE_FONT_WIDTH, LARGE_FONT_HEIGHT, 
               h_align, v_align, color_565);
}

static void write_text(const char *text, int16_t x, int16_t y, const uint8_t *font,
                uint8_t width, uint8_t height, alignment h_align, alignment v_align, 
                uint16_t color_565){
    
    uint8_t length = strlen(text);

    int16_t x_left, y_top;
    switch(h_align){
        case ALIGN_LEFT:
            x_left = x;
            break;
        case ALIGN_CENTER:
            x_left = x - (width * length / 2);
            break;
        case ALIGN_RIGHT:
            x_left = x - (width * length);
            break;
        default:
            x_left = 0;
            break;
    }
    switch(v_align){
        case ALIGN_TOP:
            y_top = y;
            break;
        case ALIGN_CENTER:
            y_top = y - (height / 2);
            break;
        case ALIGN_BOTTOM:
            y_top = y - height;
            break;
        default:
            y_top = 0;
            break;
    }

    int16_t x_current = x_left;
    int16_t y_current = y_top;
    for(uint8_t i = 0; i < length; i++){
        draw_char(text[i], x_current, y_current, font, width, height, color_565);
        x_current += width;
    }
}

static void draw_char(char c, uint8_t x_left, uint8_t y_top, const uint8_t *font,
                      uint8_t width, uint8_t height, uint16_t color_565){

    // convert to an index in the array
    c = c & 0x7F;
    if (c < ' '){
        c = 0;
    }
    else{
        c -= ' ';
    }

    const uint8_t *char_array = font + (c * height);

    for(uint8_t y = 0; y < height; y++){
        for(uint8_t x = 0; x < width; x++){
            if(char_array[y] & (1 << (width - x))){   
                hub75_set_pixel(x_left + x, y_top + y, color_565);
            }
        }
    }
}