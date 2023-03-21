// Copyright (c) 2022 Ziga Miklosic
// All Rights Reserved
// This software is under MIT licence (https://opensource.org/licenses/MIT)
////////////////////////////////////////////////////////////////////////////////
/**
*@file      thermistor.c
*@brief     Thermistor measurement and processing
*@author    Ziga Miklosic
*@date      08.12.2022
*@version   V1.0.0
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
#if ( 1 == TH_FILTER_EN )
    #include "middleware/filter/src/filter.h"
#endif

////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////

/**
 *      Thermistor handler frequency
 *
 * 	Unit: Hz
 */
#define TH_HNDL_FREQ_HZ					( 1.0f / TH_HNDL_PERIOD_S )

/**
 *  Factor for NTC calculation when given nominal NTC value at 25 degC
 */
#define TH_NTC_25DEG_FACTOR             ((float32_t) ( 1.0 / 298.15 ))      // Leave double

/**
 *	PT100/500/1000 temperature calculation factors according
 *	to DIN EN60751 standard
 */
#define TH_PT_DIN_EN60751_A		( 3.9083e-3 )	// degC^-1
#define TH_PT_DIN_EN60751_B		( -5.775e-7 )	// degC^-2

/**
 *		Precalculated factors for PT100/500/1000 calculations
 */
#define TH_PT_DIN_EN60751_AA	(( float32_t )( TH_PT_DIN_EN60751_A * TH_PT_DIN_EN60751_A ))
#define TH_PT_DIN_EN60751_2B	(( float32_t )( 2.0 * TH_PT_DIN_EN60751_B ))
#define TH_PT_DIN_EN60751_4B	(( float32_t )( 4.0 * TH_PT_DIN_EN60751_B ))

/**
 *		PT100/500/1000 Resistance Limits
 *
 * @note Taken from "doc/pt1000_pt100_pt500_tables.xlsx" table!
 *
 *	Unit: Ohm
 */
#define TH_PT1000_MAX_OHM		( 3904.81f )
#define TH_PT1000_MIN_OHM		( 185.20f )
#define TH_PT100_MAX_OHM		( 390.48f )
#define TH_PT100_MIN_OHM		( 18.52f )
#define TH_PT500_MAX_OHM		( 1937.74f )
#define TH_PT500_MIN_OHM		( 114.13f )

/**
 *  Thermistor data
 */
