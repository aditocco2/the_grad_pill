// Basic HUB75 driver, adapted from Pico example
// https://github.com/raspberrypi/pico-examples/tree/master/pio/hub75

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hub75.pio.h"

#define DATA_BASE_PIN 0
#define DATA_N_PINS 6
#define ROWSEL_BASE_PIN 6
#define ROWSEL_N_PINS 5
#define CLK_PIN 11
#define STROBE_PIN 12
#define OEN_PIN 13

#define WIDTH 64
#define HEIGHT 64

// pio state machines
#define DATA_SM 0
#define ROW_SM 1

static PIO pio = pio0;
// locations in memory of PIO programs
static uint32_t data_prog_offs;
static uint32_t row_prog_offs;

// this one is edited on
static uint16_t back_buffer[WIDTH * HEIGHT];
// this one is actually sent to the screen
static uint16_t front_buffer[WIDTH * HEIGHT];

// DMA channels
int dma_channel_back, dma_channel_front;
dma_channel_config dma_config_back, dma_config_front;

void hub75_configure();
void hub75_load_image(uint16_t *image_pointer);
void *hub75_get_back_buffer();
void hub75_refresh();
static inline uint32_t gamma_correct_565_888(uint16_t pix);

static inline uint32_t gamma_correct_565_888(uint16_t pix) {
    uint32_t r_gamma = pix & 0xf800u;
    r_gamma *= r_gamma;
    uint32_t g_gamma = pix & 0x07e0u;
    g_gamma *= g_gamma;
    uint32_t b_gamma = pix & 0x001fu;
    b_gamma *= b_gamma;
    return (b_gamma >> 2 << 16) | (g_gamma >> 14 << 8) | (r_gamma >> 24 << 0);
}

void hub75_configure(){
    // Get the locations (offsets) of the PIO programs in memory
    data_prog_offs = pio_add_program(pio, &hub75_data_rgb888_program);
    row_prog_offs = pio_add_program(pio, &hub75_row_program);

    hub75_data_rgb888_program_init(pio, DATA_SM, data_prog_offs, DATA_BASE_PIN, CLK_PIN);
    hub75_row_program_init(pio, ROW_SM, row_prog_offs, ROWSEL_BASE_PIN, ROWSEL_N_PINS, STROBE_PIN);

    // Also setup DMA controllers to later copy between buffers
    dma_channel_back = dma_claim_unused_channel(true);
    dma_config_back = dma_channel_get_default_config(dma_channel_back);
    channel_config_set_transfer_data_size(&dma_config_back, DMA_SIZE_16);
    channel_config_set_read_increment(&dma_config_back, true);
    channel_config_set_write_increment(&dma_config_back, true);

    dma_channel_front = dma_claim_unused_channel(true);
    dma_config_front = dma_channel_get_default_config(dma_channel_front);
    channel_config_set_transfer_data_size(&dma_config_front, DMA_SIZE_16);
    channel_config_set_read_increment(&dma_config_front, true);
    channel_config_set_write_increment(&dma_config_front, true);
}

void hub75_load_image(uint16_t *image_pointer){
    // copy from image pointer to back buffer
    dma_channel_configure(
        dma_channel_back, &dma_config_back,
        back_buffer, image_pointer,
        WIDTH * HEIGHT, true
    );
}

void *hub75_get_back_buffer(){
    return back_buffer;
}

void hub75_push(){
    // configures DMA to copy from back buffer to front buffer
    dma_channel_configure(
        dma_channel_front, &dma_config_front,
        front_buffer, back_buffer,
        WIDTH * HEIGHT, true
    );
}

void hub75_refresh(){
    // since this is a "1/32 scan" matrix, it sends two rows at once (0 & 32, 1 & 33, etc.)
    static uint32_t rows_to_send[2][WIDTH];

    // Loop over all row selections (0 to 31)
    for (uint8_t rowsel = 0; rowsel < (HEIGHT / 2); ++rowsel) {

        // Gamma correct every pixel and copy it into rows_to_send
        for (int x = 0; x < WIDTH; ++x) {
            uint16_t top_pixel = (uint16_t)front_buffer[WIDTH * (rowsel) + x];
            uint16_t bottom_pixel = (uint16_t)front_buffer[WIDTH * (HEIGHT / 2 + rowsel) + x];

            rows_to_send[0][x] = gamma_correct_565_888(top_pixel);
            rows_to_send[1][x] = gamma_correct_565_888(bottom_pixel);
        }

        // This driver does binary coded modulation, so each individual bit must be processed
        // Data must be sent to PIO SM 8 times
        for (int bit = 0; bit < 8; ++bit) {
            // Tells the state machine which bit to process
            hub75_data_rgb888_set_shift(pio, DATA_SM, data_prog_offs, bit);

            // Sends the pixel data over
            for (int x = 0; x < WIDTH; ++x) {
                pio_sm_put_blocking(pio, DATA_SM, rows_to_send[0][x]);
                pio_sm_put_blocking(pio, DATA_SM, rows_to_send[1][x]);
            }

            // Dummy pixel per lane
            pio_sm_put_blocking(pio, DATA_SM, 0);
            pio_sm_put_blocking(pio, DATA_SM, 0);
            // SM is finished when it stalls on empty TX FIFO
            hub75_wait_tx_stall(pio, DATA_SM);
            // Also check that previous OEn pulse is finished, else things can get out of sequence
            hub75_wait_tx_stall(pio, ROW_SM);

            // Latch row data, pulse output enable for new row.
            pio_sm_put_blocking(pio, ROW_SM, rowsel | (100u * (1u << bit) << 5));
        }
    }
}