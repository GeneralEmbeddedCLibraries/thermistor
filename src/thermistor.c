// Copyright (c) 2022 Ziga Miklosic
// All Rights Reserved
// This software is under MIT licence (https://opensource.org/licenses/MIT)
////////////////////////////////////////////////////////////////////////////////
/**
*@file      thermistor.c
*@brief     Thermistor measurement and processing
*@author    Ziga Miklosic
*@date      23.11.2022
*@version   V0.1.0
*/
////////////////////////////////////////////////////////////////////////////////
/*!
* @addtogroup THERMISTOR
* @{ <!-- BEGIN GROUP -->
*/
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

#include "thermistor.h"


// Filer module
#if ( 1 == THERMISTOR_FILTER_EN )
    #include "middleware/filter/src/filter.h"
#endif


////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////

/**
 * 	Thermistor handler frequency
 *
 * 	Unit: Hz
 */
#define TH_HNDL_FREQ_HZ					( 1.0f / TH_HNDL_PERIOD_S )





/**
 *	Temperature values
 */
typedef struct
{
	float32_t degC;		/**<Temperature value in degree Celsius */
	float32_t degF;		/**<Temperature value in degree Fahrenheit */
	float32_t kelvin;	/**<Temperature in Kelvins */		
} th_temp_val_t;





typedef struct
{
	th_temp_val_t	temp;               /**<Temperature values */
	th_temp_val_t	temp_filt;          /**<Filtered temperature values */

    #if ( 1 == THERMISTOR_FILTER_EN )
        p_filter_rc_t	lpf;			/**<Low pass filter */
    #endif
} th_data_t;





////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////

/**
 * Initialization guards
 */
static bool gb_is_init = false;

/**
 * 	Pointer to configuration table
 */
static const th_cfg_t * gp_cfg_table = NULL;

/**
 *  Thermistor data
 */
static th_data_t g_th_data[eTH_NUM_OF] = {0};




////////////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Convert NTC voltage to degree C
*
* 			NOTE: 	This function is constrained to HW design. In this scenario
* 					NTC is simply connected to pull-up resistor.
*
* @param[in]    ntc_v 			- Voltage drop across NTC resistor
* @param[in]    beta 			- Beta factor of NTC
* @param[in]    ntc_nominal_val - Nominal value of NTC, typ. @25 degC
* @param[in]    pullup_val 		- Value of pull-up resistor
* @return       temp 			- Calculated temperature
*/
////////////////////////////////////////////////////////////////////////////////
static float32_t th_ntc_vol_convert_to_degC(const float32_t ntc_v, const float32_t beta, const float32_t ntc_nominal_val, const float32_t pullup_val)
{
	float32_t temp;
	float32_t ntc_r;

	// Catch division by 0
	// This also indicates that something is wrong with temperature sensor
	if 	(	( ADC_REF_V == ntc_v )
		||	( 0.0f == ntc_nominal_val ))
	{
		ntc_r = 0;

		// Set temperature to error value
		// TODO: 
		//temp = TEMPERATURE_ERROR_VALUE_DEG_CELSIUS;
		temp = -1.0f;
	}

	// Input valid
	else
	{
		// Calculate NTC resistance
		ntc_r = (float32_t) (( pullup_val * ntc_v ) / ( ADC_REF_V - ntc_v));

		// Calculate temperature
		temp = (float32_t) (( 1.0f / (( 1.0f / 298.15f ) + (( 1.0f / beta ) * log( ntc_r / ntc_nominal_val )))) - 273.15f );
	}

	return temp;
}



////////////////////////////////////////////////////////////////////////////////
/*!
 * @} <!-- END GROUP -->
 */
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/**
*@addtogroup THERMISTOR_API
* @{ <!-- BEGIN GROUP -->
*
* 	Following functions are part of API calls.
*/
////////////////////////////////////////////////////////////////////////////////


th_status_t th_init(void)
{
	th_status_t status	= eTH_OK;
	float32_t	th_volt = 0.0f;

	if ( false == gb_is_init )
	{		
    	// Get configuration table
		gp_cfg_table = thermistor_cfg_get_table();
        
        // Configuration table missing
		if ( NULL != gp_cfg_table )
		{
            // Init all channels
            for ( uint32_t ch = 0; ch < eTH_NUM_OF; ch++ )
            {
                // Get current ADC value
                th_volt = adc_get_real( gp_cfg_table[ch].adc_ch );

                // Convert to degC
                if ( eTH_TYPE_NTC == gp_cfg_table[ch].type )
                {
                    g_th_data[ch].temp.degC = th_ntc_vol_convert_to_degC( th_volt, gp_cfg_table[ch].sensor.ntc.beta, gp_cfg_table[ch].sensor.ntc.nom_val, gp_cfg_table[ch].pull_up );
                }
                else
                {
                    g_th_data[ch].temp.degC = -1.0f;
                }

                #if ( 1 == THERMISTOR_FILTER_EN )

                    // Init LPF 
                    if ( eFILTER_OK != filter_rc_init( &g_th_data[ch].lpf, gp_cfg_table[ch].lpf_fc, TH_HNDL_FREQ_HZ, 1, g_th_data[ch].temp.degC ))
                    {
                        status = eTH_ERROR;
                        break;
                    }
                    else
                    {
                        g_th_data[ch].temp_filt.degC = g_th_data[ch].temp.degC;
                    }
            
                #endif
            }
        }
        else
        {
            status = eTH_ERROR;
        }

		// Init success
		if ( eTH_OK == status )
		{
			gb_is_init = true;
		}
	}
	
	return status;
}


