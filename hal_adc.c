/*
 * Copyright 2016-2023, Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
 *
 * This software, including source code, documentation and related
 * materials ("Software") is owned by Cypress Semiconductor Corporation
 * or one of its affiliates ("Cypress") and is protected by and subject to
 * worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you
 * obtained this Software ("EULA").
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software
 * source code solely for use in connection with Cypress's
 * integrated circuit products.  Any reproduction, modification, translation,
 * compilation, or representation of this Software except as specified
 * above is prohibited without the express written permission of Cypress.
 *
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
 * reserves the right to make changes to the Software without notice. Cypress
 * does not assume any liability arising out of the application or use of the
 * Software or any product or circuit described in the Software. Cypress does
 * not authorize its products for use in any products where a malfunction or
 * failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer
 * of such system or application assumes all risk of such use and in doing
 * so agrees to indemnify Cypress against all liability.
 */

/** @file
 *  hal_adc.c
 *
 * @brief
 *  This file contains the starting point of thermistor application.
 *  The application_start registers for Bluetooth stack in this file.
 *  This Wiced application initiates the HAL ADC drivers and reads the
 *  raw sample value and voltage once in every 5 seconds.
 */

/******************************************************************************
 *                                Includes
 ******************************************************************************/
#include "sparcommon.h"
#include "wiced_bt_cfg.h"
#include "wiced_bt_dev.h"
#include "wiced_bt_trace.h"
#include "wiced_gki.h"
#include "wiced_hal_adc.h"
#include "wiced_platform.h"
#include "wiced_hal_puart.h"
#include "wiced_timer.h"
#include "wiced_bt_stack.h"
#include "wiced_platform.h"

/******************************************************************************
 *                                Constants
 ******************************************************************************/
/* Devices that support all ADC APIs currently */
#define DEVICE_SUPPORTS_FULL_ADC_API  (defined(CYW20819) || defined(CYW20820))

/* Number of samples to be taken for doing averaged filtering */
#define AVG_NUM_OF_SAMPLES            3

/* Seconds timer (Timeout in seconds) */
#define APP_TIMEOUT_IN_SECONDS        5

/*
 * Macro function for debug log separators - readability
 * N represents the number of characters to be printed
 */
#define PRINT_N_ASTERISKS(N)           \
WICED_BT_TRACE("\r\n");                \
for(int i = 0; i < N; i++) {           \
   WICED_BT_TRACE("*");                \
}                                      \
WICED_BT_TRACE("\r\n");

/* Stringizing the passed variable */
#define GET_VARIABLE_NAME(x)  #x

/******************************************************************************
 *                                Structures
 ******************************************************************************/
extern const wiced_bt_cfg_settings_t wiced_bt_cfg_settings;
extern const wiced_bt_cfg_buf_pool_t wiced_bt_cfg_buf_pools[];

/******************************************************************************
 *                                Variables Definitions
 ******************************************************************************/
wiced_timer_t seconds_timer;                        /* Seconds timer instance */

/******************************************************************************
 *                          Function Declarations
 ******************************************************************************/
wiced_result_t
sample_adc_app_management_cback(wiced_bt_management_evt_t event,
                                wiced_bt_management_evt_data_t *p_event_data);

static void seconds_app_timer_cb(uint32_t arg);

static void adc_readings(ADC_INPUT_CHANNEL_SEL channel, char* channel_name);

#if DEVICE_SUPPORTS_FULL_ADC_API
static UINT32 convert_adc_raw_to_mvolt(INT16 raw_val);
#endif

/******************************************************************************
 *                          Function Definitions
 ******************************************************************************/

/*
 Function name:
 application_start

 Function Description:
 @brief    Starting point of your application

 @param void

 @return void
 */
APPLICATION_START( )
{

#ifdef WICED_BT_TRACE_ENABLE
    /* Set to PUART to see traces on peripheral uart(puart) */
    wiced_set_debug_uart(WICED_ROUTE_DEBUG_TO_PUART);
#ifdef CYW20706A2
    wiced_hal_puart_select_uart_pads( WICED_PUART_RXD, WICED_PUART_TXD, 0, 0);
#endif
#endif
    PRINT_N_ASTERISKS(70);
    WICED_BT_TRACE("              ADC Sample Application\r\n");
    PRINT_N_ASTERISKS(70);
    WICED_BT_TRACE(
        "This application measures voltage on the selected DC channel\r\n"
        "every 5 seconds(configurable) and displays both the raw\r\n"
        "sample and converted voltage values via chosen UART.\r\n"
        );
    PRINT_N_ASTERISKS(70);

    wiced_bt_stack_init(sample_adc_app_management_cback,
                        &wiced_bt_cfg_settings,
                        wiced_bt_cfg_buf_pools);
}


