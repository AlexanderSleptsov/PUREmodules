/* Copyright (c) 2014 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */


#include <stdint.h>
#include <string.h>

#include "SEGGER_RTT.h"

#include "config.h"
#include "nordic_common.h"
#include "nrf.h"
#include "ble_hci.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "softdevice_handler.h"
#include "app_timer.h"
#include "app_button.h"
#include "ble_nus.h"
#include "app_uart.h"
#include "app_util_platform.h"
#include "bsp.h"
#include "bsp_btn_ble.h"


#include <stdbool.h>
#include <ctype.h>
#include "nrf_gpio.h"
#include "app_error.h"
#include "nrf_drv_twi.h"
#include "bsp.h"
#define NRF_LOG_MODULE_NAME "APP"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_delay.h"
#include "i2c_driver.h"
#include "lis3mdl.h"
#include "lis2de.h"
#include "vl53l0.h"
#include "si1153.h"
#include "veml6075.h"
#include "bme280.h"
#include "supersensor.h"
#include "ble_driver.h"
#include "p1234701ct.h"
#include "apds9250.h"
#include <math.h>


#define IS_SRVC_CHANGED_CHARACT_PRESENT 0                                           /**< Include the service_changed characteristic. If not enabled, the server's database cannot be changed for the lifetime of the device. */

#if (NRF_SD_BLE_API_VERSION == 3)
#define NRF_BLE_MAX_MTU_SIZE            GATT_MTU_SIZE_DEFAULT                       /**< MTU size used in the softdevice enabling and to reply to a BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST event. */
#endif

#define APP_FEATURE_NOT_SUPPORTED       BLE_GATT_STATUS_ATTERR_APP_BEGIN + 2        /**< Reply when unsupported features are requested. */

#define CENTRAL_LINK_COUNT              0                                           /**< Number of central links used by the application. When changing this number remember to adjust the RAM settings*/
#define PERIPHERAL_LINK_COUNT           1                                           /**< Number of peripheral links used by the application. When changing this number remember to adjust the RAM settings*/

#define DEVICE_NAME                     "PureEngineering-CoreModule"                               /**< Name of device. Will be included in the advertising data. */
#define NUS_SERVICE_UUID_TYPE           BLE_UUID_TYPE_VENDOR_BEGIN                  /**< UUID type for the Nordic UART Service (vendor specific). */

#define APP_ADV_INTERVAL                64                                          /**< The advertising interval (in units of 0.625 ms. This value corresponds to 40 ms). */
#define APP_ADV_TIMEOUT_IN_SECONDS      180                                         /**< The advertising timeout (in units of seconds). */

#define APP_ADV_INTERVAL_SLOW             0x0664

#define APP_TIMER_PRESCALER             0                                           /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_OP_QUEUE_SIZE         4                                           /**< Size of timer operation queues. */

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(20, UNIT_1_25_MS)             /**< Minimum acceptable connection interval (20 ms), Connection interval uses 1.25 ms units. */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(75, UNIT_1_25_MS)             /**< Maximum acceptable connection interval (75 ms), Connection interval uses 1.25 ms units. */
#define SLAVE_LATENCY                   0                                           /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)             /**< Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units. */
#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER)  /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000, APP_TIMER_PRESCALER) /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                           /**< Number of attempts before giving up the connection parameter negotiation. */

#define DEAD_BEEF                       0xDEADBEEF                                  /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

#define UART_TX_BUF_SIZE                256                                         /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE                256  
#define TWI_SCL_M                27   //!< Master SCL pin
#define TWI_SDA_M                26   //!< Master SDA pin                                       /**< UART RX buffer size. */
#define MASTER_TWI_INST          0    //!< TWI interface used as a master accessing EEPROM memory



static ble_nus_t                        m_nus;                                      /**< Structure to identify the Nordic UART Service. */
static uint16_t                         m_conn_handle = BLE_CONN_HANDLE_INVALID;    /**< Handle of the current connection. */

