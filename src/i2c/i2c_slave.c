/**
  ******************************************************************************
  * @file    i2c_slave.c
  * @author  MCD Application Team
  * @version V2.3.0
  * @date    16-January-2014
  * @brief   I2C slave implementation using SPL
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2014 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm8s_conf.h"
#include "main.h"
#include "i2c_slave.h"
#include "i2c_proto.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ----------------------------------------------------------*/
/* I2C_TIMEOUT_MS is defined once in main.h */

/* Private macro ---------------------------------------------------------*/
/* Private variables -----------------------------------------------------------*/
/* Volatile: written in the I2C ISR and read/updated from the main loop. */
volatile uint8_t i2c_rx_buf[I2C_RX_BUF_SIZE];
volatile uint8_t i2c_tx_buf[I2C_TX_BUF_SIZE];
volatile uint8_t i2c_rx_idx = 0;
volatile uint8_t i2c_tx_idx = 0;
volatile uint8_t i2c_rx_len = 0;   /* bytes received in the last write phase */
volatile uint8_t i2c_state = 0;    /* 0: idle, 1: receiving, 2: transmitting */

/* Private function prototypes -----------------------------------------------*/
void i2c_slave_reset(void);
uint8_t i2c_check_timeout(void);
void i2c_isr_handler(void);
void i2c_prepare_read_response(void);
/* i2c_required_len() is defined in i2c_proto.h (shared with host tests). */

/* Private functions ------------------------------------------------------*/

/**
  * @brief  Reset I2C slave
  * @param  None
  * @retval None
  */
void i2c_slave_reset(void)
{
    /* Reset I2C slave state */
    i2c_rx_idx = 0;
    i2c_tx_idx = 0;
    i2c_rx_len = 0;
    i2c_state = 0;
    i2c_last_activity = system_ticks;
    
    /* Reinitialize I2C */
    I2C_Cmd(DISABLE);
    I2C_DeInit();
    
    /* I2C slave init via direct register access */
    I2C->FREQR = 16;
    I2C->OARL = (uint8_t)((I2C_SLAVE_ADDR << 1) | 0x01); /* ADDCONF=1 */
    I2C->OARH = 0x00;
    I2C->ITR = 0x07; // ERR + EVT + BUF
    I2C->CR1 = 0x01;  // PE=1
    I2C->CR2 = 0x04; // ACK=1
    I2C_Cmd(ENABLE);
}

/**
  * @brief  Check I2C timeout
  * @param  None
  * @retval 1 if timeout occurred, 0 otherwise
  */
uint8_t i2c_check_timeout(void)
{
    uint32_t now, last;

    /* Atomic snapshot: system_ticks is updated by the TIM4 ISR. */
    sim();
    now = system_ticks;
    last = i2c_last_activity;
    rim();

    if (last > 0 && (now - last) > I2C_TIMEOUT_MS)
    {
        i2c_need_reset = 1;
        return 1;
    }
    return 0;
}

/**
  * @brief  I2C interrupt service routine
  * @param  None
  * @retval None
  */
void i2c_isr_handler(void)
{
    uint8_t sr1 = I2C->SR1;      /* read once: decides every branch below */
    uint8_t sr2 = I2C->SR2;

    /* --- Address matched (ADDR = SR1 bit1) --- */
    if (sr1 & 0x02)
    {
        uint8_t sr3 = I2C->SR3;   /* reading SR3 clears ADDR (SR1 read above) */

        if (sr3 & 0x04)           /* TRA = 1: slave is a transmitter */
        {
            /* Combined write+read (repeated START): the command was received
               during the write phase and there is no STOPF before the read, so
               the response must be built here. Capture the write-phase length
               so the response builder can validate the command, then preload DR
               with the first byte. */
            i2c_rx_len = i2c_rx_idx;
            i2c_prepare_read_response();
            i2c_state = 2;
            i2c_tx_idx = 0;
            I2C->DR = i2c_tx_buf[i2c_tx_idx++];
        }
        else
        {
            uint8_t i;
            i2c_state = 1;        /* receiver */
            i2c_rx_idx = 0;
            i2c_rx_len = 0;
            /* Clear the buffer so a short transaction cannot replay stale
               bytes from the previous command. */
            for (i = 0; i < I2C_RX_BUF_SIZE; i++)
                i2c_rx_buf[i] = 0;
        }
        i2c_last_activity = system_ticks;
    }

    /* --- Stop detected (STOPF = SR1 bit4) --- */
    if (sr1 & 0x10)
    {
        I2C->CR2 = I2C->CR2;      /* reading SR1 above + writing CR2 clears STOPF */
        i2c_rx_len = i2c_rx_idx;  /* authoritative length for the main loop */
        i2c_cmd_ready = 1;
        i2c_state = 0;
        i2c_last_activity = system_ticks;
    }

    /* --- Data received (RXNE = SR1 bit6) --- */
    if (sr1 & 0x40)
    {
        uint8_t d = I2C->DR;
        if (i2c_rx_idx < I2C_RX_BUF_SIZE)
            i2c_rx_buf[i2c_rx_idx++] = d;
        i2c_last_activity = system_ticks;
    }

    /* --- Transmit empty (TXE = SR1 bit7) --- */
    if (sr1 & 0x80)
    {
        if (i2c_state == 2 && i2c_tx_idx < I2C_TX_BUF_SIZE)
            I2C->DR = i2c_tx_buf[i2c_tx_idx++];
        else
            I2C->DR = 0x00;       /* writing DR clears TXE (avoids a storm) */
    }

    /* --- Error flags (SR2: BERR/ARLO/AF/OVR) ---
       AF is set by hardware on the master's NACK of the last read byte - this
       is normal for every read transaction, not a fault. BERR/ARLO/AF/OVR are
       all cleared by writing 0 to those SR2 bits (the same sequence the SPL's
       I2C_ClearFlag uses). Do NOT clear them with a PE 0->1 toggle: disabling
       the peripheral while the bus is live desynchronises the slave, which made
       back-to-back reads fail until the idle-timeout reset recovered it. Only a
       genuine bus error (BERR/ARLO/OVR) re-arms the peripheral. */
    if (sr2 & 0x0F)
    {
        uint8_t fatal = sr2 & 0x0B;          /* BERR | ARLO | OVR */
        I2C->SR2 = (uint8_t)~(uint8_t)0x0F;  /* clear BERR/ARLO/AF/OVR */
        if (fatal)
        {
            I2C->CR1 &= (uint8_t)~0x01;      /* PE = 0 */
            I2C->CR1 |= 0x01;                /* PE = 1: re-arm after a bus error */
        }
        i2c_state = 0;
    }
}

