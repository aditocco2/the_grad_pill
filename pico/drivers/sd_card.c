// Basic SD card driver for SPI
// Adapted from Yaseen Twati's guide:
// https://web.archive.org/web/20250324102712/http://yaseen.ly/writing-data-to-sdcards-without-a-filesystem-spi/

#include "sd_card.h"

#include <stdbool.h>
#include <stdint.h>
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "pico/time.h"

#include <stdio.h>
#include <pico/stdlib.h>

// #define DEBUG

#define MISO 16
#define CS 17
#define SCLK 18
#define MOSI 19

#define INIT_RATE 250000
#define DATA_RATE 20000000
#define TIMEOUT_MS 300

// spi commands need pointers to dummy data so here they are
static const uint8_t high = 0xFF;
static const uint8_t zero = 0x00;
static uint8_t dummy;

// timer for timeout shenanigans
absolute_time_t start_time;

_Bool sd_card_init();
_Bool sd_card_write_block(uint32_t block_addr, const void *buffer, uint16_t buffer_size);
_Bool sd_card_read_block(uint32_t block_addr, uint8_t *buffer, uint16_t buffer_size);
_Bool sd_card_read_blocks(uint32_t block_addr, uint16_t num_blocks, uint8_t* buffer);
_Bool sd_card_check_status();

static void send_cmd(uint8_t cmd, uint32_t arg, uint8_t crc);
static uint8_t read_response();
static _Bool wait_card_busy();
static void send_dummy_byte();
static void read_dummy_crc();
static _Bool wait_for_data_start();
static void reset_timer();
static _Bool timer_exceeds_us(uint32_t us);
static _Bool timeout();

// initialize card, assuming V2
// sequence:
// CMD0 -> 0x01
// CMD8 -> 0x01
// CMD55 & CMD41 -> 0x00 (loop until success)
_Bool sd_card_init(){
    
    gpio_init(CS);
    gpio_set_dir(CS, GPIO_OUT);
    gpio_put(CS, 1);

    gpio_set_function(MISO, GPIO_FUNC_SPI);
    gpio_set_function(SCLK, GPIO_FUNC_SPI);
    gpio_set_function(MOSI, GPIO_FUNC_SPI);
    gpio_pull_up(MISO);

    // must be <400KHz during init
    spi_init(spi0, INIT_RATE);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    // hold CS high for at least 74 clock cycles to switch the card to native mode
    for(uint8_t i = 0; i < 10; i++){
        send_dummy_byte();
    }
    gpio_put(CS, 0);
    send_dummy_byte();

    // reset into idle state
    send_cmd(0, 0, 0x95);
    if(read_response() != 0x01){
        #ifdef DEBUG 
        printf("SD card did not respond to CMD0\n"); 
        #endif
        gpio_put(CS, 1);
        return false;
    }

    gpio_put(CS, 1);
    send_dummy_byte();
    gpio_put(CS, 0);

    // verify V2
    send_cmd(8, 0x01AA, 0x87);
    if(read_response() != 0x01){
        #ifdef DEBUG 
        printf("SD card did not respond to CMD8\n"); 
        #endif
        gpio_put(CS, 1);
        return false;
    }

    gpio_put(CS, 1);
    send_dummy_byte();   

    // high capacity mode may take a few tries
    reset_timer();
    while(1){

        gpio_put(CS, 0);
        send_dummy_byte();

        // inform the card next command is an ACMD
        send_cmd(55, 0, 0x65);

        for(uint8_t d = 0; d < 4; d++){
            send_dummy_byte(); 
        }
            
        // set high capacity mode, if we get 0 it was successful
        send_cmd(41, 0x40000000, 0x77);
        if(read_response() == 0x00){
            break;
        }

        if(timeout()){
            #ifdef DEBUG 
            printf("Timeout waiting for CMD41/CMD55 chain\n"); 
            #endif
            gpio_put(CS, 1);
            return false;
        }

        gpio_put(CS, 1);
    }

    gpio_put(CS, 1);
    send_dummy_byte();

    // now that init is over, we can up the bitrate
    spi_set_baudrate(spi0, DATA_RATE);

    return true;
}

_Bool sd_card_read_block(uint32_t block_addr, uint8_t *buffer, uint16_t buffer_size){

    gpio_put(CS, 0);
    send_dummy_byte();
    
    // CMD17 for single block read
    send_cmd(17, block_addr, 0x01);
    if(read_response() != 0x00){
        #ifdef DEBUG 
        printf("SD card did not respond to CMD17\n"); 
        #endif
        gpio_put(CS, 1);
        return false;
    }

    if(!wait_for_data_start()){
        return false;
    }

    // read the data into the buffer
    spi_read_blocking(spi0, 0xFF, buffer, buffer_size);

    // must read the entire 512 byte block, so flush out dummy data
    for(uint16_t i = buffer_size; i < BLOCK_SIZE; i++){
        spi_read_blocking(spi0, 0xFF, &dummy, buffer_size);
    }

    read_dummy_crc();

    gpio_put(CS, 1);
    send_dummy_byte();
    return true;

}