th_status_t	th_is_init(bool * const p_is_init)
{
	th_status_t status = eTH_OK;

	if ( NULL != p_is_init )
	{
		*p_is_init = gb_is_init;
	}
	else
	{
		status = eTH_ERROR;
	}
	
	return status;
}


th_status_t th_hndl(void)
{
	th_status_t status	= eTH_OK;
	float32_t	th_volt = 0.0f;

	TH_ASSERT( true == gb_is_init );

	if ( true == gb_is_init )
	{
		for ( uint32_t ch = 0; ch < eTH_NUM_OF; ch++ )
		{
			// Get current ADC value
			th_volt = adc_get_real( gp_cfg_table[ch].adc_ch );

			// NTC type
			if ( eTH_TYPE_NTC == gp_cfg_table[ch].type )
			{
				g_th_data[ch].temp.degC = th_ntc_vol_convert_to_degC( th_volt, gp_cfg_table[ch].sensor.ntc.beta, gp_cfg_table[ch].sensor.ntc.nom_val, gp_cfg_table[ch].pull_up );
			}
			else
			{
				g_th_data[ch].temp.degC = -1.0f;
			}

			// Update filter
            #if ( 1 == THERMISTOR_FILTER_EN )
                g_th_data[ch].temp_filt.degC = filter_rc_update( g_th_data[ch].lpf, g_th_data[ch].temp.degC );
            #endif
		}
	}
	else
	{
		status = eTH_ERROR;
	}
	
	return status;
}




th_status_t th_get_degC(const th_opt_t th, float32_t * const p_temp)
{
	th_status_t status = eTH_OK;

	TH_ASSERT( true == gb_is_init );
	TH_ASSERT( NULL != p_temp );

	if	(	( true == gb_is_init )
		&&	( NULL != p_temp ))
	{
		*p_temp = g_th_data[th].temp.degC;
	}
	else
	{
		status = eTH_ERROR;
	}
	
	return status;
}


th_status_t th_get_degF(const th_opt_t th, float32_t * const p_temp)
{
	th_status_t status = eTH_OK;

	TH_ASSERT( true == gb_is_init );
	TH_ASSERT( NULL != p_temp );

	if	(	( true == gb_is_init )
		&&	( NULL != p_temp ))
	{
		*p_temp = g_th_data[th].temp.degF;
	}
	else
	{
		status = eTH_ERROR;
	}
	
	return status;
}


th_status_t th_get_kelvin(const th_opt_t th, float32_t * const p_temp)
{
	th_status_t status = eTH_OK;

	TH_ASSERT( true == gb_is_init );
	TH_ASSERT( NULL != p_temp );

	if	(	( true == gb_is_init )
		&&	( NULL != p_temp ))
	{
		*p_temp = g_th_data[th].temp.kelvin;
	}
	else
	{
		status = eTH_ERROR;
	}
	
	return status;
}

th_status_t th_get_resistance(const th_opt_t th, float32_t * const p_res)
{
	th_status_t status = eTH_OK;

	TH_ASSERT( true == gb_is_init );
	TH_ASSERT( NULL != p_res );

	if	(	( true == gb_is_init )
		&&	( NULL != p_res ))
	{
		// TODO: ...
	}
	else
	{
		status = eTH_ERROR;
	}
	
	return status;
}


#if ( 1 == THERMISTOR_FILTER_EN )


    th_status_t th_get_degC_filt(const th_opt_t th, float32_t * const p_temp)
    {
    	th_status_t status = eTH_OK;

    	TH_ASSERT( true == gb_is_init );
    	TH_ASSERT( NULL != p_temp );

    	if	(	( true == gb_is_init )
    		&&	( NULL != p_temp ))
    	{
    		*p_temp = g_th_data[th].temp_filt.degC;
    	}
    	else
    	{
    		status = eTH_ERROR;
    	}
	
    	return status;
    }


    th_status_t th_get_degF_filt(const th_opt_t th, float32_t * const p_temp)
    {
    	th_status_t status = eTH_OK;

    	TH_ASSERT( true == gb_is_init );
    	TH_ASSERT( NULL != p_temp );

    	if	(	( true == gb_is_init )
    		&&	( NULL != p_temp ))
    	{
    		*p_temp = g_th_data[th].temp_filt.degF;
    	}
    	else
    	{
    		status = eTH_ERROR;
    	}
	
    	return status;
    }


    th_status_t th_get_kelvin_filt(const th_opt_t th, float32_t * const p_temp)
    {
    	th_status_t status = eTH_OK;

    	TH_ASSERT( true == gb_is_init );
    	TH_ASSERT( NULL != p_temp );

    	if	(	( true == gb_is_init )
    		&&	( NULL != p_temp ))
    	{
    		*p_temp = g_th_data[th].temp_filt.kelvin;
    	}
    	else
    	{
    		status = eTH_ERROR;
    	}
	
    	return status;
    }


    th_status_t th_set_lpf_fc(const th_opt_t th, const float32_t fc)
    {
    	th_status_t status = eTH_OK;

    	TH_ASSERT( true == gb_is_init );

    	if ( true == gb_is_init )
    	{

    	}
    	else
    	{
    		status = eTH_ERROR;
    	}
	
    	return status;
    }


    th_status_t th_get_lpf_fc(const th_opt_t th, float32_t * const p_fc)
    {
    	th_status_t status = eTH_OK;

    	TH_ASSERT( true == gb_is_init );

    	if ( true == gb_is_init )
    	{

    	}
    	else
    	{
    		status = eTH_ERROR;
    	}
	
    	return status;
    }

#endif


////////////////////////////////////////////////////////////////////////////////
/**
* @} <!-- END GROUP -->
*/
////////////////////////////////////////////////////////////////////////////////
