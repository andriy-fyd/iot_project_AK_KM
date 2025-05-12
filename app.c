/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "em_common.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "app.h"
#include "app_log.h"
#include "temperature.h"
#include "gatt_db.h" // Pour l'ID de la caractéristique GATT à renvoyer
#include "sl_sleeptimer.h"


// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;
static sl_sleeptimer_timer_handle_t temperature_timer;
static uint32_t timer_counter = 0;
void temperature_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data);

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////

  app_log_info("%s\n", __FUNCTION__);

  // Initialiser le capteur de température et humidité
  sl_status_t sc = sl_sensor_rht_init();
  app_assert_status(sc);
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application code here!                              //
  // This is called infinitely.                                              //
  // Do not call blocking functions from here!                               //
  /////////////////////////////////////////////////////////////////////////////
}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/

void temperature_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  (void)data;
  timer_counter++;
  app_log_info("Timer step %lu\n", timer_counter);
}

void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      // Create an advertising set.
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
      app_assert_status(sc);
      // Start advertising and enable connections.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      app_log_info("%s: connection opened!\n", __FUNCTION__);
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      app_log_info("%s: connection closed!\n", __FUNCTION__);

      // Désinitialiser le capteur
      sl_sensor_rht_deinit();

      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      // Restart advertising after client has disconnected.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);
      break;

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////
    ///
    case sl_bt_evt_gatt_server_user_read_request_id:
      uint8_t att_id = evt->data.evt_gatt_server_user_read_request.characteristic;
      uint8_t conn = evt->data.evt_gatt_server_user_read_request.connection;

      app_log_info("Read request on attribute ID: %d\n", att_id);

      // Si on lit la caractéristique température
      if (att_id == gattdb_temperature) {
        int16_t temp_ble = get_ble_temperature();
        uint8_t temp_buffer[2];
        temp_buffer[0] = (uint8_t)(temp_ble & 0xFF);
        temp_buffer[1] = (uint8_t)((temp_ble >> 8) & 0xFF);

        sl_bt_gatt_server_send_user_read_response(
          conn,
          att_id,
          0,
          sizeof(temp_buffer),
          temp_buffer,
          NULL
        );

        // Logging
        if (temp_ble == -32768) {
          app_log_error("Temperature read failed!\n");
        } else {
          float temp_celsius = temp_ble / 100.0f;
          int temp_int = (int)temp_celsius;
          int temp_frac = (int)((temp_celsius - temp_int) * 100);
          app_log_info("Temperature sent: %d (BLE format) -> %d.%02d deg C\n", temp_ble, temp_int, temp_frac);
        }
      }

      break;

    case sl_bt_evt_gatt_server_characteristic_status_id: {
      uint8_t att_id = evt->data.evt_gatt_server_characteristic_status.characteristic;
      uint8_t status_flags = evt->data.evt_gatt_server_characteristic_status.status_flags;
      uint16_t client_config = evt->data.evt_gatt_server_characteristic_status.client_config_flags;

      app_log_info("Characteristic status: ID=%d, flags=0x%02X, config=0x%04X\n", att_id, status_flags, client_config);

      if (att_id == gattdb_temperature && status_flags == sl_bt_gatt_server_client_config) {
        if (client_config == 0x0001) {
          // Notify activé
          sl_status_t sc = sl_sleeptimer_start_periodic_timer_ms(
            &temperature_timer,
            1000,
            temperature_timer_callback,
            NULL,
            0,
            0
          );
          app_assert_status(sc);
          app_log_info("Notify enabled -> Timer started\n");
        } else if (client_config == 0x0000) {
          // Notify désactivé
          sl_status_t sc = sl_sleeptimer_stop_timer(&temperature_timer);
          app_assert_status(sc);
          app_log_info("Notify disabled -> Timer stopped\n");
        }
      }
      break;
    }


    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}
