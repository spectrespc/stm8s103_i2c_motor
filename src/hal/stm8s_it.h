/**
  ******************************************************************************
  * @file    stm8s_it.h
  * @author  MCD Application Team
  * @version V2.3.0
  * @date    16-January-2014
  * @brief   This file contains all interrupt handlers.
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

/* Define to prevent recursive inclusion ---------------------------------*/
#ifndef __STM8S_IT_H
#define __STM8S_IT_H

/* Includes ------------------------------------------------------------------*/
#include "stm8s.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants ----------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
#ifdef _COSMIC_
 void NonHandledInterrupt(void);
#endif
#ifdef _SDCC_
 void NonHandledInterrupt(void);
#endif

/* IT handlers for STM8S103xx devices --------------------------------------------*/
/* IMPORTANT (SDCC): SDCC builds the interrupt vector table from the
   __interrupt(N) declarations that are visible in the module containing main().
   Therefore main.c MUST include this header, and only the handlers actually
   implemented in stm8s_it.c are declared here, each with its correct STM8S103
   vector number (RM0016, STM8S103 interrupt vector table):
     11 = TIM1 update/overflow/trigger/break
     19 = I2C
     23 = TIM4 update/overflow                                              */

/* I2C interrupt handler (vector 19) */
INTERRUPT_HANDLER(I2C_IRQHandler, 19);

/* TIM1 update/overflow/trigger/break (vector 11) */
INTERRUPT_HANDLER(TIM1_UPD_OVF_TRG_BRK_IRQHandler, 11);

/* TIM4 update/overflow (vector 23) */
INTERRUPT_HANDLER(TIM4_UPD_OVF_IRQHandler, 23);

#endif /* __STM8S_IT_H */