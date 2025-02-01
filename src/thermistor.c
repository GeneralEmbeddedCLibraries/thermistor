// Copyright (c) 2023 Ziga Miklosic
// All Rights Reserved
// This software is under MIT licence (https://opensource.org/licenses/MIT)
////////////////////////////////////////////////////////////////////////////////
/**
*@file      thermistor.c
*@brief     Thermistor measurement and processing
*@author    Ziga Miklosic
*@email     ziga.miklosic@gmail.com
*@date      09.11.2023
*@version   V1.1.0
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

    /**
     *  Compatibility check with Filter module
     *
     *  Support version V2.x.x
     */
    _Static_assert( 2 == FILTER_VER_MAJOR );

#endif

////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////

/**
 *      Thermistor handler frequency
 *
 *     Unit: Hz
 */
#define TH_HNDL_FREQ_HZ         ( 1.0f / TH_HNDL_PERIOD_S )

/**
 *  Factor for NTC calculation when given nominal NTC value at 25 degC
 */
#define TH_NTC_25DEG_FACTOR     ((float32_t) ( 1.0 / 298.15 ))      // Leave double

/**
 *    PT100/500/1000 temperature calculation factors according
 *    to DIN EN60751 standard
 */
#define TH_PT_DIN_EN60751_A     ( 3.9083e-3 )    // degC^-1
#define TH_PT_DIN_EN60751_B     ( -5.775e-7 )    // degC^-2

/**
 *        Precalculated factors for PT100/500/1000 calculations
 */
#define TH_PT_DIN_EN60751_AA    (( float32_t )( TH_PT_DIN_EN60751_A * TH_PT_DIN_EN60751_A ))
#define TH_PT_DIN_EN60751_2B    (( float32_t )( 2.0 * TH_PT_DIN_EN60751_B ))
#define TH_PT_DIN_EN60751_4B    (( float32_t )( 4.0 * TH_PT_DIN_EN60751_B ))

/**
 *        PT100/500/1000 Resistance Limits
 *
 * @note Taken from "doc/pt1000_pt100_pt500_tables.xlsx" table!
 *
 *    Unit: Ohm
 */
#define TH_PT1000_MAX_OHM       ( 3904.81f )
#define TH_PT1000_MIN_OHM       ( 185.20f )
#define TH_PT100_MAX_OHM        ( 390.48f )
#define TH_PT100_MIN_OHM        ( 18.52f )
#define TH_PT500_MAX_OHM        ( 1937.74f )
#define TH_PT500_MIN_OHM        ( 114.13f )

/**
 *  Thermistor data
 */
typedef struct
{
    float32_t res;        /**<Thermistor resistance */
    float32_t temp;       /**<Temperature values in degC */
    float32_t temp_filt;  /**<Filtered temperature values in degC */

    #if ( 1 == TH_FILTER_EN )
        p_filter_rc_t lpf;   /**<Low pass filter */
    #endif

    th_status_t status;    /**<Thermistor status */
} th_data_t;

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////

/**
 * Initialization guards
 */
static bool gb_is_init = false;

/**
 *     Pointer to configuration table
 */
static const th_cfg_t * gp_cfg_table = NULL;

/**
 *  Thermistor data
 */
static th_data_t g_th_data[eTH_NUM_OF] = {0};

////////////////////////////////////////////////////////////////////////////////
// Function Prototypes
////////////////////////////////////////////////////////////////////////////////
static float32_t    th_calc_res_single_pull     (const th_ch_t th);
static float32_t    th_calc_res_both_pull       (const th_ch_t th);
static float32_t    th_calc_resistance          (const th_ch_t th);
static float32_t    th_calc_ntc_temperature     (const float32_t rth, const float32_t beta, const float32_t rth_nom);
static float32_t    th_calc_pt100_temperature   (const float32_t rth);
static float32_t    th_calc_pt500_temperature   (const float32_t rth);
static float32_t    th_calc_pt1000_temperature  (const float32_t rth);
static th_status_t  th_init_filter              (const th_ch_t th);
static th_status_t  th_status_hndl              (const th_ch_t th, const float32_t temp);
static th_status_t  th_check_cfg_table          (const th_cfg_t * const p_cfg);

static inline float32_t th_limit_f32            (const float32_t in, const float32_t min, const float32_t max);