/**
  * @brief  Prepare the transmit buffer for read-type commands.
  *         Called from the I2C ISR when the slave is addressed as a
  *         transmitter (i.e. on the repeated START of a combined write+read
  *         transaction). Only reads shared state - it never commands the
  *         motors, so it is safe to run in interrupt context.
  * @param  None
  * @retval None
  */
void i2c_prepare_read_response(void)
{
    uint8_t cmd = i2c_rx_buf[0];
    uint8_t motor = (uint8_t)(i2c_rx_buf[1] & 0x01); /* clamp to 0..1 */

    /* Reject a command that was not fully received: return a zeroed response
       rather than acting on, or echoing, stale/partial data. */
    if (i2c_rx_len < i2c_required_len(cmd))
    {
        i2c_tx_buf[0] = 0;
        i2c_tx_buf[1] = 0;
        i2c_tx_buf[2] = 0;
        i2c_tx_buf[3] = 0;
        i2c_tx_idx = 0;
        return;
    }

    switch (cmd)
    {
        case 0x04: /* Get position (int32, big-endian) */
            {
                int32_t pos;
                sim();          /* atomic read; iret restores CC, so no rim() */
                pos = steppers[motor].position;
                i2c_tx_buf[0] = (pos >> 24) & 0xFF;
                i2c_tx_buf[1] = (pos >> 16) & 0xFF;
                i2c_tx_buf[2] = (pos >> 8) & 0xFF;
                i2c_tx_buf[3] = pos & 0xFF;
                i2c_tx_idx = 0;
            }
            break;
        case 0x05: /* Get status */
            i2c_tx_buf[0] = steppers[motor].state;
            i2c_tx_idx = 0;
            break;
        case 0xFF: /* Firmware version */
            i2c_tx_buf[0] = 2; /* Major */
            i2c_tx_buf[1] = 0; /* Minor */
            i2c_tx_buf[2] = 0; /* Patch */
            i2c_tx_idx = 0;
            break;
        default:
            break;
    }
}

/**
  * @brief  Execute motor / control commands. Called from the main loop (after
  *         STOPF sets i2c_cmd_ready), so motor actions never run inside an ISR.
  *         Read-type commands are handled by i2c_prepare_read_response().
  * @param  None
  * @retval None
  */
void i2c_process_command(void)
{
    uint8_t cmd = i2c_rx_buf[0];
    uint8_t motor = i2c_rx_buf[1];
    /* steps/speed share the same 16-bit payload position (bytes 2..3); the
       frame is [cmd][motor][hi][lo], so reading bytes 4..5 always returned 0. */
    int16_t steps = (int16_t)(((uint16_t)i2c_rx_buf[2] << 8) | i2c_rx_buf[3]);
    uint16_t speed = ((uint16_t)i2c_rx_buf[2] << 8) | i2c_rx_buf[3];

    /* Ignore a command that was not fully received, so stale buffer bytes can
       never drive motor/speed/position. */
    if (i2c_rx_len < i2c_required_len(cmd))
        return;

    switch (cmd)
    {
        case 0x01: /* Move */
            stepper_move(motor, steps);
            break;
        case 0x02: /* Set speed */
            stepper_set_speed(motor, speed);
            break;
        case 0x03: /* Stop */
            stepper_stop(motor);
            break;
        case 0x06: /* Software reset */
            WWDG->CR = 0xBF;
            while(1);
            break;
        default:
            /* Read commands (0x04/0x05/0xFF) are handled in the ISR; unknown
               commands (including the removed 0x07 calibrate) are ignored. */
            break;
    }
}