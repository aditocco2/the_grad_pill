/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hub75.pio.h"

#include "mountains_128x64_rgb565.h"

#define DATA_BASE_PIN 0
#define DATA_N_PINS 6
#define ROWSEL_BASE_PIN 6
#define ROWSEL_N_PINS 5
#define CLK_PIN 11
#define STROBE_PIN 12
#define OEN_PIN 13

#define WIDTH 128
#define HEIGHT 64

// pio state machines
#define DATA_SM 0
#define ROW_SM 1

static PIO pio = pio0;
// locations in memory of PIO programs
static uint32_t data_prog_offs;
static uint32_t row_prog_offs;

void hub75_configure();
void hub75_refresh();
static inline uint32_t gamma_correct_565_888(uint16_t pix);

// load image, should go in separate function later
const uint16_t *image = (uint16_t *)mountains;

void main(void){

    stdio_init_all();
    hub75_configure();

    while(1){
        hub75_refresh();
    }
}

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
}

void hub75_refresh(){
    // since this is a "1/32 scan" matrix, it sends two rows at once (0 & 32, 1 & 33, etc.)
    static uint32_t rows_to_send[2][WIDTH];

    // Loop over all row selections (0 to 31)
    for (uint8_t rowsel = 0; rowsel < (HEIGHT / 2); ++rowsel) {

        // Gamma correct every pixel and copy it into rows_to_send
        for (int x = 0; x < WIDTH; ++x) {
            uint16_t top_pixel = (uint16_t)image[WIDTH * (rowsel) + x];
            uint16_t bottom_pixel = (uint16_t)image[WIDTH * (HEIGHT / 2 + rowsel) + x];

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