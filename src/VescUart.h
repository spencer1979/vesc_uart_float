#ifndef _VESCUART_h
#define _VESCUART_h

#include <Arduino.h>
#include "datatypes.h"
#include "buffer.h"
#include "crc.h"

typedef enum {
	FLOAT_COMMAND_GET_INFO = 0,		// get version / package info
	FLOAT_COMMAND_GET_RTDATA = 1,	// get rt data
	FLOAT_COMMAND_RT_TUNE = 2,		// runtime tuning (don't write to eeprom)
	FLOAT_COMMAND_TUNE_DEFAULTS = 3,// set tune to defaults (no eeprom)
	FLOAT_COMMAND_CFG_SAVE = 4,		// save config to eeprom
	FLOAT_COMMAND_CFG_RESTORE = 5,	// restore config from eeprom
	FLOAT_COMMAND_TUNE_OTHER = 6,	// make runtime changes to startup/etc
	FLOAT_COMMAND_RC_MOVE = 7,		// move motor while board is idle

	FLOAT_COMMAND_GET_ADVANCED,  // get ADVANCED setting only for SPESC 
	FLOAT_COMMAND_ENGINE_SOUND_INFO, // engine sound info , erpm ,duty 
	//single
    FLOAT_COMMAND_GET_DUTYCYCLE,
	FLOAT_COMMAND_GET_ERPM,
	FLOAT_COMMAND_GET_PID_OUTPUT,
	FLOAT_COMMAND_GET_SWITCH_STATE,
	FLOAT_COMMAND_GET_MOTOR_CURRENT,
	FLOAT_COMMAND_GET_INPUT_VOLTAGE,
	//advanced 
	FLOAT_COMMAND_GET_LIGHT_MODE,
	FLOAT_COMMAND_GET_IDLE_WARN_TIME,
	FLOAT_COMMAND_GET_ENGINE_SOUND_VOLUME,
	FLOAT_COMMAND_GET_ENGIEN_SOUND_ENABLE,
	FLOAT_COMMAND_GET_OVER_SPEED_WARN,
	FLOAT_COMMAND_GET_START_UP_WARN,
	//button 
	//esp32 get 
	FLOAT_COMMAND_HORN_TRIGGERED,
	FLOAT_COMMAND_EXCUSE_ME_TRIGGERED,
	FLOAT_COMMAND_POLICE_TRIGGERED,
	//esp32 reset 
	FLOAT_COMMAND_HORN_RESET,
	FLOAT_COMMAND_EXCUSE_ME_RESET,
	FLOAT_COMMAND_POLICE_RESET,
	//app set true 
	FLOAT_COMMAND_HORN_SET,
	FLOAT_COMMAND_EXCUSE_ME_SET,
	FLOAT_COMMAND_POLICE_SET,

} float_commands;

class   VescUart
{
  struct fw_version
  {
    uint8_t fw_version_major, fw_version_minor;
  };

  struct trigger_sound
  {
    bool sound_horn_triggered=false;
    bool sound_excuse_me_trigger=false;
    bool sound_police_triggered=false;
  };

 /**This data structure is used for engine sound */
  struct soundData
  { float pidOutput;
    float motorCurrent;
    uint8_t swState;
    float dutyCycle;
    float erpm;
    float inputVoltage;
    fw_version fw;
    trigger_sound triggerSound;
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
void reset_sound_triggered( float_commands cmd);
bool is_sound_horn_triggered(void);
bool is_sound_excuse_me_triggered(void);
bool is_sound_police_triggered(void);
//advanced data 
bool get_adv_engine_sound_enable(void);
bool get_adv_startup_safety_warning(void);
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
