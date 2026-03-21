#ifndef SD_CARD_H
#define SD_CARD_H

#include <stdbool.h>
#include <stdint.h>

#define BLOCK_SIZE 512

_Bool sd_card_init();
_Bool sd_card_write_block(uint32_t block_addr, const void *buffer, uint16_t buffer_size);
_Bool sd_card_read_block(uint32_t block_addr, uint8_t *buffer, uint16_t buffer_size);
_Bool sd_card_read_blocks(uint32_t block_addr, uint16_t num_blocks, uint8_t* buffer);
_Bool sd_card_check_status();


#endif