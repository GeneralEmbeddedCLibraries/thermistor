# **Thermistor**

Thermistor module converts temperature sensor measurement into real values in °C, °F or Kelvin units. Module is written in C programming lang with empasis to be highly portable and configurable to different HW layouts and temperature sensors. As name suggest module supports only pasive temperature measurement devices. For now NTC and PT1000.

Supported thermistors HW topologies:
 - NTC with pull-up resistor
 - PT1000 with pull-up resistor 


## **Dependencies**

### **1. ADC Low Level driver**
It is mandatory to have following ADC low level driver API functions:
 - Function to retriev voltage on pin in volts. Prototype function: 

```C 
float32_t adc_get_real (const adc_pins_t pin)
```

Additionally ADC low level driver must take following path:
```
"root/drivers/periphery/adc/adc.h"
```

### **2. Filter module **
If enabled filter *THERMISTOR_FILTER_EN = 1*, then [Filter](https://github.com/GeneralEmbeddedCLibraries/filter) must be part of project. Filter module must take following path:
```
"root/middleware/filter/src/filter.h"
```

## **General Embedded C Libraries Ecosystem**
In order to be part of *General Embedded C Libraries Ecosystem* this module must be placed in following path: 

```
root/drivers/devices/thermistor/"module_space"
```

## **API**
| API Functions | Description | Prototype |
| --- | ----------- | ----- |
| **th_init** | Initialization of thermistor module | th_status_t th_init(void) |


## **Usage**

**GENERAL NOTICE: Put all user code between sections: USER CODE BEGIN & USER CODE END!**


1. Copy template files to root directory of module.
2. Configure CLI module for application needs. Configuration options are following:

| Configuration | Description |
| --- | --- |
| **TH_HNDL_PERIOD_S** 			| Period of main thermistor handler in seconds. |


3. List all used thermistors inside *thermistor_cfg.h*:
    ```C
    /**
    *  Thermistor count
    */
    typedef enum
    {
        // USER CODE BEGIN...
        
        eTH_NTC_INT = 0,
        eTH_NTC_COMP,
        eTH_PTC_AUX,

        // USER CODE END...

        eTH_NUM_OF
    } th_opt_t;
    ```

4. Setup thermistor configurations inside *thermistor_cfg.c*:
TODO: 

5. Initialize thermistor module:
	```C
	// Init thermistor
	if ( eTH_OK != th_init())
	{
        // Initialization error...
		// Furhter actions here...
	}
	```

6. Make sure to call *th_hndl()* at fixed period of *TH_HNDL_PERIOD_S* configurations:
	```C
	@at TH_HNDL_PERIOD_S period
	{
        // Handle thermistor sensors
        th_hndl();
	}
	```



