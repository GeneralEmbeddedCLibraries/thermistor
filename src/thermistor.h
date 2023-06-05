// Copyright (c) 2022 Ziga Miklosic
// All Rights Reserved
// This software is under MIT licence (https://opensource.org/licenses/MIT)
////////////////////////////////////////////////////////////////////////////////
/**
*@file      thermistor.h
*@brief     Thermistor measurement and processing
*@author    Ziga Miklosic
*@date      08.12.2022
*@version   V1.0.0
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
 * 	Module version
 */
#define TH_VER_MAJOR		( 1 )
#define TH_VER_MINOR		( 0 )
#define TH_VER_DEVELOP		( 0 )

/**
 * 	Thermistor status
 */
typedef enum
{
    eTH_OK				= 0,		/**<Normal operation */
    eTH_ERROR			= 0x01,		/**<General error code */
	eTH_ERROR_OPEN		= 0x02,		/**<Open connection on sensor terminal*/
	eTH_ERROR_SHORT     = 0x04,		/**<Shorted sensor connections */
} th_status_t;

/**
 *  Thermistor error status type
 */
typedef enum
{
    eTH_ERR_FLOATING = 0,       /**<Floating error - clears after error condition is gone */
    eTH_ERR_PERMANENT,          /**<Permanent error - clears either on reset or API call of th_reset_error */
} th_err_type_t;

/**
 *	Sensor types
 */
typedef enum
{
	eTH_TYPE_NTC = 0,		/**<NTC thermistor */
	eTH_TYPE_PT1000,		/**<PT1000 */
	eTH_TYPE_PT100,			/**<PT100 */
	eTH_TYPE_PT500			/**<PT500 */
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
    adc_ch_opt_t    adc_ch;			/**<ADC channel */
    th_hw_conn_t    hw_conn;        /**<Hardware configuration of thermisto connection */
    th_hw_pull_t    hw_pull;        /**<Hardware configuration of pull resistor connection */
    float32_t       pull_up;        /**<Resistance of pull-up resistor */
    float32_t       pull_down;      /**<Resistance of pull-down resistor */
    float32_t		lpf_fc;			/**<Default LPF cutoff frequency */
    th_temp_type_t	type;			/**<Sensor type */
    
    /**<Sensor specific configurations */
    union
    {
        /**<NTC */
        struct
        {
            float32_t beta;         /**<NTC Beta factor */
            float32_t nom_val;      /**<Nominal value of NTC @25degC in Ohms */
        } ntc;

    } sensor;

    /**<Valid range */
    struct
    {
        float32_t min;              /**<Minimum allowed limit in degC */
        float32_t max;              /**<Maximum allowed limit in degC */
    } range;

    th_err_type_t   err_type;       /**<Error type */

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
th_status_t th_get_status       (const th_opt_t th);

#if ( 1 == TH_FILTER_EN )
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