_Bool sd_card_read_blocks(uint32_t block_addr, uint16_t num_blocks, uint8_t *buffer){
    
    gpio_put(CS, 0);
    
    // send CMD18 to read multiblock
    send_cmd(18, block_addr, 0x57);
    if(read_response() != 0x00){
        #ifdef DEBUG 
        printf("SD card did not respond to CMD18\n"); 
        #endif
        gpio_put(CS, 1);
        return false;
    }

    // sd card still gives a data start token between blocks, so we
    // must filter it out every 512 reads
    for(uint16_t i = 0; i < num_blocks; i++){
        if(!wait_for_data_start()){
            return false;
        }
        // read real data
        spi_read_blocking(spi0, 0xFF, buffer + (BLOCK_SIZE * i), BLOCK_SIZE);
        read_dummy_crc();
    }

    // stop transaction or something
    send_cmd(12, 0, 0x01);

    read_dummy_crc();

    gpio_put(CS, 1);
    send_dummy_byte();
    return true;

}


_Bool sd_card_write_block(uint32_t block_addr, const void *buffer, uint16_t buffer_size){

    gpio_put(CS, 0);

    // send CMD24 to write a block
    send_cmd(24, block_addr, 0x01);
    if(read_response() != 0x00){
        #ifdef DEBUG 
        printf("SD card did not respond to CMD17\n"); 
        #endif
        gpio_put(CS, 1);
        return false;
    }

    uint8_t data_start = 0xFE;
    spi_write_blocking(spi0, &data_start, 1);

    // write the buffer to the SD card
    spi_write_blocking(spi0, buffer, buffer_size);

    // write a bunch of dummy data if the buffer is under 512 bytes
    for(uint16_t i = buffer_size; i < BLOCK_SIZE; i++){
        spi_write_blocking(spi0, &zero, 1);
    }

    send_dummy_byte();
    send_dummy_byte();

    gpio_put(CS, 1);
    send_dummy_byte();
    return true;
}


_Bool sd_card_check_status(){
    // send CMD13 to check for a response, return true if still alive
    gpio_put(CS, 0);
    send_cmd(13, 0, 0x95);
    if(read_response() != 0x00){
        #ifdef DEBUG 
        printf("SD card did not respond to CMD13\n"); 
        #endif
        gpio_put(CS, 1);
        return false;
    }
    gpio_put(CS, 1);
    return true;
}

static void send_cmd(uint8_t cmd, uint32_t arg, uint8_t crc){
    wait_card_busy();

    uint8_t buf[6] = {0x40 | cmd, arg >> 24, arg >> 16, arg >> 8, arg, crc};

    spi_write_blocking(spi0, buf, 6);
}

static void send_dummy_byte(){
    spi_write_blocking(spi0, &high, 1);
}

static void read_dummy_crc(){
    spi_read_blocking(spi0, 0xFF, &dummy, 2);
}

static void reset_timer(){
    start_time = get_absolute_time();
}

static _Bool timer_exceeds_us(uint32_t time_us){
    uint32_t timer_value_us = absolute_time_diff_us(start_time, get_absolute_time());
    return timer_value_us > time_us;
}

static _Bool timeout(){
    return timer_exceeds_us(TIMEOUT_MS * 1000);
}

// keep going until we get a byte that's not 0xFF (or timeout)
static uint8_t read_response(){
    reset_timer();
    uint8_t response;
    do{
        spi_read_blocking(spi0, 0xFF, &response, 1);
        if(timeout()){
            #ifdef DEBUG 
            printf("Timeout waiting for response\n"); 
            #endif
            break;
        }
    } while(response == 0xFF);
    return response;
}

// card is busy until we get 0xFF again
static _Bool wait_card_busy(){
    uint8_t response = 0xFF;
    reset_timer();
    while(1){
        spi_read_blocking(spi0, 0xFF, &response, 1);
        if(response == 0xFF){
            return true;
        }
        if(timeout()){
            return false;
        }
    }
}

// Reading data requires us to wait for a data start token
static _Bool wait_for_data_start(){
    reset_timer();

    uint8_t r_byte = 0xFF;
    while(r_byte != 0xFE){
        spi_read_blocking(spi0, 0xFF, &r_byte, 1);

        if(timeout()){
            return false;
        }
    }

    return true;
}