////////////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Calculate resistance of thermistor with single pull resistor
*
* @param[in]    th  - Thermistor option
* @return       res - Resistance of thermistor
*/
////////////////////////////////////////////////////////////////////////////////
static float32_t th_calc_res_single_pull(const th_ch_t th)
{
    float32_t   th_res  = 0.0f;
    uint16_t    adc_raw = 0U;

    // Get raw adc value
    adc_get_raw( gp_cfg_table[th].adc_ch, &adc_raw );

    // Calculate ADC ratio
    const float32_t adc_ratio = ((float32_t)((float32_t) adc_get_raw_max() / (float32_t) ( adc_raw + 1U ))); // +1 to prevent dividing by zero!

    // Thermistor on low side
    if ( eTH_HW_LOW_SIDE == gp_cfg_table[th].hw.conn )
    {
        if ( adc_ratio < 1.0f )
        {
            th_res = (float32_t) ( gp_cfg_table[th].hw.pull_up / ( adc_ratio - 1.0f ));
        }
        else
        {
            th_res = 1e6f;  // ADC ration is above 1 means Rth is very high!
        }
    }

    // Thermistor on high side
    else
    {
        if ( adc_ratio < 1.0f )
        {
            th_res = (float32_t) ( gp_cfg_table[th].hw.pull_down * ( adc_ratio - 1.0f ));
        }
        else
        {
            th_res = 0.0f;  // ADC ration is above 1 means Rth is 0 ohm!
        }
    } 
    
    return th_res;     
}

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Calculate resistance of thermistor with both pull resistors
*
* @param[in]    th  - Thermistor option
* @return       res - Resistance of thermistor
*/
////////////////////////////////////////////////////////////////////////////////
static float32_t th_calc_res_both_pull(const th_ch_t th)
{
    float32_t th_res    = 0.0f;

    // TODO: Implementation needed!
    (void) th;
    
    return th_res;     
}

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Calculate resistance of thermistor
*
* @note     In case of unplasible voltage -1 is returned!
*
* @param[in]    th  - Thermistor option
* @return       res - Resistance of thermistor
*/
////////////////////////////////////////////////////////////////////////////////
static float32_t th_calc_resistance(const th_ch_t th)
{
    float32_t th_res        = 0.0f;
    float32_t th_res_lim    = 0.0f;

    // Single pull resistor
    if  (   ( eTH_HW_PULL_UP    == gp_cfg_table[th].hw.pull_mode )
        ||  ( eTH_HW_PULL_DOWN  == gp_cfg_table[th].hw.pull_mode ))
    {
        th_res = th_calc_res_single_pull( th );
    }

    // Both pull resistors
    else
    {
        th_res = th_calc_res_both_pull( th );
    }

    // Limit thermistor resistance
    switch( gp_cfg_table[th].type )
    {
        case eTH_TYPE_NTC:
            th_res_lim = th_limit_f32( th_res, 1.0f, 10e6f );
            break;

        case eTH_TYPE_PT100:
            th_res_lim = th_limit_f32( th_res, TH_PT100_MIN_OHM, TH_PT100_MAX_OHM );
            break;

        case eTH_TYPE_PT500:
            th_res_lim = th_limit_f32( th_res, TH_PT500_MIN_OHM, TH_PT500_MAX_OHM );
            break;

        case eTH_TYPE_PT1000:
            th_res_lim = th_limit_f32( th_res, TH_PT1000_MIN_OHM, TH_PT1000_MAX_OHM );
            break;

        default:
            TH_ASSERT( 0 );
            break;
    }

    return th_res_lim;
}

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Convert NTC resistance to degree C
*
* @param[in]    rth     - Resistance of NTC thermistor
* @param[in]    beta    - Beta factor of NTC
* @param[in]    rth_nom - Nominal value of NTC @25 degC
* @return       temp    - Calculated temperature
*/
////////////////////////////////////////////////////////////////////////////////
static float32_t th_calc_ntc_temperature(const float32_t rth, const float32_t beta, const float32_t rth_nom)
{
    float32_t temp = 0.0f;

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
* @param[in]    rth     - Resistance of PT100 thermistor
* @return       temp    - Calculated temperature
*/
////////////////////////////////////////////////////////////////////////////////
static float32_t th_calc_pt100_temperature(const float32_t rth)
{
    float32_t temp  = 0.0f;

    // Calculate temperature
    temp = (float32_t) (( -TH_PT_DIN_EN60751_A + sqrtf( TH_PT_DIN_EN60751_AA - TH_PT_DIN_EN60751_4B * ( 1 - rth / 100.0f ))) / TH_PT_DIN_EN60751_2B );
    
    return temp;
}

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Convert PT500 resistance to degree C
*
* @note     Calculation of PT500 according to DIN EN60751 standard.
*           For futher details look at table: doc/pt1000_pt100_pt500_tables.xlsx 
*
* @param[in]    rth     - Resistance of PT500 thermistor
* @return       temp    - Calculated temperature
*/
////////////////////////////////////////////////////////////////////////////////
static float32_t th_calc_pt500_temperature(const float32_t rth)
{
    float32_t temp  = 0.0f;

    // Calculate temperature
    temp = (float32_t) (( -TH_PT_DIN_EN60751_A + sqrtf( TH_PT_DIN_EN60751_AA - TH_PT_DIN_EN60751_4B * ( 1 - rth / 500.0f ))) / TH_PT_DIN_EN60751_2B );
    
    return temp;
}

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Convert PT1000 resistance to degree C
*
* @note     Calculation of PT1000 according to DIN EN60751 standard.
*           For futher details look at table: doc/pt1000_pt100_pt500_tables.xlsx 
*
* @param[in]    rth     - Resistance of PT1000 thermistor
* @return       temp    - Calculated temperature
*/
////////////////////////////////////////////////////////////////////////////////
static float32_t th_calc_pt1000_temperature(const float32_t rth)
{
    float32_t temp  = 0.0f;

    // Calculate temperature
    temp = (float32_t) (( -TH_PT_DIN_EN60751_A + sqrtf( TH_PT_DIN_EN60751_AA - TH_PT_DIN_EN60751_4B * ( 1 - rth / 1000.0f ))) / TH_PT_DIN_EN60751_2B );
    
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
static float32_t th_calc_temperature(const th_ch_t th)
{
    float32_t temp = 0.0f;

    // Calculate thermistor resistance
    g_th_data[th].res = th_calc_resistance( th );

    // Sensor type
    switch( gp_cfg_table[th].type )
    {
        case eTH_TYPE_NTC:
            temp = th_calc_ntc_temperature( g_th_data[th].res, gp_cfg_table[th].ntc.beta, gp_cfg_table[th].ntc.nom_val );
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
static th_status_t th_init_filter(const th_ch_t th)
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
static th_status_t th_status_hndl(const th_ch_t th, const float32_t temp)
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
* @brief        Check configuration table
*
* @param[in]    p_cfg   - Configuration table
* @return       status  - Status of operation
*/
////////////////////////////////////////////////////////////////////////////////
static th_status_t th_check_cfg_table(const th_cfg_t * const p_cfg)
{
    th_status_t status = eTH_OK;

    if ( NULL != p_cfg )
    {
        // Check all entries
        for ( uint32_t th = 0; th < eTH_NUM_OF; th++ )
        {
            /**
             *  Check for correct configuration
             *
             *      1. LPF filter cutoff frequency shall be higher that 0 Hz
             *      2. Valid HW configuration are:
             *          - eTH_HW_LOW_SIDE  with eTH_HW_PULL_UP
             *          - eTH_HW_HIGH_SIDE with eTH_HW_PULL_DOWN
             *          - eTH_HW_LOW_SIDE  with eTH_HW_PULL_BOTH
             *          - eTH_HW_HIGH_SIDE with eTH_HW_PULL_BOTH
             *      3. Range: Max is larger than min value
             */

            if  (   ( p_cfg[th].lpf_fc > 0.0f )                                                                             // 1.
                &&  (   (( eTH_HW_LOW_SIDE == p_cfg[th].hw.conn )   && ( eTH_HW_PULL_UP == p_cfg[th].hw.pull_mode ))        // 2.
                    ||  (( eTH_HW_HIGH_SIDE == p_cfg[th].hw.conn )  && ( eTH_HW_PULL_DOWN == p_cfg[th].hw.pull_mode  ))
                    ||  (( eTH_HW_LOW_SIDE == p_cfg[th].hw.conn )   && ( eTH_HW_PULL_BOTH == p_cfg[th].hw.pull_mode  ))
                    ||  (( eTH_HW_HIGH_SIDE == p_cfg[th].hw.conn )  && ( eTH_HW_PULL_BOTH == p_cfg[th].hw.pull_mode  )))
                &&  ( p_cfg[th].range.max > p_cfg[th].range.min ))                                                          // 3.
            {
                // Valid config
            }
            else
            {
                status = eTH_ERROR;
                TH_DBG_PRINT( "ERROR: Invalid thermistor configuration at %d entry!", th );
                break;
            }
        }
    }
    else
    {
        status = eTH_ERROR;
        TH_DBG_PRINT( "ERROR: Missing thermistor config table!" );
    }

    return status;
}

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Limit floating point value
*
* @param[in]    in    - Input value to limit
* @param[in]    min    - Minimum value limit
* @param[in]    max    - Maximum value
* @return       out    - Limited output value
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
*     Following functions are part of API calls.
*/
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/*!
* @brief        Init thermistors
*
* @return       status - Status of operation
*/
////////////////////////////////////////////////////////////////////////////////
th_status_t th_init(void)
{
    th_status_t status = eTH_OK;

    if ( false == gb_is_init )
    {
        // Get configuration table
        gp_cfg_table = th_cfg_get_table();

        // Check configuration table
        status = th_check_cfg_table( gp_cfg_table );
        
        // Configuration table missing
        if ( eTH_OK == status )
        {
            // Init all thermistors
            for ( uint32_t th = 0; th < eTH_NUM_OF; th++ )
            {
                // Get current temperature
                g_th_data[th].temp      = th_calc_temperature( th );
                g_th_data[th].temp_filt = g_th_data[th].temp;
                
                // Init filter
                if ( eTH_OK != th_init_filter( th ))
                {
                    status = eTH_ERROR;
                    break;
                }
            }
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
* @brief        De-init thermistors
*
* @return       status - Status of operation
*/
////////////////////////////////////////////////////////////////////////////////
th_status_t th_deinit(void)
{
    th_status_t status = eTH_OK;

    if ( true == gb_is_init )
    {
        // Reset all thermistor values
        for ( uint32_t th = 0; th < eTH_NUM_OF; th++ )
        {
            // Get current temperature
            g_th_data[th].temp      = 0.0f;
            g_th_data[th].temp_filt = 0.0f;
        }

        gb_is_init = false;
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
th_status_t th_is_init(bool * const p_is_init)
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
* @return       status - Status of operation
*/
////////////////////////////////////////////////////////////////////////////////
th_status_t th_hndl(void)
{
    th_status_t status = eTH_OK;

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
                 (void) filter_rc_hndl( g_th_data[th].lpf, g_th_data[th].temp, &g_th_data[th].temp_filt );
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
* @brief        Get RAW temperature in ADC codes
*
* @param[in]    th      - Thermistor option
* @param[out]   p_raw   - RAW temperature
* @return       status  - Status of operation
*/
////////////////////////////////////////////////////////////////////////////////
th_status_t th_get_raw(const th_ch_t th, uint16_t * const p_raw)
{
    th_status_t status = eTH_OK;

    TH_ASSERT( true == gb_is_init );
    TH_ASSERT( NULL != p_raw );
    TH_ASSERT( th < eTH_NUM_OF );

    if  (   ( true == gb_is_init )
        &&  ( NULL != p_raw )
        &&  ( th < eTH_NUM_OF ))
    {
        // Get raw adc value
        adc_get_raw( gp_cfg_table[th].adc_ch, p_raw );
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
th_status_t th_get_degC(const th_ch_t th, float32_t * const p_temp)
{
    th_status_t status = eTH_OK;

    TH_ASSERT( true == gb_is_init );
    TH_ASSERT( NULL != p_temp );
    TH_ASSERT( th < eTH_NUM_OF );

    if  (   ( true == gb_is_init )
        &&  ( NULL != p_temp )
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
th_status_t th_get_degF(const th_ch_t th, float32_t * const p_temp)
{
    th_status_t status = eTH_OK;

    TH_ASSERT( true == gb_is_init );
    TH_ASSERT( NULL != p_temp );
    TH_ASSERT( th < eTH_NUM_OF );

    if  (   ( true == gb_is_init )
        &&  ( NULL != p_temp )
        &&  ( th < eTH_NUM_OF ))
    {
        // Conversion formula: T[°F] = 9/5[°F/°C] * T[°C] + 32[°F]
        *p_temp = (float32_t)(( 1.8f * g_th_data[th].temp ) + 32.0f );
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
th_status_t th_get_kelvin(const th_ch_t th, float32_t * const p_temp)
{
    th_status_t status = eTH_OK;

    TH_ASSERT( true == gb_is_init );
    TH_ASSERT( NULL != p_temp );
    TH_ASSERT( th < eTH_NUM_OF );

    if  (   ( true == gb_is_init )
        &&  ( NULL != p_temp )
        &&  ( th < eTH_NUM_OF ))
    {
        // Conversion formula: T[K] = T[°C] + 273.15[K]
        *p_temp = (float32_t)( g_th_data[th].temp + 273.15f );
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
th_status_t th_get_resistance(const th_ch_t th, float32_t * const p_res)
{
    th_status_t status = eTH_OK;

    TH_ASSERT( true == gb_is_init );
    TH_ASSERT( NULL != p_res );
    TH_ASSERT( th < eTH_NUM_OF );

    if  (   ( true == gb_is_init )
        &&  ( NULL != p_res )
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
th_status_t th_get_status(const th_ch_t th)
{
    th_status_t status = eTH_OK;

    TH_ASSERT( true == gb_is_init );
    TH_ASSERT( th < eTH_NUM_OF );

    if  (   ( true == gb_is_init )
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
    th_status_t th_get_degC_filt(const th_ch_t th, float32_t * const p_temp)
    {
        th_status_t status = eTH_OK;

        TH_ASSERT( true == gb_is_init );
        TH_ASSERT( NULL != p_temp );
        TH_ASSERT( th < eTH_NUM_OF );

        if  (   ( true == gb_is_init )
            &&  ( NULL != p_temp )
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
    th_status_t th_get_degF_filt(const th_ch_t th, float32_t * const p_temp)
    {
        th_status_t status = eTH_OK;

        TH_ASSERT( true == gb_is_init );
        TH_ASSERT( NULL != p_temp );
        TH_ASSERT( th < eTH_NUM_OF );

        if  (   ( true == gb_is_init )
            &&  ( NULL != p_temp )
            &&  ( th < eTH_NUM_OF ))
        {
            // Conversion formula: T[°F] = 9/5[°F/°C] * T[°C] + 32[°F]
            *p_temp = (float32_t)(( 1.8f * g_th_data[th].temp_filt ) + 32.0f );
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
    th_status_t th_get_kelvin_filt(const th_ch_t th, float32_t * const p_temp)
    {
        th_status_t status = eTH_OK;

        TH_ASSERT( true == gb_is_init );
        TH_ASSERT( NULL != p_temp );
        TH_ASSERT( th < eTH_NUM_OF );

        if  (   ( true == gb_is_init )
            &&  ( NULL != p_temp )
            &&  ( th < eTH_NUM_OF ))
        {
            // Conversion formula: T[K] = T[°C] + 273.15[K]
            *p_temp = (float32_t)( g_th_data[th].temp_filt + 273.15f );
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
    th_status_t th_set_lpf_fc(const th_ch_t th, const float32_t fc)
    {
        th_status_t status = eTH_OK;

        TH_ASSERT( true == gb_is_init );
        TH_ASSERT( th < eTH_NUM_OF );
        TH_ASSERT( fc > 0.0f );

        if  (   ( true == gb_is_init )
            &&  ( th < eTH_NUM_OF )
            &&  ( fc > 0.0f ))
        {
            if ( eFILTER_OK != filter_rc_fc_set( g_th_data[th].lpf, fc ))
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
    th_status_t th_get_lpf_fc(const th_ch_t th, float32_t * const p_fc)
    {
        th_status_t status = eTH_OK;

        TH_ASSERT( true == gb_is_init );
        TH_ASSERT( NULL != p_fc );
        TH_ASSERT( th < eTH_NUM_OF );

        if  (   ( true == gb_is_init )
            &&  ( NULL != p_fc )
            &&  ( th < eTH_NUM_OF ))
        {
            (void) filter_rc_fc_get( g_th_data[th].lpf, p_fc );
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