typedef struct
{
    float32_t       res;                /**<Thermistor resistance */
	float32_t   	temp;               /**<Temperature values in degC */
	float32_t   	temp_filt;          /**<Filtered temperature values in degC */

    #if ( 1 == TH_FILTER_EN )
        p_filter_rc_t	lpf;			/**<Low pass filter */
    #endif

    th_status_t      status;             /**<Thermistor status */
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
// Function Prototypes
////////////////////////////////////////////////////////////////////////////////
static float32_t    th_get_vcc                  (void);
static float32_t    th_calc_res_single_pull     (const th_opt_t th);
static float32_t    th_calc_res_both_pull       (const th_opt_t th);
static float32_t    th_calc_resistance          (const th_opt_t th);
static float32_t    th_calc_ntc_temperature     (const float32_t rth, const float32_t beta, const float32_t rth_nom);
static float32_t    th_calc_pt100_temperature   (const float32_t rth);
static float32_t    th_calc_pt500_temperature   (const float32_t rth);
static float32_t    th_calc_pt1000_temperature  (const float32_t rth);
static th_status_t  th_init_filter              (const th_opt_t th);
static th_status_t  th_status_hndl              (const th_opt_t th, const float32_t temp);

static inline float32_t th_limit_f32			(const float32_t in, const float32_t min, const float32_t max);

////////////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Get thermistor power supply (VCC) potencial
*
* @note     Based on "THERMISTOR_SUPPLY_RIPPLE_COMP_EN" macro definition function 
*           returns:
*               
*               ENABLE  - Measured power supply voltage in V
*               DISABLE - Constant value of macro "THERMISTOR_SUPPLY_V"
*
* @return       res     - Resistance of thermistor
*/
////////////////////////////////////////////////////////////////////////////////
static float32_t th_get_vcc(void)
{
    float32_t vcc = 0.0f;

    #if ( 1 == TH_SUPPLY_RIPPLE_COMP_EN )
        vcc = adc_get_real( TH_SUPPLY_ADC_CH );
    #else
        vcc = (float32_t) ( TH_SUPPLY_V );
    #endif

    return vcc;
}

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Calculate resistance of thermistor with single pull resistor
*
* @note     In case of unplasible voltage -1 is returned!
*
* @param[in]    th      - Thermistor option
* @return       res     - Resistance of thermistor
*/
////////////////////////////////////////////////////////////////////////////////
static float32_t th_calc_res_single_pull(const th_opt_t th)
{
    float32_t th_res = 0.0f;
    
    // Get thermistor voltage
    const float32_t vth = adc_get_real( gp_cfg_table[th].adc_ch );

    // Get VCC voltage
    const float32_t vcc = th_get_vcc();
    
    // Check for valid voltage ranges
    if (( vth < vcc ) && ( vth >= 0.0f ) && ( vcc > 0.0f ))
    {
        // Thermistor on low side
        if ( eTH_HW_LOW_SIDE == gp_cfg_table[th].hw_conn )
        {
            // Thermistor on low side with pull-up
            th_res = (float32_t) (( gp_cfg_table[th].pull_up * vth ) / ( vcc - vth ));
        }

        // Thermistor on high side
        else
        {   
            // Thermistor on high side with pull-down
            th_res = (float32_t) (( gp_cfg_table[th].pull_down * vth ) / ( vcc - vth ));
        } 
    }
    
    // Unplausible voltage
    else
    {
        th_res = -1.0f;
    } 
    
    return th_res;     
}

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Calculate resistance of thermistor with both pull resistors
*
* @note     In case of unplasible voltage -1 is returned!
*
* @param[in]    th      - Thermistor option
* @return       res     - Resistance of thermistor
*/
////////////////////////////////////////////////////////////////////////////////
static float32_t th_calc_res_both_pull(const th_opt_t th)
{
    float32_t th_res = 0.0f;
    
    // Get thermistor voltage
    const float32_t vth = adc_get_real( gp_cfg_table[th].adc_ch );

    // Get VCC voltage
    const float32_t vcc = th_get_vcc();
    
    // Check for valid voltage ranges
    if (( vth < vcc ) && ( vth >= 0.0f ))
    {
        // Thermistor on low side
        if ( eTH_HW_LOW_SIDE == gp_cfg_table[th].hw_conn )
        {
            // Thermistor on low side with both resistors
            th_res = (float32_t) ((( vcc - vth ) / ( gp_cfg_table[th].pull_up * vth )) - ( 1.0f / gp_cfg_table[th].pull_down ));
            
            // Check for division by zero
            if ( th_res > 0.0f )
            {
                th_res = (float32_t)( 1.0f / th_res );
            }

            //  TODO: Check what to do if that happens. Might happen in real circuit under specific circuitstainces...
            else
            {
                TH_DBG_PRINT( "TH: Unhandler event..." );
                TH_ASSERT( 0 );
            }
        }

        // Thermistor on high side
        else
        {   
            // Thermistor on low side with both resistors
            th_res = (float32_t) ((( vcc - vth ) / ( gp_cfg_table[th].pull_down * vth )) - ( 1.0f / gp_cfg_table[th].pull_up ));
            
            // Check for division by zero
            if ( th_res > 0.0f )
            {
                th_res = (float32_t)( 1.0f / th_res );
            }

            //  TODO: Check what to do if that happens. Might happen in real circuit under specific circuitstainces...
            else
            {
                TH_DBG_PRINT( "TH: Unhandler event..." );
                TH_ASSERT( 0 );
            } 
        } 
    }
    
    // Unplausible voltage
    else
    {
        th_res = -1.0f;
    } 
    
    return th_res;     
}

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Calculate resistance of thermistor
*
* @note     In case of unplasible voltage -1 is returned!
*
* @param[in]    th      - Thermistor option
* @return       res     - Resistance of thermistor
*/
////////////////////////////////////////////////////////////////////////////////
static float32_t th_calc_resistance(const th_opt_t th)
{
    float32_t th_res = 0.0f;

    // Single pull resistor
    if  (   ( eTH_HW_PULL_UP    == gp_cfg_table[th].hw_pull )
        ||  ( eTH_HW_PULL_DOWN  == gp_cfg_table[th].hw_pull ))
    {
        th_res = th_calc_res_single_pull( th );
    }

    // Both pull resistors
    else
    {
        th_res = th_calc_res_both_pull( th );
    }

    return th_res;
}

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Convert NTC resistance to degree C
*
* @param[in]    rth 			- Resistance of NTC thermistor
* @param[in]    beta 			- Beta factor of NTC
* @param[in]    rth_nom         - Nominal value of NTC @25 degC
* @return       temp 			- Calculated temperature
*/
////////////////////////////////////////////////////////////////////////////////
static float32_t th_calc_ntc_temperature(const float32_t rth, const float32_t beta, const float32_t rth_nom)
{
    float32_t temp = 0.0f;

    TH_ASSERT( rth_nom > 0.0f );

    // Calculate temperature
    temp = (float32_t) (( 1.0f / ( TH_NTC_25DEG_FACTOR + (( 1.0f / beta ) * log( rth / rth_nom )))) - 273.15f );

    return temp;
}

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Convert PT100 resistance to degree C
*
* @note     Calculation of PT500 according to DIN EN60751 standard.
*           For futher details look at table: doc/pt1000_pt100_pt500_tables.xlsx 
*
* @param[in]    rth 			- Resistance of PT100 thermistor
* @return       temp 			- Calculated temperature
*/
////////////////////////////////////////////////////////////////////////////////
static float32_t th_calc_pt100_temperature(const float32_t rth)
{
	float32_t temp  = 0.0f;

	// Limit termistor resistance
	const float32_t rth_lim = th_limit_f32( rth, TH_PT100_MIN_OHM, TH_PT100_MAX_OHM );

	// Calculate temperature
	temp = (float32_t) (( -TH_PT_DIN_EN60751_A + sqrtf( TH_PT_DIN_EN60751_AA - TH_PT_DIN_EN60751_4B * ( 1 - rth_lim / 100.0f ))) / TH_PT_DIN_EN60751_2B );
    
    return temp;
}

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Convert PT500 resistance to degree C
*
* @note     Calculation of PT500 according to DIN EN60751 standard.
*           For futher details look at table: doc/pt1000_pt100_pt500_tables.xlsx 
*
* @param[in]    rth 			- Resistance of PT500 thermistor
* @return       temp 			- Calculated temperature
*/
////////////////////////////////////////////////////////////////////////////////
static float32_t th_calc_pt500_temperature(const float32_t rth)
{
	float32_t temp  = 0.0f;

	// Limit termistor resistance
	const float32_t rth_lim = th_limit_f32( rth, TH_PT500_MIN_OHM, TH_PT500_MAX_OHM );

	// Calculate temperature
	temp = (float32_t) (( -TH_PT_DIN_EN60751_A + sqrtf( TH_PT_DIN_EN60751_AA - TH_PT_DIN_EN60751_4B * ( 1 - rth_lim / 500.0f ))) / TH_PT_DIN_EN60751_2B );
    
    return temp;
}

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Convert PT1000 resistance to degree C
*
* @note     Calculation of PT1000 according to DIN EN60751 standard.
*           For futher details look at table: doc/pt1000_pt100_pt500_tables.xlsx 
*
* @param[in]    rth 			- Resistance of PT1000 thermistor
* @return       temp 			- Calculated temperature
*/
////////////////////////////////////////////////////////////////////////////////
static float32_t th_calc_pt1000_temperature(const float32_t rth)
{
	float32_t temp  = 0.0f;

	// Limit termistor resistance
	const float32_t rth_lim = th_limit_f32( rth, TH_PT1000_MIN_OHM, TH_PT1000_MAX_OHM );

	// Calculate temperature
	temp = (float32_t) (( -TH_PT_DIN_EN60751_A + sqrtf( TH_PT_DIN_EN60751_AA - TH_PT_DIN_EN60751_4B * ( 1 - rth_lim / 1000.0f ))) / TH_PT_DIN_EN60751_2B );
    
    return temp;
}

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Calculate temperature
*
* @param[in]    th      - Thermistor option
* @return       temp    - Calculated temperature
*/
////////////////////////////////////////////////////////////////////////////////
static float32_t th_calc_temperature(const th_opt_t th)
{
    float32_t temp = 0.0f;

    // Calculate thermistor resistance
    g_th_data[th].res = th_calc_resistance( th );

    if ( g_th_data[th].res > 0.0f )
    {
        // Sensor type
        switch( gp_cfg_table[th].type )
        {
            case eTH_TYPE_NTC:
                temp = th_calc_ntc_temperature( g_th_data[th].res, gp_cfg_table[th].sensor.ntc.beta, gp_cfg_table[th].sensor.ntc.nom_val );
                break;

            case eTH_TYPE_PT1000:
                temp = th_calc_pt1000_temperature( g_th_data[th].res );
                break;

            case eTH_TYPE_PT100:
                temp = th_calc_pt100_temperature( g_th_data[th].res );
                break;

            case eTH_TYPE_PT500:
                temp = th_calc_pt500_temperature( g_th_data[th].res );
                break;

            default:
                TH_ASSERT( 0 );
                break;
        }
    }

    return temp;
}

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Init filters
*
* @param[in]    th      - Thermistor option
* @return       status  - Status of operation
*/
////////////////////////////////////////////////////////////////////////////////
static th_status_t th_init_filter(const th_opt_t th)
{
    th_status_t status = eTH_OK;

    #if ( 1 == TH_FILTER_EN )

        // Init LPF 
        if ( eFILTER_OK != filter_rc_init( &g_th_data[th].lpf, gp_cfg_table[th].lpf_fc, TH_HNDL_FREQ_HZ, 1, g_th_data[th].temp ))
        {
            status = eTH_ERROR;
        }

    #endif

    return status;
}

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Handle thermistor status
*
* @param[in]    th      - Thermistor option
* @param[in]    temp    - Thermistor temperature
* @return       status  - Status of operation
*/
////////////////////////////////////////////////////////////////////////////////
static th_status_t th_status_hndl(const th_opt_t th, const float32_t temp)
{
    th_status_t status = eTH_OK;

    // Check for status if:
    //      1. Error type is floating
    //  OR      2a. Error type is permanent
    //      AND 2b. Status is OK 
    if  (    ( eTH_ERR_FLOATING == gp_cfg_table[th].err_type )
        ||  (( eTH_ERR_PERMANENT == gp_cfg_table[th].err_type ) && ( eTH_OK == g_th_data[th].status )))
    {
        // Above MAX range
        if ( temp > gp_cfg_table[th].range.max )
        {
            // Sensor type
            switch( gp_cfg_table[th].type )
            {
                case eTH_TYPE_NTC:
                    status = eTH_ERROR_SHORT;
                    break;

                case eTH_TYPE_PT1000:
                case eTH_TYPE_PT100:
                case eTH_TYPE_PT500:
                    status = eTH_ERROR_OPEN;
                    break;

                default:
                    TH_ASSERT( 0 );
                    break;
            } 
        }

        // Bellow MIN range
        else if (temp < gp_cfg_table[th].range.min )
        {
            // Sensor type
            switch( gp_cfg_table[th].type )
            {
                case eTH_TYPE_NTC:
                    status = eTH_ERROR_OPEN;
                    break;

                case eTH_TYPE_PT1000:
                case eTH_TYPE_PT100:
                case eTH_TYPE_PT500:
                    status = eTH_ERROR_SHORT;
                    break;

                default:
                    TH_ASSERT( 0 );
                    break;
            }     
        }
    
        // In NORMAL range
        else
        {
            status = eTH_OK;
        }
    }

    return status;
}

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Limit floating point value
*
* @param[in]    in	- Input value to limit
* @param[in]    min	- Minimum value limit
* @param[in]    max	- Maximum value 
* @return       out	- Limited output value
*/
////////////////////////////////////////////////////////////////////////////////
static inline float32_t th_limit_f32(const float32_t in, const float32_t min, const float32_t max)
{
	float32_t out = in;
	
	if ( in > max )
	{
		out = max;
	}
	else if ( in < min )
	{
		out = min;
	}
	else
	{
		out = in;
	}

	return out;
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

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Init thermistors
*
* @return       status  - Status of operation
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
            // Init all thermistors
            for ( uint32_t th = 0; th < eTH_NUM_OF; th++ )
            {
                // Get current temperature
                g_th_data[th].temp = th_calc_temperature( th );
                g_th_data[th].temp_filt = g_th_data[th].temp;
                
                // Init filter
                if ( eTH_OK != th_init_filter( th ))
                {
                    status = eTH_ERROR;
                    break;
                }
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

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Get initialization flag
*
* @param[out]   p_is_init   - Initialization flag
* @return       status      - Status of operation
*/
////////////////////////////////////////////////////////////////////////////////
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

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Thermistor main handler
*
* @return       status  - Status of operation
*/
////////////////////////////////////////////////////////////////////////////////
th_status_t th_hndl(void)
{
	th_status_t status	= eTH_OK;
	float32_t	th_volt = 0.0f;

	TH_ASSERT( true == gb_is_init );

	if ( true == gb_is_init )
	{
        // Handle all thermistors
		for ( uint32_t th = 0; th < eTH_NUM_OF; th++ )
		{
			// Get temperature
            g_th_data[th].temp = th_calc_temperature( th );            

			// Update filter
            #if ( 1 == TH_FILTER_EN )
                g_th_data[th].temp_filt = filter_rc_update( g_th_data[th].lpf, g_th_data[th].temp );
            #else
                g_th_data[th].temp_filt = g_th_data[th].temp;
            #endif

            // Check status on filtered temperature
            g_th_data[th].status = th_status_hndl( th, g_th_data[th].temp_filt );
		}
	}
	else
	{
		status = eTH_ERROR;
	}
	
	return status;
}

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Get temperature in deg C
*
* @param[in]    th      - Thermistor option
* @param[out]   p_temp  - Pointer to temperature
* @return       status  - Status of operation
*/
////////////////////////////////////////////////////////////////////////////////
th_status_t th_get_degC(const th_opt_t th, float32_t * const p_temp)
{
	th_status_t status = eTH_OK;

	TH_ASSERT( true == gb_is_init );
	TH_ASSERT( NULL != p_temp );
    TH_ASSERT( th < eTH_NUM_OF );

    if	(	( true == gb_is_init )
        &&	( NULL != p_temp )
        &&  ( th < eTH_NUM_OF ))
	{
		*p_temp = g_th_data[th].temp;
	}
	else
	{
		status = eTH_ERROR;
	}
	
	return status;
}

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Get temperature in deg F
*
* @param[in]    th      - Thermistor option
* @param[out]   p_temp  - Pointer to temperature
* @return       status  - Status of operation
*/
////////////////////////////////////////////////////////////////////////////////
th_status_t th_get_degF(const th_opt_t th, float32_t * const p_temp)
{
	th_status_t status = eTH_OK;

	TH_ASSERT( true == gb_is_init );
	TH_ASSERT( NULL != p_temp );
    TH_ASSERT( th < eTH_NUM_OF );

    if	(	( true == gb_is_init )
        &&	( NULL != p_temp )
        &&  ( th < eTH_NUM_OF ))
	{
        // TODO: Convert ...
		*p_temp = g_th_data[th].temp;
	}
	else
	{
		status = eTH_ERROR;
	}
	
	return status;
}

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Get temperature in kelvins
*
* @param[in]    th      - Thermistor option
* @param[out]   p_temp  - Pointer to temperature
* @return       status  - Status of operation
*/
////////////////////////////////////////////////////////////////////////////////
th_status_t th_get_kelvin(const th_opt_t th, float32_t * const p_temp)
{
	th_status_t status = eTH_OK;

	TH_ASSERT( true == gb_is_init );
	TH_ASSERT( NULL != p_temp );
    TH_ASSERT( th < eTH_NUM_OF );

    if	(	( true == gb_is_init )
        &&	( NULL != p_temp )
        &&  ( th < eTH_NUM_OF ))
	{
        // TODO: Convert...
		*p_temp = g_th_data[th].temp;
	}
	else
	{
		status = eTH_ERROR;
	}
	
	return status;
}

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Get resistance of thermistor in Ohms
*
* @param[in]    th      - Thermistor option
* @param[out]   p_res   - Pointer to resistance
* @return       status  - Status of operation
*/
////////////////////////////////////////////////////////////////////////////////
th_status_t th_get_resistance(const th_opt_t th, float32_t * const p_res)
{
	th_status_t status = eTH_OK;

	TH_ASSERT( true == gb_is_init );
	TH_ASSERT( NULL != p_res );
    TH_ASSERT( th < eTH_NUM_OF );

    if	(	( true == gb_is_init )
        &&	( NULL != p_res )
        &&  ( th < eTH_NUM_OF ))
	{
        *p_res = g_th_data[th].res;
	}
	else
	{
		status = eTH_ERROR;
	}
	
	return status;
}

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Get thermistor status
*
* @param[in]    th      - Thermistor option
* @return       status  - Status of operation
*/
////////////////////////////////////////////////////////////////////////////////
th_status_t th_get_status(const th_opt_t th)
{
    th_status_t status = eTH_OK;

    TH_ASSERT( true == gb_is_init );
    TH_ASSERT( th < eTH_NUM_OF );

    if	(	( true == gb_is_init )
        &&  ( th < eTH_NUM_OF ))
    {
        status = g_th_data[th].status;
    }
    else
    {
        status = eTH_ERROR;
    }

    return status;    
}

#if ( 1 == TH_FILTER_EN )

    ////////////////////////////////////////////////////////////////////////////////
    /*!
    * @brief        Get filtered temperature in deg C
    *
    * @param[in]    th      - Thermistor option
    * @param[out]   p_temp  - Pointer to temperature
    * @return       status  - Status of operation
    */
    ////////////////////////////////////////////////////////////////////////////////
    th_status_t th_get_degC_filt(const th_opt_t th, float32_t * const p_temp)
    {
    	th_status_t status = eTH_OK;

    	TH_ASSERT( true == gb_is_init );
    	TH_ASSERT( NULL != p_temp );
    	TH_ASSERT( th < eTH_NUM_OF );

    	if	(	( true == gb_is_init )
    		&&	( NULL != p_temp )
            &&  ( th < eTH_NUM_OF ))
    	{
    		*p_temp = g_th_data[th].temp_filt;
    	}
    	else
    	{
    		status = eTH_ERROR;
    	}
	
    	return status;
    }

    ////////////////////////////////////////////////////////////////////////////////
    /*!
    * @brief        Get filtered temperature in deg F
    *
    * @param[in]    th      - Thermistor option
    * @param[out]   p_temp  - Pointer to temperature
    * @return       status  - Status of operation
    */
    ////////////////////////////////////////////////////////////////////////////////
    th_status_t th_get_degF_filt(const th_opt_t th, float32_t * const p_temp)
    {
    	th_status_t status = eTH_OK;

    	TH_ASSERT( true == gb_is_init );
    	TH_ASSERT( NULL != p_temp );
    	TH_ASSERT( th < eTH_NUM_OF );

    	if	(	( true == gb_is_init )
    		&&	( NULL != p_temp )
            &&  ( th < eTH_NUM_OF ))
    	{
            // TODO: Convert...
    		*p_temp = g_th_data[th].temp_filt;
    	}
    	else
    	{
    		status = eTH_ERROR;
    	}
	
    	return status;
    }

    ////////////////////////////////////////////////////////////////////////////////
    /*!
    * @brief        Get filtered temperature in kelvin
    *
    * @param[in]    th      - Thermistor option
    * @param[out]   p_temp  - Pointer to temperature
    * @return       status  - Status of operation
    */
    ////////////////////////////////////////////////////////////////////////////////
    th_status_t th_get_kelvin_filt(const th_opt_t th, float32_t * const p_temp)
    {
    	th_status_t status = eTH_OK;

    	TH_ASSERT( true == gb_is_init );
    	TH_ASSERT( NULL != p_temp );
    	TH_ASSERT( th < eTH_NUM_OF );

    	if	(	( true == gb_is_init )
    		&&	( NULL != p_temp )
            &&  ( th < eTH_NUM_OF ))
    	{
            // TODO: Convert...
    		*p_temp = g_th_data[th].temp_filt;
    	}
    	else
    	{
    		status = eTH_ERROR;
    	}
	
    	return status;
    }

    ////////////////////////////////////////////////////////////////////////////////
    /*!
    * @brief        Set LPF cuttoff frequency
    *
    * @param[in]    th      - Thermistor option
    * @param[in]    fc      - Cutoff frequency of LPF
    * @return       status  - Status of operation
    */
    ////////////////////////////////////////////////////////////////////////////////
    th_status_t th_set_lpf_fc(const th_opt_t th, const float32_t fc)
    {
    	th_status_t status = eTH_OK;

    	TH_ASSERT( true == gb_is_init );
        TH_ASSERT( th < eTH_NUM_OF );
        TH_ASSERT( fc > 0.0f );

    	if  (   ( true == gb_is_init )
            &&  ( th < eTH_NUM_OF )
            &&  ( fc > 0.0f ))
    	{
            if ( eFILTER_OK != filter_rc_change_cutoff( g_th_data[th].lpf, fc, TH_HNDL_FREQ_HZ ))
            {
                status = eTH_ERROR;
            }
    	}
    	else
    	{
    		status = eTH_ERROR;
    	}
	
    	return status;
    }

    ////////////////////////////////////////////////////////////////////////////////
    /*!
    * @brief        Get LPF cuttoff frequency
    *
    * @param[in]    th      - Thermistor option
    * @param[out]   p_fc    - Pointer to LPF cutoff frequency
    * @return       status  - Status of operation
    */
    ////////////////////////////////////////////////////////////////////////////////
    th_status_t th_get_lpf_fc(const th_opt_t th, float32_t * const p_fc)
    {
    	th_status_t status = eTH_OK;

    	TH_ASSERT( true == gb_is_init );
    	TH_ASSERT( NULL != p_fc );
        TH_ASSERT( th < eTH_NUM_OF );

    	if  (   ( true == gb_is_init )
            &&  ( NULL != p_fc )
            &&  ( th < eTH_NUM_OF ))
    	{
            *p_fc = filter_rc_get_cutoff( g_th_data[th].lpf );
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
