#ifndef _VESCUART_h
#define _VESCUART_h

#include <Arduino.h>
#include "datatypes.h"
#include "buffer.h"
#include "crc.h"

typedef enum
{

	ESP_COMMAND_GET_ADVANCED,	   // get ADVANCED setting only for SPESC
	ESP_COMMAND_ENGINE_SOUND_INFO, // engine sound info , erpm ,duty
								   
	ESP_COMMAND_GET_DUTYCYCLE,
	ESP_COMMAND_GET_ERPM,
	ESP_COMMAND_GET_PID_OUTPUT,
	ESP_COMMAND_GET_SWITCH_STATE,
	ESP_COMMAND_GET_MOTOR_CURRENT,
	ESP_COMMAND_GET_INPUT_VOLTAGE,
	// advanced
	ESP_COMMAND_GET_LIGHT_MODE,
	ESP_COMMAND_GET_IDLE_WARN_TIME,
	ESP_COMMAND_GET_ENGINE_SOUND_VOLUME,
	ESP_COMMAND_GET_ENABLE_DATA, //reference to float_enable_mask
	ESP_COMMAND_GET_OVER_SPEED_WARN,
	// button
	// esp32 get the sound trigger
	ESP_COMMAND_SOUND_GET,
	// app set sound trigger
	ESP_COMMAND_SOUND_SET,
} esp_commands;

typedef enum
{
	SOUND_HORN_TRIGGERED ,
	SOUND_EXCUSE_ME_TRIGGERED,
	SOUND_POLICE_TRIGGERED,
} soundTriggeredType;


//Determine the function of a certain bit
typedef enum
{
	EXT_DCDC_ENABLE_MASK_BIT,
	ENGINE_SOUND_ENABLE_MASK_BIT,
	START_UP_WARNING_ENABLE_MASK_BIT,
} float_enable_mask;


class   VescUart
{

 /**This data structure is used for engine sound */
  struct soundData
  { float pidOutput;
    float motorCurrent;
    uint8_t swState;
    float dutyCycle;
    float erpm;
    float inputVoltage;
  };


  struct advancedData
  {
  uint8_t adv_lights_mode;
	uint8_t adv_idle_warning_time;
	bool adv_engine_sound_enable;
	uint16_t adv_engine_sound_volume;
	uint8_t adv_over_speed_warning;
	bool adv_startup_safety_warning;
  };

  // Timeout - specifies how long the function will wait for the vesc to respond
  const uint32_t _TIMEOUT;

public:
  /**
   * @brief      Class constructor
   */
  VescUart(uint32_t timeout_ms = 100 );

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


/** get float data */
bool soundUpdate(void);
/**get advanced data */
bool advancedUpdate(void);
/**return value  */
uint8_t get_fw_version(void);
float get_pid_output(void);
float get_motor_current(void);
uint8_t get_switch_state(void);
float get_duty_cycle(void);
float get_erpm(void);
float get_input_voltage(void);
uint8_t get_sound_triggered(void);
//advanced data 

uint8_t get_adv_enable_data(void);
uint16_t get_adv_engine_sound_volume(void);
uint8_t get_adv_over_speed_warning(void);



private:
  /** Variabel to hold the reference to the Serial object to use for UART */
  Stream *serialPort = NULL;

  /** Variabel to hold the reference to the Serial object to use for debugging.
   * Uses the class Stream instead of HarwareSerial */
  Stream *debugPort = NULL;
  soundData sndData; 
  advancedData advData;
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
  bool processReadPacket(uint8_t *message , int lenPay);

  /**
   * @brief      Help Function to print uint8_t array over Serial for Debug
   *
   * @param      data  - Data array to print
   * @param      len   - Lenght of the array to print
   */
  void serialPrint(uint8_t *data, int len);

};

#endif
