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
 *	32-bit floating point definition
 */
typedef float float32_t;

////////////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////////////
th_status_t th_init				(void);
th_status_t	th_is_init			(bool * const p_is_init);
th_status_t th_hndl				(void);
th_status_t th_bg_hndl			(void);
th_status_t th_get_degC			(const th_opt_t th, float32_t * const p_temp);
th_status_t th_get_degF			(const th_opt_t th, float32_t * const p_temp);
th_status_t th_get_kelvin		(const th_opt_t th, float32_t * const p_temp);
th_status_t th_get_degC_filt	(const th_opt_t th, float32_t * const p_temp);
th_status_t th_get_degF_filt	(const th_opt_t th, float32_t * const p_temp);
th_status_t th_get_kelvin_filt	(const th_opt_t th, float32_t * const p_temp);
th_status_t th_set_lpf_fc		(const th_opt_t th, const float32_t fc);
th_status_t th_get_lpf_fc		(const th_opt_t th, float32_t * const p_fc);

#endif // __THERMISTOR_H

////////////////////////////////////////////////////////////////////////////////
/**
* @} <!-- END GROUP -->
*/
////////////////////////////////////////////////////////////////////////////////
