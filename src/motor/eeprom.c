#include "main.h"
#include "stm8s_conf.h"
#include "eeprom.h"

void eeprom_init(void)
{
    /* Initialize EEPROM data */
    eeprom_dirty = 0;
    eeprom_last_save_time = 0;
    
    /* Read position data from EEPROM */
    uint8_t data[10]; /* 8 bytes for positions + 2 bytes for checksum */
    
    /* Read data from EEPROM */
    uint16_t addr = EEPROM_BASE_ADDR;
    
    /* Read 10 bytes (8 bytes for each motor position + 2 bytes for checksum) */
for (int i = 0; i < 10; i++)
    {
        data[i] = *((uint8_t*)(addr + i));
    }
    
    /* Check checksum */
    uint16_t checksum = eeprom_crc16(data, 8);
    if (checksum != ((data[8] << 8) | data[9]))
    {
        /* Invalid data, set positions to zero */
        steppers[0].position = 0;
        steppers[1].position = 0;
    }
    else
    {
        /* Valid data, use it */
        steppers[0].position = ((int32_t)data[0] << 24) | ((int32_t)data[1] << 16) | ((int32_t)data[2] << 8) | (int32_t)data[3];
        steppers[1].position = ((int32_t)data[4] << 24) | ((int32_t)data[5] << 16) | ((int32_t)data[6] << 8) | (int32_t)data[7];
    }
}

void eeprom_periodic_save(void)
{
    uint32_t now;
    uint8_t data[10]; /* 8 bytes for positions + 2 bytes for checksum */
    int32_t pos0, pos1;
    uint16_t checksum;
    uint16_t addr = EEPROM_BASE_ADDR;
    int i;

    /* Atomic snapshot of the 32-bit tick (also written by the TIM4 ISR) */
    sim();
    now = system_ticks;
    rim();

    if (!eeprom_dirty || (now - eeprom_last_save_time) <= 1000)
        return;

    /* Never program while a motor is stepping. On STM8 the programming stall
       (up to ~30 ms for 10 bytes) stops the TIM4 update ISR, which would skip
       step generation (the DDA) and the nFAULT check, and would also block a
       safety STOP. STATUS_ERROR is not RUNNING, so error positions are saved. */
    if (steppers[0].state == STATUS_RUNNING || steppers[1].state == STATUS_RUNNING)
        return;

    /* Atomic snapshot of the 32-bit positions (written by the TIM4 ISR) */
    sim();
    pos0 = steppers[0].position;
    pos1 = steppers[1].position;
    rim();

    data[0] = (pos0 >> 24) & 0xFF;
    data[1] = (pos0 >> 16) & 0xFF;
    data[2] = (pos0 >> 8) & 0xFF;
    data[3] = pos0 & 0xFF;

    data[4] = (pos1 >> 24) & 0xFF;
    data[5] = (pos1 >> 16) & 0xFF;
    data[6] = (pos1 >> 8) & 0xFF;
    data[7] = pos1 & 0xFF;

    /* Calculate checksum */
    checksum = eeprom_crc16(data, 8);
    data[8] = (checksum >> 8) & 0xFF;
    data[9] = checksum & 0xFF;

    /* Write data to EEPROM */
    FLASH_Unlock(FLASH_MEMTYPE_DATA);

    for (i = 0; i < 10; i++)
    {
        FLASH_ProgramByte(addr + i, data[i]);
        FLASH_WaitForLastOperation(FLASH_MEMTYPE_DATA);
    }

    FLASH_Lock(FLASH_MEMTYPE_DATA);

    /* Mark as clean */
    eeprom_dirty = 0;
    eeprom_last_save_time = now;
}

uint16_t eeprom_crc16(uint8_t *data, uint8_t len)
{
    uint16_t crc = 0xFFFF;
    uint8_t i;
    
    while (len--)
    {
        crc ^= *data++;
        for (i = 0; i < 8; i++)
        {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    
    return crc;
}