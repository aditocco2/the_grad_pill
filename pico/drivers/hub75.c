// HUB75/HUB75E Driver: Lightwork Edition
// Uses 4 DMA channels and a PIO block for CPU-free refresh
// Takes RGB565 input and lightmaps to RGB888

// Assumes the panel is split up into two scan areas with 6 RGB pins

#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hub75.h"
#include "hub75.pio.h"

#define BYTES_PER_BITPLANE ((WIDTH * HEIGHT) / 2)
#define RGB_N_PINS 6

// RGB565 pixels
static uint16_t back_buffer[WIDTH * HEIGHT];
// RGB888 bitplanes
static uint32_t front_buffer_a[WIDTH * HEIGHT];
static uint32_t front_buffer_b[WIDTH * HEIGHT];
// pointers to swap for double buffering
static uint32_t *active_buffer;
static uint32_t *inactive_buffer;

// Pulse widths for sending to Row SM
static uint32_t pw_table[] = {10, 20, 40, 80, 160, 320, 640, 1280};

// 6-bit to 8-bit color mapping
static const uint8_t cie_brightness_table[64] = {
    0,    0,    1,    1,    2,    2,    3,    3,    4,    5,    5,    6,    7,    8,    9,   10,
   12,   13,   14,   16,   18,   20,   22,   24,   26,   28,   31,   33,   36,   39,   42,   45,
   49,   52,   56,   60,   64,   68,   73,   77,   82,   87,   92,   98,  103,  109,  115,  122,
  128,  135,  142,  149,  156,  164,  172,  180,  189,  197,  206,  215,  225,  235,  245,  255
};

// DMA channels to write pixel data / pulse widths to PIO
int rgb_channel, pw_channel, rgb_reload_channel, pw_reload_channel;
dma_channel_config rgb_config, pw_config, rgb_reload_config, pw_reload_config;

// Core0 -> Core1 notification to make bitplanes
int update_doorbell;

static PIO pio = PIO_BLOCK;
static uint32_t data_prog_offset, row_prog_offset;

void hub75_configure();
void hub75_set_refresh_cb(void (*callback)());
void hub75_set_update_cb(void (*callback)());
uint16_t *hub75_get_back_buffer();
void hub75_load_image();
void hub75_set_pixel(uint8_t x, uint8_t y, uint16_t rgb565);
void hub75_set_brightness(uint8_t b);
void hub75_update();

static void configure_all_dma();
static void dma_handler();
static void (*refresh_cb)();
static void (*update_cb)();
static void core1_entry();
static void core1_doorbell_handler();
static void make_bitplanes(uint16_t *in_565, uint8_t *out_888);
static inline uint32_t rgb565_to_rgb888(uint16_t pix);


void hub75_configure(){
    // Get the locations of the PIO programs in memory
    data_prog_offset = pio_add_program(pio, &hub75_data_program);
    row_prog_offset = pio_add_program(pio, &hub75_row_program);

    // initialize PIO programs, passing in screen width/height
    hub75_data_program_init(pio, DATA_SM, data_prog_offset, RED_0, CLK, WIDTH);
    hub75_row_program_init(pio, ROW_SM, row_prog_offset, ROW_A, NUM_ROW_PINS, LATCH, HEIGHT);

    // initialize pointers to swap later for double buffering
    active_buffer = front_buffer_a;
    inactive_buffer = front_buffer_b;

    configure_all_dma();

    multicore_launch_core1(core1_entry);
}

uint16_t *hub75_get_back_buffer(){
    return back_buffer;
}

void hub75_set_refresh_cb(void (*callback)()){
    // set internal callback to passed-in one
    refresh_cb = callback;
    // Make reload DMA trigger an IRQ on completion
    dma_channel_set_irq0_enabled(rgb_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true); 
}

void hub75_set_update_cb(void (*callback)()){
    // set internal callback to passed-in one
    update_cb = callback;
}

void hub75_load_image(uint16_t * image_pointer){
    memcpy(back_buffer, image_pointer, WIDTH * HEIGHT * 2);
}

void hub75_set_pixel(uint8_t x, uint8_t y, uint16_t rgb565){
    if((x >= 0) && (y >= 0) && (x < WIDTH) && (y < HEIGHT)){
        back_buffer[y * WIDTH + x] = rgb565;
    }
    
}

void hub75_set_brightness(uint8_t b){
    for(uint8_t i = 0; i < 8; i++){
        pw_table[i] = b << i;
    }
}

void hub75_update(){
    // tell core 1 to make bitplanes and update the display
    multicore_doorbell_set_other_core(update_doorbell);
}

