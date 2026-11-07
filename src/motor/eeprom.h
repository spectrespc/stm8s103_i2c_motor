#ifndef EEPROM_H
#define EEPROM_H

#include <stdint.h>

#define EEPROM_BASE_ADDR    0x4000
/* STM8S003F3P6 (the chip on both boards) has 128 bytes of data EEPROM.  The
 * old 1024 value matched neither part (the STM8S103F3P6 has 640 bytes).  The
 * firmware only uses the first 10 bytes at EEPROM_BASE_ADDR, so runtime code
 * is unaffected. */
#define EEPROM_SIZE          128
#define EEPROM_PAGE_SIZE      64

void eeprom_init(void);
void eeprom_periodic_save(void);
uint16_t eeprom_crc16(uint8_t *data, uint8_t len);

#endif