#ifndef _VESCUART_h
#define _VESCUART_h

#include <Arduino.h>
#include "datatypes.h"
#include "buffer.h"
#include "crc.h"
typedef enum
{
  SOUND_HORN,
  SOUND_EXCUSE_ME,
  SOUND_POLICE,
} soundType;
    /**idle warning time */
typedef enum
{
  FLOAT_IDLE_WARNING_TIME_DISABLE = 0,
  FLOAT_IDLE_WARNING_TIME_1M,
  FLOAT_IDLE_WARNING_TIME_5M,
  FLOAT_IDLE_WARNING_TIME_10M,
  FLOAT_IDLE_WARNING_TIME_30M,
  FLOAT_IDLE_WARNING_TIME_60M,
  FLOAT_IDLE_WARNING_TIME_120M

} FLOAT_IDLE_TIME;

/** float command  */
typedef enum
{
	FLOAT_COMMAND_GET_INFO = 0,		 // get version / package info
	FLOAT_COMMAND_GET_RTDATA = 1,	 // get rt data
	FLOAT_COMMAND_RT_TUNE = 2,		 // runtime tuning (don't write to eeprom)
	FLOAT_COMMAND_TUNE_DEFAULTS = 3, // set tune to defaults (no eeprom)
	FLOAT_COMMAND_CFG_SAVE = 4,		 // save config to eeprom
	FLOAT_COMMAND_CFG_RESTORE = 5,	 // restore config from eeprom

	FLOAT_COMMAND_GET_ADVANCED=6,  // get ADVANCED setting only for SPESC 
	FLOAT_COMMAND_ENGINE_SOUND_INFO=7, // engine sound info , erpm ,duty 
  //sound triggered 
	FLOAT_COMMAND_HORN_TRIGGERED=8,
	FLOAT_COMMAND_EXCUSE_ME_TRIGGERED=9,
	FLOAT_COMMAND_POLICE_TRIGGERED=10,
  //
  FLOAT_COMMAND_HORN_RESET=11,
	FLOAT_COMMAND_EXCUSE_ME_RESET=12,
	FLOAT_COMMAND_POLICE_RESET=13,
} float_commands;

// float data type
typedef enum
{
	STARTUP = 0,
	RUNNING = 1,
	RUNNING_TILTBACK = 2,
	RUNNING_WHEELSLIP = 3,
	RUNNING_UPSIDEDOWN = 4,
	FAULT_ANGLE_PITCH = 5,
	FAULT_ANGLE_ROLL = 6,
	FAULT_SWITCH_HALF = 7,
	FAULT_SWITCH_FULL = 8,
	FAULT_STARTUP = 9,
	FAULT_REVERSE = 10,
	FAULT_QUICKSTOP = 11
} FloatState;

typedef enum
{
	OFF = 0,
	HALF,
	ON
} SwitchState;

/**light mode*/
typedef enum
{
	FLOAT_LIGHT_OFF = 0,
	FLOAT_LIGHT_FLASH,
	FLOAT_LIGHT_FULL_ON
} FLOAT_LIGHT_MODE;

// audio source
// using VESC controll id to get source
typedef enum
{
  AUDIO_SOURCE_CSR,
  AUDIO_SOURCE_ESP32,
} AudioSource;


class   VescUart
{ 
 /**This data structure is used for engine sound */
  struct soundData
  { float pidOutput;
    float motorCurrent;
    FloatState floatState;
    SwitchState swState;
    float dutyCycle;
    float erpm;
    float inputVoltage;
    bool sound_horn_triggered=false;
    bool sound_excuse_me_trigger=false;
    bool sound_police_triggered=false;
  };

  struct advancedData
  {
  FLOAT_LIGHT_MODE lights_mode;
	FLOAT_IDLE_TIME idle_warning_time;
	bool engine_sound_enable;
	uint16_t engine_sound_volume;
	uint8_t over_speed_warning;
	bool startup_safety_warning;
  };

  // Timeout - specifies how long the function will wait for the vesc to respond
  const uint32_t _TIMEOUT;

public:
  /**
   * @brief      Class constructor
   */
  VescUart(uint32_t timeout_ms = 150);

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
bool getSoundData(void);

/**get advanced data */
bool getAdvancedData(void);
/**check app is disable */
bool is_app_disable_output(void);

void reset_sound_triggered( float_commands cmd);

bool is_sound_horn_triggered(void);
bool is_sound_excuse_me_triggered(void);
bool is_sound_police_triggered(void);

bool gte_engine_sound_enable(void);
bool get_startup_safety_warning(void);
uint16_t get_engine_sound_volume(void);
uint8_t get_over_speed_warning(void);


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
