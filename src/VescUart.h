#ifndef _VESCUART_h
#define _VESCUART_h

#include <Arduino.h>
#include "datatypes.h"
#include "buffer.h"
#include "crc.h"
#define ESP32_COMMAND_ID 102
typedef enum
{
  ESP_COMMAND_GET_READY=0,
	ESP_COMMAND_GET_ADV_INFO,   // get ADVANCED setting only for SPESC
	ESP_COMMAND_ENGINE_SOUND_INFO, // engine sound info , erpm ,duty
	// esp32 get the sound trigger
	ESP_COMMAND_SOUND_GET,
	// app set sound trigger
	ESP_COMMAND_SOUND_SET,
   //enable item data
  ESP_COMMAND_ENABLE_ITEM_INFO,

} esp_commands;

typedef enum
{
	SOUND_HORN_TRIGGERED ,
	SOUND_EXCUSE_ME_TRIGGERED,
	SOUND_POLICE_TRIGGERED,
} sound_triggered_mask;



//Determine the function of a certain bit
typedef enum
{
EXT_DCDC_ENABLE_MASK_BIT=0,
ENGINE_SOUND_ENABLE_MASK_BIT,
START_UP_WARNING_ENABLE_MASK_BIT,
} float_enable_mask;



  class VescUart
  {

  // Timeout - specifies how long the function will wait for the vesc to respond
  const uint32_t _TIMEOUT;
 /**This data structure is used for engine sound */
  struct soundData_t
  { float pidOutput;
    uint8_t swState;
    float erpm;
    float inputVoltage;
    float motorCurrent;
  };


  struct advancedData_t
  {
  uint8_t lights_mode;
	uint8_t idle_warning_time;
	uint16_t engine_sound_volume;
	uint8_t over_speed_warning;
  float battery_level;
  float low_battery_warning_level;
  uint8_t engine_sampling_data;
  };
  
public:


  /**
   * @brief      Class constructor
   */
  VescUart(uint32_t timeout_ms = 200);

  /**
   * @brief      Set the serial port for uart communication
   * @param      port  - Reference to Serial port (pointer)
   */
  void setSerialPort(Stream *port);

  /**
   * @brief      Set the serial port for debugging
   * @param      port  - Reference to Serial port (pointer)
   */
  void setDebugPort(Stream *port);

  /**Send uart command function*/
  bool get_vesc_ready(void);

  bool soundUpdate(void);

  bool advancedUpdate(void);

uint8_t get_enable_item_data(void);

  uint8_t get_sound_triggered(void);

  /**
   *Only return data, need to use the above function to update
   */
  /**sound value Update with soundUdate() */
  float get_erpm(void);
  float get_input_voltage(void);
  float get_pid_output(void);
  uint8_t get_switch_state(void);
 float get_motor_current(void);
  // advanced data Update with advancedUpdate*/
  uint16_t get_engine_sound_volume(void);
  uint8_t get_over_speed_value(void);
  uint8_t get_idle_warning_time(void);
  float get_battery_level(void);
  float get_low_battery_warning_level(void);
//Use motor current , Erpm , pid as engine throttle
uint8_t get_engine_sampling(void);
private:
  /** Variabel to hold the reference to the Serial object to use for UART */
  Stream *serialPort = NULL;

  /** Variabel to hold the reference to the Serial object to use for debugging.
   * Uses the class Stream instead of HarwareSerial */
  Stream *debugPort = NULL;
  soundData_t engineData;
  advancedData_t settingData;
  uint8_t soundTriggered=0;
  uint8_t enableItemData=0;

  bool isVescReady=0; // check float_enable_mask neum 
  /**
   * @brief      Packs the payload and sends it over Serial
   *
   * @param      payload  - The payload as a unit8_t Array with length of int lenPayload
   * @param      lenPay   - Length of payload
   * @return     The number of bytes send
   */
  int packSendPayload(uint8_t *payload, int lenPay);

  /**
   * @brief      Receives the message over Serial
   *
   * @param      payloadReceived  - The received payload as a unit8_t Array
   * @return     The number of bytes receeived within the payload
   */
  int receiveUartMessage(uint8_t *payloadReceived);

  /**
   * @brief      Verifies the message (CRC-16) and extracts the payload
   *
   * @param      message  - The received UART message
   * @param      lenMes   - The lenght of the message
   * @param      payload  - The final payload ready to extract data from
   * @return     True if the process was a success
   */
  bool unpackPayload(uint8_t *message, int lenMes, uint8_t *payload);

  /**
   * @brief      Extracts the data from the received payload
   *
   * @param      message  - The payload to extract data from
   * @return     True if the process was a success
   */
  bool processReadPacket(uint8_t *message, int lenPay);

  /**
   * @brief      Help Function to print uint8_t array over Serial for Debug
   *
   * @param      data  - Data array to print
   * @param      len   - Lenght of the array to print
   */
  void serialPrint(uint8_t *data, int len);
  };

#endif