/*
 Function name:
 sample_adc_app_management_cback

 Function Description:
 @brief    Callback function that will be invoked by application_start()

 @param  event           Bluetooth management event type
 @param  p_event_data    Pointer to the the bluetooth management event data

 @return timer success/failure status of the callback function
 */
wiced_result_t
sample_adc_app_management_cback(wiced_bt_management_evt_t event,
                                wiced_bt_management_evt_data_t *p_event_data)
{
    WICED_BT_TRACE("Received Event : %d\n\n\r", event);

    switch(event)
    {
    /* Bluetooth  stack enabled */
    case BTM_ENABLED_EVT:
        /* Initialize the necessary peripherals (ADC) */
        wiced_hal_adc_init();

        /*
         * Configure seconds periodic timer and start timer with
         * APP_TIMEOUT_IN_SECONDS
         */
        wiced_init_timer(&seconds_timer,
                         seconds_app_timer_cb,
                         0,
                         WICED_SECONDS_PERIODIC_TIMER);
        wiced_start_timer(&seconds_timer,
                          APP_TIMEOUT_IN_SECONDS);
        break;

    default:
        WICED_BT_TRACE("Unknown Event \r\n");
        break;
    }

    return WICED_SUCCESS;
}

/*
 Function name:
 seconds_app_timer_cb

 Function Description:
 @brief    This callback function is invoked on timeout of seconds_timer.

 @param arg    unused argument

 @return void
 */
static void seconds_app_timer_cb(uint32_t arg)
{

    PRINT_N_ASTERISKS(70);

    adc_readings(ADC_INPUT_P0, GET_VARIABLE_NAME(ADC_INPUT_P0));
    adc_readings(ADC_INPUT_ADC_BGREF, GET_VARIABLE_NAME(ADC_INPUT_ADC_BGREF));
    #ifdef ADC_INPUT_VDDIO
    adc_readings(ADC_INPUT_VDDIO, GET_VARIABLE_NAME(ADC_INPUT_VDDIO));
    #endif
    adc_readings(ADC_INPUT_VDD_CORE, GET_VARIABLE_NAME(ADC_INPUT_VDD_CORE));

}


/*
 Function name:
 adc_readings

 Function Description:
 @brief    This function allows the user to get the ADC readings of the
           particular channel that is passed.

 @param channel       ADC channel to be sampled

 @return void doesnt return anything
 */
static void adc_readings(ADC_INPUT_CHANNEL_SEL channel, char* channel_name)
{

    UINT32 voltage_val = 0;
    INT16 sign_raw_val = 0;
#if DEVICE_SUPPORTS_FULL_ADC_API
    INT16 conv_val = 0;
#endif

    /*
     * Measure the sample(raw and voltage values) on the channel being passed
     * as an argument
     */
    voltage_val = wiced_hal_adc_read_voltage(channel);
#if defined(CYW20706A2) || defined(CYW43012C0)
    sign_raw_val = wiced_hal_adc_read_raw_sample(channel);
#else
    sign_raw_val = wiced_hal_adc_read_raw_sample(channel, AVG_NUM_OF_SAMPLES);
#endif
#if DEVICE_SUPPORTS_FULL_ADC_API
    conv_val = convert_adc_raw_to_mvolt(sign_raw_val);
#endif

    WICED_BT_TRACE("ADC Channel: %s\r\n", channel_name);

    WICED_BT_TRACE("Signed Raw Sample value\t\t\t\t: %d\r\n", sign_raw_val);
    WICED_BT_TRACE("FW Voltage value(in mV)\t\t\t\t: %d\r\n", voltage_val);
#if DEVICE_SUPPORTS_FULL_ADC_API
    WICED_BT_TRACE("Voltage equivalent of received sample(in mV)\t: %d\r\n",
                   conv_val);
#endif

    WICED_BT_TRACE("\r\n");

}

#if DEVICE_SUPPORTS_FULL_ADC_API
/*
 Function name:
 convert_adc_raw_to_mvolt

 Function Description:
 @brief    This function allows the user to convert a raw sample into
           its equivalent voltage.

 @param raw_val    Sampled raw value from ADC

 @return millivoltage equivalent of sampled raw value
 */
static UINT32 convert_adc_raw_to_mvolt(INT16 raw_val)
{
    INT32 mvolt = raw_val;
    INT32 gnd_reading = wiced_hal_adc_get_ground_offset();
    INT32 ref_reading = wiced_hal_adc_get_reference_reading();
    UINT32 ref_mvolts = wiced_hal_adc_get_reference_micro_volts();

    if (mvolt == 0)
    {
        return 0;
    }

    if (mvolt < gnd_reading)
    {
        mvolt = gnd_reading;
    }

    mvolt -= gnd_reading;
    mvolt *= ref_mvolts;
    mvolt += ((ref_reading - gnd_reading) >> 1);           /* For rounding up */
    mvolt /= (ref_reading - gnd_reading);

    return (uint32_t)mvolt;
}
#endif
