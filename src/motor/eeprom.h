#ifndef EEPROM_H
#define EEPROM_H

#include <stdint.h>

#define EEPROM_BASE_ADDR    0x4000
#define EEPROM_SIZE         1024
#define EEPROM_PAGE_SIZE      64

void eeprom_init(void);
void eeprom_periodic_save(void);
uint16_t eeprom_crc16(uint8_t *data, uint8_t len);

#endif