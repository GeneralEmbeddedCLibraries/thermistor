// Copyright (c) 2022 Ziga Miklosic
// All Rights Reserved
// This software is under MIT licence (https://opensource.org/licenses/MIT)
////////////////////////////////////////////////////////////////////////////////
/**
*@file      thermistor.h
*@brief     Thermistor measurement and processing
*@author    Ziga Miklosic
*@date      23.11.2022
*@version   V0.1.0
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
#include "drivers/peripheral/adc/adc.h"

////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////

/**
 * 	Thermistor status
 */
typedef enum
{
    eTH_OK				= 0,		/**<Normal operation */
    eTH_ERROR			= 0x01,		/**<General error code */
	eTH_ERROR_OPEN		= 0x02,		/**<Open connection */
	eTH_ERROR_SHORTED	= 0x04,		/**<Shorted */
} th_status_t;

/**
 *	Sensor types
 */
typedef enum
{
	eTH_TYPE_NTC = 0,		/**<NTC thermistor */
	eTH_TYPE_PT1000			/**<PT1000 */
} th_temp_type_t;

/**
 *  Sensor HW connection
 *
 *  @brief  Thermistor is either connected on high or low
 *          side. High side means that thermistor is connected 
 *          between positive rail and pull-down resistor. 
 *          Low side means that thermistor is connected between
 *          GND and pull-up resistor
 */
typedef enum
{
    eTH_HW_LOW_SIDE = 0,    /**<Thermistor layouted on low side */
    eTH_HW_HIGH_SIDE,       /**<Thermistor layouted on high side */
} th_hw_conn_t;

/**
 *  Pull resistor connections
 */
typedef enum
{
    eTH_HW_PULL_DOWN   = 0,    /**<Thermistor HW connected with pull-down resistor */
    eTH_HW_PULL_UP,            /**<Thermistor HW connected with pull-up resistor */ 
    eTH_HW_PULL_BOTH,          /**<Thermistor HW connected with both pull-up and pull-down resistor */
} th_hw_pull_t;

/**
 *	Thermistor configuration
 */
typedef struct
{	
    adc_pins_t		adc_ch;			/**<ADC channel */	
    th_hw_conn_t    hw_conn;        /**<Hardware configuration of thermisto connection */
    th_hw_pull_t    hw_pull;        /**<Hardware configuration of pull resistor connection */
    float32_t       pull_up;        /**<Resistance of pull-up resistor */
    float32_t       pull_down;      /**<Resistance of pull-down resistor */
    float32_t		lpf_fc;			/**<LPF cutoff frequency */
    th_temp_type_t	type;			/**<Sensor type */
    
    // For NTC calc
	float32_t		beta;			/**<NTC Beta factor */
	float32_t		nom_val;		/**<Nominal value of NTC in Ohms */
	

} th_cfg_t;

/**
 *	32-bit floating point definition
 */
typedef float float32_t;

////////////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////////////
th_status_t th_init				(void);
th_status_t	th_is_init			(bool * const p_is_init);
th_status_t th_hndl				(void);
th_status_t th_get_degC			(const th_opt_t th, float32_t * const p_temp);
th_status_t th_get_degF			(const th_opt_t th, float32_t * const p_temp);
th_status_t th_get_kelvin		(const th_opt_t th, float32_t * const p_temp);
th_status_t th_get_resistance   (const th_opt_t th, float32_t * const p_res);

#if ( 1 == THERMISTOR_FILTER_EN )
    th_status_t th_get_degC_filt	(const th_opt_t th, float32_t * const p_temp);
    th_status_t th_get_degF_filt	(const th_opt_t th, float32_t * const p_temp);
    th_status_t th_get_kelvin_filt	(const th_opt_t th, float32_t * const p_temp);
    th_status_t th_set_lpf_fc		(const th_opt_t th, const float32_t fc);
    th_status_t th_get_lpf_fc		(const th_opt_t th, float32_t * const p_fc);
#endif

#endif // __THERMISTOR_H

////////////////////////////////////////////////////////////////////////////////
/**
* @} <!-- END GROUP -->
*/
////////////////////////////////////////////////////////////////////////////////