static void configure_all_dma(){
    rgb_channel = dma_claim_unused_channel(true);
    rgb_config = dma_channel_get_default_config(rgb_channel);
    channel_config_set_transfer_data_size(&rgb_config, DMA_SIZE_32);
    channel_config_set_read_increment(&rgb_config, true);
    channel_config_set_write_increment(&rgb_config, false);
    channel_config_set_dreq(&rgb_config, pio_get_dreq(pio, DATA_SM, true));

    pw_channel = dma_claim_unused_channel(true);
    pw_config = dma_channel_get_default_config(pw_channel);
    channel_config_set_transfer_data_size(&pw_config, DMA_SIZE_32);
    channel_config_set_read_increment(&pw_config, true);
    channel_config_set_write_increment(&pw_config, false);
    channel_config_set_dreq(&pw_config, pio_get_dreq(pio, ROW_SM, true));

    rgb_reload_channel = dma_claim_unused_channel(true);
    rgb_reload_config = dma_channel_get_default_config(rgb_reload_channel);
    channel_config_set_transfer_data_size(&rgb_reload_config, DMA_SIZE_32);
    channel_config_set_read_increment(&rgb_reload_config, false);
    channel_config_set_write_increment(&rgb_reload_config, false);

    pw_reload_channel = dma_claim_unused_channel(true);
    pw_reload_config = dma_channel_get_default_config(pw_reload_channel);
    channel_config_set_transfer_data_size(&pw_reload_config, DMA_SIZE_32);
    channel_config_set_read_increment(&pw_reload_config, false);
    channel_config_set_write_increment(&pw_reload_config, false);

    // Setup chaining between them (important to do this after all channels defined)
    channel_config_set_chain_to(&rgb_config, rgb_reload_channel);
    channel_config_set_chain_to(&pw_config, pw_reload_channel);

    // Make RGB DMA copy from active front buffer to Data SM FIFO
    dma_channel_configure(
        rgb_channel, &rgb_config,
        &pio->txf[DATA_SM], active_buffer,
        WIDTH * HEIGHT, false
    );

    // Make PW DMA copy from pulse buffer to Row SM FIFO
    dma_channel_configure(
        pw_channel, &pw_config,
        &pio->txf[ROW_SM], pw_table,
        8, false // 8 pulse widths for 8 bits
    );

    // Make RGB Reloader copy front buffer's address to RGB read address register
    dma_channel_configure(
        rgb_reload_channel, &rgb_reload_config,
        &dma_hw->ch[rgb_channel].al3_read_addr_trig, &active_buffer,
        1, true
    );

    // Make PW Reloader copy pulse table's address to PW read address register
    static uint32_t pulse_addr = (uint32_t)pw_table;
    dma_channel_configure(
        pw_reload_channel, &pw_reload_config,
        &dma_hw->ch[pw_channel].al3_read_addr_trig, &pulse_addr, 
        1, true
    );

    // Start the data channels, which chain to the reloader channels
    dma_channel_start(pw_channel);
    dma_channel_start(rgb_channel);
}

static void dma_handler(){
    dma_irqn_acknowledge_channel(0, rgb_channel);
    (*refresh_cb)();
}

static void core1_entry(){

    // Sets up a doorbell (notification) for hub75_update to poke core 1
    update_doorbell = multicore_doorbell_claim_unused(1 << 1, true);
    irq_set_exclusive_handler(multicore_doorbell_irq_num(update_doorbell), core1_doorbell_handler);
    irq_set_enabled(multicore_doorbell_irq_num(update_doorbell), true);

    while(1){
        __wfe();
    }
}

static void core1_doorbell_handler(){

    if(multicore_doorbell_is_set_current_core(update_doorbell)){
        
        multicore_doorbell_clear_current_core(update_doorbell);

        make_bitplanes(back_buffer, (uint8_t *)inactive_buffer);

        uint32_t *temp;
        temp = active_buffer;
        active_buffer = inactive_buffer;
        inactive_buffer = temp;

        if(update_cb){
            update_cb();
        }
    }
}

static void make_bitplanes(uint16_t *in_565, uint8_t *out_888){
    
    for(uint8_t rowsel = 0; rowsel < HEIGHT/2; rowsel++){
        // process a pixel at row 0, then a pixel at row 31, etc.
        for(uint8_t x = 0; x < WIDTH; x++){
            uint32_t pixel_top = rgb565_to_rgb888(in_565[rowsel * WIDTH + x]);
            uint32_t pixel_bottom = rgb565_to_rgb888(in_565[(HEIGHT/2 + rowsel) * WIDTH + x]);
            // Now each pixel is in the binary form 00000000 BBBBBBBB GGGGGGGG RRRRRRRR
            for(uint8_t bit = 0; bit < 8; bit++){
                uint8_t rgb = (pixel_top >> (bit)      & 0b001)
                            | (pixel_top >> (7 + bit)  & 0b010)
                            | (pixel_top >> (14 + bit) & 0b100)
                            
                            | ((pixel_bottom >> (bit)      & 0b001) << 3)
                            | ((pixel_bottom >> (7 + bit)  & 0b010) << 3)
                            | ((pixel_bottom >> (14 + bit) & 0b100) << 3);
                            // Now each pixel is like 00BGRBGR, with 3 LSBs the top pixel

                uint32_t index = (BYTES_PER_BITPLANE * bit) + (WIDTH * rowsel + x);
                out_888[index] = rgb;
            }
        }
    }
}

static inline uint32_t rgb565_to_rgb888(uint16_t pix){
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