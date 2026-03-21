#include <pico/stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include "images/test_gradient.h"

/*
c: perform/time bitplane conversion
b: print bitplanes
*/

#define WIDTH 64
#define HEIGHT 64

uint8_t __attribute__((aligned(4))) buffer[WIDTH * HEIGHT * 4];

absolute_time_t ts;

void make_bitplanes(uint16_t *in_565, uint8_t *out_888);
static inline uint32_t rgb565_to_rgb888(uint16_t pix);

static const uint8_t cie_brightness_table[64] = {
    0,    0,    1,    1,    2,    2,    3,    3,    4,    5,    5,    6,    7,    8,    9,   10,
   12,   13,   14,   16,   18,   20,   22,   24,   26,   28,   31,   33,   36,   39,   42,   45,
   49,   52,   56,   60,   64,   68,   73,   77,   82,   87,   92,   98,  103,  109,  115,  122,
  128,  135,  142,  149,  156,  164,  172,  180,  189,  197,  206,  215,  225,  235,  245,  255
};

int main(){
    stdio_init_all();

    int c;

    while(1){
        c = getchar_timeout_us(0);
        if(c == PICO_ERROR_TIMEOUT){
            continue;
        }
        c = (char)c;
        if(c == 'c'){
            ts = get_absolute_time();
            make_bitplanes(test_gradient, buffer);
            uint64_t time = absolute_time_diff_us(ts, get_absolute_time());
            printf("bitplanes took %llu us\n", time);
        } 
        if(c == 'b'){
            for(int i = 0; i < WIDTH * HEIGHT * 4; i++){
                if(i % 32 == 0){
                    printf("\n%d\t", i);
                }
                printf("%02x ", buffer[i]);
            }
        } 
    }
}

void make_bitplanes(uint16_t *in_565, uint8_t *out_888){

    uint32_t bytes_per_bitplane = WIDTH * HEIGHT/2;
    
    for(uint8_t rowcel = 0; rowcel < HEIGHT/2; rowcel++){
        // process a pixel at row 0, then a pixel at row 31, etc.
        for(uint8_t x = 0; x < WIDTH; x++){
            uint32_t pixel_top = rgb565_to_rgb888(in_565[rowcel * WIDTH + x]);
            uint32_t pixel_bottom = rgb565_to_rgb888(in_565[(HEIGHT/2 + rowcel) * WIDTH + x]);
            // Now each pixel is in the binary form 00000000 BBBBBBBB GGGGGGGG RRRRRRRR
            for(uint8_t bit = 0; bit < 8; bit++){
                uint8_t rgb = (pixel_top >> (bit)      & 0b001)
                            | (pixel_top >> (7 + bit)  & 0b010)
                            | (pixel_top >> (14 + bit) & 0b100)
                            
                            | ((pixel_bottom >> (bit)      & 0b001) << 3)
                            | ((pixel_bottom >> (7 + bit)  & 0b010) << 3)
                            | ((pixel_bottom >> (14 + bit) & 0b100) << 3);
                            // Now each pixel is like 00BGRBGR, with 3 LSBs the top pixel

                uint32_t index = (bytes_per_bitplane * bit) + (WIDTH * rowcel + x);
                out_888[index] = rgb;
            }
        }
    }
}

static inline uint32_t rgb565_to_rgb888(uint16_t pix){

    // 00000000 00000000 RRRRRGGG GGGBBBBB -> 00000000 BBBBB000 GGGGGG00 RRRRR000
    // Linear map first to 666
    uint8_t r = ((pix & 0xF800) >> 11 << 1);
    uint8_t g = ((pix & 0x07E0) >> 5 << 0);
    uint8_t b = ((pix & 0x001F) >> 0 << 1);
    // Then light-map to 888
    r = cie_brightness_table[r];
    g = cie_brightness_table[g];
    b = cie_brightness_table[b];
    return (b << 16) | (g << 8) | (r << 0);
}