static ble_uuid_t                       m_adv_uuids[] = {{BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE}};  /**< Universally unique service identifier. */

bool ble_on = true;
bool lis2de_on = true;
bool lis3mdl_on = false;
bool bme280_on = false;
bool veml6075_on = false; 
bool si1153_on = false;
bool vl53l0_on = false;
bool apds9250_on = false;
bool p1234701ct_on = false;


/**
 * @brief TWI master instance
 *
 * Instance of TWI master driver that would be used for communication with simulated
 * eeprom memory.
 */
//TW0_USE_EAY added in nrf_drv_twi line 80
static const nrf_drv_twi_t m_twi_master = NRF_DRV_TWI_INSTANCE(MASTER_TWI_INST);



/**@brief Function for assert macro callback.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyse
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num    Line number of the failing ASSERT call.
 * @param[in] p_file_name File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}


/**@brief Function for the GAP initialization.
 *
 * @details This function will set up all the necessary GAP (Generic Access Profile) parameters of
 *          the device. It also sets the permissions and appearance.
 */
static void gap_params_init(void)
{
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *) DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the data from the Nordic UART Service.
 *
 * @details This function will process the data received from the Nordic UART BLE Service and send
 *          it to the UART module.
 *
 * @param[in] p_nus    Nordic UART Service structure.
 * @param[in] p_data   Data to be send to UART module.
 * @param[in] length   Length of the data.
 */
/**@snippet [Handling the data received over BLE] */
static void nus_data_handler(ble_nus_t * p_nus, uint8_t * p_data, uint16_t length)
{
    app_uart_put(p_data[0]);
    //Parses the input from the android app to turn on or off
    //the different sensors. 
    switch (p_data[0])
    {
        case LIS2DE_ON_MESSAGE:
            lis2de_init(m_twi_master);
            lis2de_on = true;
            break;

        case LIS2DE_OFF_MESSAGE:
            lis2de_powerdown(m_twi_master);
            lis2de_on = false;
            break;

        case LIS3MDL_ON_MESSAGE:
            lis3mdl_init(m_twi_master);
            lis3mdl_on = true;
            break;

        case LIS3MDL_OFF_MESSAGE:
            lis3mdl_powerdown(m_twi_master);
            lis3mdl_on = false;
            break;

        case BME280_ON_MESSAGE:
            bme280_init(m_twi_master);
            bme280_on = true;
            break;

        case BME280_OFF_MESSAGE:
            bme280_powerdown(m_twi_master);
            bme280_on = false;
            break;

        case VEML6075_ON_MESSAGE:
            veml6075_init(m_twi_master);
            veml6075_on = true;
            break;

        case VEML6075_OFF_MESSAGE:
            veml6075_powerdown(m_twi_master);
            veml6075_on = false;
            break;

        case SI1153_ON_MESSAGE:
            si1153_init(m_twi_master);
            si1153_on = true;
            break;

        case SI1153_OFF_MESSAGE:
            //Si1153 automatically moves to Standby Mode
            si1153_on = false;
            break;

        case APDS9250_ON_MESSAGE:
            apds9250_init(m_twi_master);
            apds9250_on = true;
            break;

        case APDS9250_OFF_MESSAGE:
            apds9250_powerdown(m_twi_master);
            apds9250_on = false;
            break;

        case P1234701CT_ON_MESSAGE:
            p1234701ct_init(m_twi_master);
            p1234701ct_on = true;
            break;

        case P1234701CT_OFF_MESSAGE:
            p1234701ct_powerdown(m_twi_master);
            p1234701ct_on = false;
            break;

        //NEED TO ADD TOF SENSOR


        default:
            break;
    }

    //Code to put ble received messages into uart

    /*for (uint32_t i = 0; i < length; i++)
    {       
        while (app_uart_put(p_data[i]) != NRF_SUCCESS);
    }

    while (app_uart_put('\r') != NRF_SUCCESS);
    while (app_uart_put('\n') != NRF_SUCCESS);*/
}

