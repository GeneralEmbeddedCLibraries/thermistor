// Copyright (c) 2025 Ziga Miklosic
// All Rights Reserved
// This software is under MIT licence (https://opensource.org/licenses/MIT)
////////////////////////////////////////////////////////////////////////////////
/**
*@file      thermistor.h
*@brief     Thermistor measurement and processing
*@author    Ziga Miklosic
*@email     ziga.miklosic@gmail.com
*@date      01.02.2025
*@version   V1.2.0
*/
////////////////////////////////////////////////////////////////////////////////
/**
*@addtogroup THERMISTOR
* @{ <!-- BEGIN GROUP -->
*/
////////////////////////////////////////////////////////////////////////////////

#ifndef __THERMISTOR_H
#define __THERMISTOR_H

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../../thermistor_cfg.h"

// ADC low level driver
#include "drivers/peripheral/adc/adc/src/adc.h"

////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////

/**
 *     Module version
 */
#define TH_VER_MAJOR        ( 1 )
#define TH_VER_MINOR        ( 2 )
#define TH_VER_DEVELOP      ( 0 )

/**
 *     Thermistor status
 */
typedef enum
{
    eTH_OK          = 0x00U,	/**<Normal operation */
    eTH_ERROR       = 0x01U,	/**<General error code */
    eTH_ERROR_OPEN  = 0x02U,	/**<Open connection on sensor terminal*/
    eTH_ERROR_SHORT = 0x04U,	/**<Shorted sensor connections */
} th_status_t;

////////////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////////////
th_status_t th_init             (void);
th_status_t th_deinit           (void);
th_status_t th_is_init          (bool * const p_is_init);
th_status_t th_hndl             (void);

th_status_t th_get_raw          (const th_ch_t th, uint16_t * const p_raw);
th_status_t th_get_degC         (const th_ch_t th, float32_t * const p_temp);
th_status_t th_get_degF         (const th_ch_t th, float32_t * const p_temp);
th_status_t th_get_kelvin       (const th_ch_t th, float32_t * const p_temp);
th_status_t th_get_resistance   (const th_ch_t th, float32_t * const p_res);
th_status_t th_get_status       (const th_ch_t th);

#if ( 1 == TH_FILTER_EN )
    th_status_t th_get_degC_filt    (const th_ch_t th, float32_t * const p_temp);
    th_status_t th_get_degF_filt    (const th_ch_t th, float32_t * const p_temp);
    th_status_t th_get_kelvin_filt  (const th_ch_t th, float32_t * const p_temp);
    th_status_t th_set_lpf_fc       (const th_ch_t th, const float32_t fc);
    th_status_t th_get_lpf_fc       (const th_ch_t th, float32_t * const p_fc);
#endif

#endif // __THERMISTOR_H

////////////////////////////////////////////////////////////////////////////////
/**
* @} <!-- END GROUP -->
*/
////////////////////////////////////////////////////////////////////////////////
