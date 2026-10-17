/**
  ******************************************************************************
  * @file    stm8s_conf.h
  * @author  MCD Application Team
  * @version V2.3.0
  * @date    16-January-2014
  * @brief   This file is used to configure the STM8S Devices.
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
#ifndef __STM8S_CONF_H
#define __STM8S_CONF_H

/* Includes ------------------------------------------------------------------*/
#include "stm8s.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Uncomment the line below to enable peripheral header file inclusion */
#include "stm8s_clk.h"
#include "stm8s_gpio.h"
#include "stm8s_i2c.h"
#include "stm8s_tim4.h"
#include "stm8s_iwdg.h"
#include "stm8s_flash.h"
#include "stm8s_itc.h"

/* Uncomment the line below to enable ITC header file inclusion */
/* #include "stm8s_itc.h" */

/* Uncomment the line below to enable EXTI header file inclusion */
/* #include "stm8s_exti.h" */

/* Uncomment the line below to enable ADC header file inclusion */
/* #include "stm8s_adc.h" */

/* Uncomment the line below to enable UART header file inclusion */
/* #include "stm8s_uart.h" */

/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

#endif /* __STM8S_CONF_H */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/