/**@snippet [Handling the data received over BLE] */


/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void)
{
    uint32_t       err_code;
    ble_nus_init_t nus_init;

    memset(&nus_init, 0, sizeof(nus_init));

    nus_init.data_handler = nus_data_handler;

    err_code = ble_nus_init(&m_nus, &nus_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling an event from the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module
 *          which are passed to the application.
 *
 * @note All this function does is to disconnect. This could have been done by simply setting
 *       the disconnect_on_fail config parameter, but instead we use the event handler
 *       mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    uint32_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}


/**@brief Function for handling errors from the Connection Parameters module.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
static void sleep_mode_enter(void)
{
    uint32_t err_code = bsp_indication_set(BSP_INDICATE_IDLE);
    APP_ERROR_CHECK(err_code);

    // Prepare wakeup buttons.
    err_code = bsp_btn_ble_sleep_mode_prepare();
    APP_ERROR_CHECK(err_code);

    // Go to system-off mode (this function will not return; wakeup will cause a reset).
    err_code = sd_power_system_off();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    uint32_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            APP_ERROR_CHECK(err_code);
            break;
        case BLE_ADV_EVT_IDLE:
            sleep_mode_enter();
            break;
        default:
            break;
    }
}


/**@brief Function for the application's SoftDevice event handler.
 *
 * @param[in] p_ble_evt SoftDevice event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    uint32_t err_code;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            APP_ERROR_CHECK(err_code);
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            break; // BLE_GAP_EVT_CONNECTED

        case BLE_GAP_EVT_DISCONNECTED:
            err_code = bsp_indication_set(BSP_INDICATE_IDLE);
            APP_ERROR_CHECK(err_code);
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            break; // BLE_GAP_EVT_DISCONNECTED

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // Pairing not supported
            err_code = sd_ble_gap_sec_params_reply(m_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
            APP_ERROR_CHECK(err_code);
            break; // BLE_GAP_EVT_SEC_PARAMS_REQUEST

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes have been stored.
            err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break; // BLE_GATTS_EVT_SYS_ATTR_MISSING

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break; // BLE_GATTC_EVT_TIMEOUT

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break; // BLE_GATTS_EVT_TIMEOUT

        case BLE_EVT_USER_MEM_REQUEST:
            err_code = sd_ble_user_mem_reply(p_ble_evt->evt.gattc_evt.conn_handle, NULL);
            APP_ERROR_CHECK(err_code);
            break; // BLE_EVT_USER_MEM_REQUEST

        case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
        {
            ble_gatts_evt_rw_authorize_request_t  req;
            ble_gatts_rw_authorize_reply_params_t auth_reply;

            req = p_ble_evt->evt.gatts_evt.params.authorize_request;

            if (req.type != BLE_GATTS_AUTHORIZE_TYPE_INVALID)
            {
                if ((req.request.write.op == BLE_GATTS_OP_PREP_WRITE_REQ)     ||
                    (req.request.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_NOW) ||
                    (req.request.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_CANCEL))
                {
                    if (req.type == BLE_GATTS_AUTHORIZE_TYPE_WRITE)
                    {
                        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
                    }
                    else
                    {
                        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_READ;
                    }
                    auth_reply.params.write.gatt_status = APP_FEATURE_NOT_SUPPORTED;
                    err_code = sd_ble_gatts_rw_authorize_reply(p_ble_evt->evt.gatts_evt.conn_handle,
                                                               &auth_reply);
                    APP_ERROR_CHECK(err_code);
                }
            }
        } break; // BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST

#if (NRF_SD_BLE_API_VERSION == 3)
        case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
            err_code = sd_ble_gatts_exchange_mtu_reply(p_ble_evt->evt.gatts_evt.conn_handle,
                                                       NRF_BLE_MAX_MTU_SIZE);
            APP_ERROR_CHECK(err_code);
            break; // BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST
#endif

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for dispatching a SoftDevice event to all modules with a SoftDevice
 *        event handler.
 *
 * @details This function is called from the SoftDevice event interrupt handler after a
 *          SoftDevice event has been received.
 *
 * @param[in] p_ble_evt  SoftDevice event.
 */
static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
{
    ble_conn_params_on_ble_evt(p_ble_evt);
    ble_nus_on_ble_evt(&m_nus, p_ble_evt);
    on_ble_evt(p_ble_evt);
    ble_advertising_on_ble_evt(p_ble_evt);
    bsp_btn_ble_on_ble_evt(p_ble_evt);

}


/**@brief Function for the SoftDevice initialization.
 *
 * @details This function initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    uint32_t err_code;

    nrf_clock_lf_cfg_t clock_lf_cfg = NRF_CLOCK_LFCLKSRC;

    // Initialize SoftDevice.
    SOFTDEVICE_HANDLER_INIT(&clock_lf_cfg, NULL);

    ble_enable_params_t ble_enable_params;
    err_code = softdevice_enable_get_default_config(CENTRAL_LINK_COUNT,
                                                    PERIPHERAL_LINK_COUNT,
                                                    &ble_enable_params);
    APP_ERROR_CHECK(err_code);

    //Check the ram settings against the used number of links
    CHECK_RAM_START_ADDR(CENTRAL_LINK_COUNT,PERIPHERAL_LINK_COUNT);

    // Enable BLE stack.
#if (NRF_SD_BLE_API_VERSION == 3)
    ble_enable_params.gatt_enable_params.att_mtu = NRF_BLE_MAX_MTU_SIZE;
#endif
    err_code = softdevice_enable(&ble_enable_params);
    APP_ERROR_CHECK(err_code);

    // Subscribe for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling events from the BSP module.
 *
 * @param[in]   event   Event generated by button press.
 */
void bsp_event_handler(bsp_event_t event)
{
    uint32_t err_code;
    switch (event)
    {
        case BSP_EVENT_SLEEP:
            sleep_mode_enter();
            break;

        case BSP_EVENT_DISCONNECT:
            err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            if (err_code != NRF_ERROR_INVALID_STATE)
            {
                APP_ERROR_CHECK(err_code);
            }
            break;

        case BSP_EVENT_WHITELIST_OFF:
            if (m_conn_handle == BLE_CONN_HANDLE_INVALID)
            {
                err_code = ble_advertising_restart_without_whitelist();
                if (err_code != NRF_ERROR_INVALID_STATE)
                {
                    APP_ERROR_CHECK(err_code);
                }
            }
            break;

        default:
            break;
    }
}


/**@brief   Function for handling app_uart events.
 *
 * @details This function will receive a single character from the app_uart module and append it to
 *          a string. The string will be be sent over BLE when the last character received was a
 *          'new line' i.e '\r\n' (hex 0x0D) or if the string has reached a length of
 *          @ref NUS_MAX_DATA_LENGTH.
 */
/**@snippet [Handling the data received over UART] */
void uart_event_handle(app_uart_evt_t * p_event)
{
    static uint8_t data_array[BLE_NUS_MAX_DATA_LEN];
    static uint8_t index = 0;
    uint32_t       err_code;

    switch (p_event->evt_type)
    {
        case APP_UART_DATA_READY:
            UNUSED_VARIABLE(app_uart_get(&data_array[index]));
            index++;

            if ((data_array[index - 1] == '\n') || (index >= (BLE_NUS_MAX_DATA_LEN)))
            {
                err_code = ble_nus_string_send(&m_nus, data_array, index);
                if (err_code != NRF_ERROR_INVALID_STATE)
                {
                    APP_ERROR_CHECK(err_code);
                }

                index = 0;
            }
            break;

        case APP_UART_COMMUNICATION_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_communication);
            break;

        case APP_UART_FIFO_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_code);
            break;

        default:
            break;
    }
}
/**@snippet [Handling the data received over UART] */


/**@brief  Function for initializing the UART module.
 */
/**@snippet [UART Initialization] */
static void uart_init(void)
{
    uint32_t err_code;
    const app_uart_comm_params_t comm_params =
    {
        RX_PIN_NUMBER,
        TX_PIN_NUMBER,
        RTS_PIN_NUMBER,
        CTS_PIN_NUMBER,
        APP_UART_FLOW_CONTROL_DISABLED,
        false,
        UART_BAUDRATE_BAUDRATE_Baud115200
    };

    APP_UART_FIFO_INIT( &comm_params,
                       UART_RX_BUF_SIZE,
                       UART_TX_BUF_SIZE,
                       uart_event_handle,
                       APP_IRQ_PRIORITY_LOWEST,
                       err_code);
    APP_ERROR_CHECK(err_code);
}
/**@snippet [UART Initialization] */


/**@brief Function for initializing the Advertising functionality.
 */
static void advertising_init(void)
{
    uint32_t               err_code;
    ble_advdata_t          advdata;
    ble_advdata_t          scanrsp;
    ble_adv_modes_config_t options;

    // Build advertising data struct to pass into @ref ble_advertising_init.
    memset(&advdata, 0, sizeof(advdata));
    advdata.name_type          = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance = false;
    advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;

    memset(&scanrsp, 0, sizeof(scanrsp));
    scanrsp.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    scanrsp.uuids_complete.p_uuids  = m_adv_uuids;

    memset(&options, 0, sizeof(options));
    options.ble_adv_fast_enabled  = true;
    options.ble_adv_fast_interval = APP_ADV_INTERVAL;
    options.ble_adv_fast_timeout  = APP_ADV_TIMEOUT_IN_SECONDS;

    options.ble_adv_slow_enabled  = true;
    options.ble_adv_slow_interval = APP_ADV_INTERVAL_SLOW;
    options.ble_adv_slow_timeout  = 0;

    err_code = ble_advertising_init(&advdata, &scanrsp, &options, on_adv_evt, NULL);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing buttons and leds.
 *
 * @param[out] p_erase_bonds  Will be true if the clear bonding button was pressed to wake the application up.
 */
static void buttons_leds_init(bool * p_erase_bonds)
{
    bsp_event_t startup_event;

    uint32_t err_code = bsp_init(BSP_INIT_LED | BSP_INIT_BUTTONS,
                                 APP_TIMER_TICKS(100, APP_TIMER_PRESCALER),
                                 bsp_event_handler);
    APP_ERROR_CHECK(err_code);

    err_code = bsp_btn_ble_init(NULL, &startup_event);
    APP_ERROR_CHECK(err_code);

    *p_erase_bonds = (startup_event == BSP_EVENT_CLEAR_BONDING_DATA);
}


/**@brief Function for placing the application in low power state while waiting for events.
 *//*
static void power_manage(void)
{
    uint32_t err_code = sd_app_evt_wait();
    APP_ERROR_CHECK(err_code);
}*/



/**
 * @brief Initialize the master TWI
 *
 * Function used to initialize master TWI interface that would communicate with simulated EEPROM.
 *
 * @return NRF_SUCCESS or the reason of failure
 */
static ret_code_t twi_master_init(void)
{
    ret_code_t ret;
    const nrf_drv_twi_config_t config =
    {
       .scl                = TWI_SCL_M,
       .sda                = TWI_SDA_M,
       .frequency          = NRF_TWI_FREQ_400K,
       .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
       .clear_bus_init     = false
    };

    ret = nrf_drv_twi_init(&m_twi_master, &config, NULL, NULL);

    if (NRF_SUCCESS == ret)
    {
        nrf_drv_twi_enable(&m_twi_master);
    }

    return ret;
}

static void fin_dis(int anaOut) {
	float result = 109.579* pow(anaOut, -0.415);
	
	NRF_LOG_RAW_INFO("Distance---->" NRF_LOG_FLOAT_MARKER "\r\n", NRF_LOG_FLOAT(result));
	
}

int8_t run_si1153_local_ble(nrf_drv_twi_t twi_master,ble_nus_t m_nus){
	int si1153_data;
	int si1153_R;
	int si1153_IR1;
	int si1153_IR2;

	uint8_t length = 18;
	uint8_t *ble_string[length];

	nrf_delay_ms(10);

	
	bsp_board_led_invert(0);
	

		//read lis2de data from the chip
	uint8_t who_am_i = lis2de_whoami(twi_master);
	NRF_LOG_RAW_INFO("Accelerometer who_am_i: %x.\r\n", who_am_i);
	uint8_t status = lis2de_readStatus(twi_master);
	NRF_LOG_RAW_INFO("Accelerometer Status: %x.\r\n", status);
	int8_t OUT_X = lis2de_readOUT_X(twi_master);
	int8_t OUT_Y = lis2de_readOUT_Y(twi_master);
	int8_t OUT_Z = lis2de_readOUT_Z(twi_master);
	
	
	// read si1153 data from the chip
	si1153_IR1 = si1153_data = si1153_get_channel_data(twi_master,0);
	si1153_IR2 = si1153_get_channel_data(twi_master,1);
	si1153_R = si1153_get_channel_data(twi_master,2);
	

	
	//send out si1153 data to the phone via ble
	memset(ble_string, 0, 20);
	sprintf((char *)ble_string, "%05d,%05d,%05d,",si1153_IR1, si1153_IR2, si1153_R);
	send_ble_data(m_nus,(uint8_t *)ble_string,length);

	//send out lis2de data to the phone via ble
	memset(ble_string, 0, 20);
	sprintf((char *)ble_string, "%05d,%05d,%05d\n",OUT_X, OUT_Y, OUT_Z);
	send_ble_data(m_nus,(uint8_t *)ble_string,length);

	
	NRF_LOG_RAW_INFO("%05d,%05d,%05d,%05d,%05d,%05d\n\r", si1153_IR1,si1153_IR2,si1153_R,OUT_X, OUT_Y, OUT_Z);
	send_command(m_twi_master,Si1153_FORCE);


	NRF_LOG_FLUSH();
	return 0;



}


int8_t run_lis2de_local_ble(nrf_drv_twi_t twi_master,ble_nus_t m_nus){
	uint8_t length = 19;
	uint8_t *ble_string[length];


	int8_t OUT_X = lis2de_readOUT_X(twi_master);
	sprintf((char *)ble_string, "lis2dex: %.2x \r\n",OUT_X);
	send_ble_data(m_nus,(uint8_t *)ble_string,length);
	

	int8_t OUT_Y = lis2de_readOUT_Y(twi_master);
	sprintf((char *)ble_string, "lis2dey: %x \r\n",OUT_Y);
        send_ble_data(m_nus,(uint8_t *)ble_string,length);
	
	int8_t OUT_Z = lis2de_readOUT_Z(twi_master);
	printf((char *)ble_string, "lis2deZ: %x \r\n",OUT_Z);
        send_ble_data(m_nus,(uint8_t *)ble_string,length);
	

	return 0;

}


//Code that runs in main to read data from sensors that are on
void print_to_ble(void){
  //  if(lis2de_on){
       // run_lis2de_local_ble(m_twi_master,m_nus);
       //
        run_si1153_local_ble(m_twi_master, m_nus);
//    }
	
	
  /*  if(lis3mdl_on){
        run_lis3mdl_ble(m_twi_master,m_nus);
    }
    if(bme280_on){
        run_bme280_ble(m_twi_master,m_nus);
    }
    if(veml6075_on){
        run_veml6075_ble(m_twi_master,m_nus);
    }
    if(si1153_on){
        run_si1153_ble(m_twi_master,m_nus);
    }
    if(vl53l0_on){
    }
    if(apds9250_on){
        run_apds9250_ble(m_twi_master,m_nus);
    }
    if(p1234701ct_on){
        run_p1234701ct_ble(m_twi_master,m_nus);
    }
*/
}

APP_TIMER_DEF(sensor_loop_timer_id);

static void sensor_loop_handler(void * p_context)
{
   print_to_ble();  
}

static void create_sensor_timer()
{   
    uint32_t err_code;

    // Create timers
    err_code = app_timer_create(&sensor_loop_timer_id, APP_TIMER_MODE_REPEATED, sensor_loop_handler);
    APP_ERROR_CHECK(err_code);

     err_code = app_timer_start(sensor_loop_timer_id, APP_TIMER_TICKS(50, APP_TIMER_PRESCALER), NULL);
     APP_ERROR_CHECK(err_code);
}


/**@brief Application main function.
 */




int si1153_test(void)
{


	NRF_LOG_RAW_INFO("si1153 test\n\r");
	NRF_LOG_FLUSH();   
	NRF_LOG_RAW_INFO("reach before send_command \n\r");
	send_command(m_twi_master,Si1153_RESET_SW);
	nrf_delay_ms(10);
	//si1153_init(m_twi_master);
	NRF_LOG_RAW_INFO("reach after send_command \n\r");
	param_set(m_twi_master, Si1153_CHAN_LIST, 0x07);

	param_set(m_twi_master, Si1153_LED1_A, 0x3F);
	param_set(m_twi_master, Si1153_LED2_A, 0x3F);
	param_set(m_twi_master, Si1153_LED3_A, 0x3F);

	param_set(m_twi_master, Si1153_ADCCONFIG_0, 0x62);
	param_set(m_twi_master, Si1153_MEASCONFIG_0, 0x01);
	param_set(m_twi_master, Si1153_ADCSENS_0, 0x01);

	param_set(m_twi_master, Si1153_ADCCONFIG_1, 0x62);
	param_set(m_twi_master, Si1153_MEASCONFIG_1, 0x02);
	param_set(m_twi_master, Si1153_ADCSENS_1, 0x01);

	param_set(m_twi_master, Si1153_ADCCONFIG_2, 0x62);
	param_set(m_twi_master, Si1153_MEASCONFIG_2, 0x04);
	param_set(m_twi_master, Si1153_ADCSENS_2, 0x01);

	send_command(m_twi_master,Si1153_FORCE);
	
	
	NRF_LOG_FLUSH();   

	return 0;

}

int main(void)
{
    uint32_t err_code;
    bool erase_bonds;

    /* Initializing TWI master interface for SuperSensor */
    err_code = twi_master_init();
    APP_ERROR_CHECK(err_code);

    APP_ERROR_CHECK(NRF_LOG_INIT(NULL));

    // Initialize.
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, false);
    uart_init();

	lis2de_init(m_twi_master);
	lis2de_on = true;
    buttons_leds_init(&erase_bonds);
    ble_stack_init();
    gap_params_init();
    services_init();
    advertising_init();
    conn_params_init();
    bsp_board_leds_init();

    //test_supersensor(m_twi_master);

  //NRF_LOG_RAW_INFO("UART Start!------->\n\r");
   err_code = ble_advertising_start(BLE_ADV_MODE_FAST);
  // NRF_LOG_RAW_INFO("UART Start!------->\n\r");
    APP_ERROR_CHECK(err_code);
	
	si1153_test();
    NRF_LOG_RAW_INFO(",ch0, chan1, chan2\n\r");
	
    NRF_LOG_FLUSH();

    create_sensor_timer();

    // Enter main loop.
    for (;;)
    {
	//NRF_LOG_RAW_INFO("UART Start!------->\n\r");
	//SEGGER_RTT_TerminalOut(1, "Hello World!\n");
	NRF_LOG_FLUSH(); 
	err_code = sd_app_evt_wait();
	APP_ERROR_CHECK(err_code);
	
    }
	
}


/**
 * @}
 */
