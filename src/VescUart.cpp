#include <stdint.h>
#include "VescUart.h"

VescUart::VescUart(uint32_t timeout_ms ) : _TIMEOUT(timeout_ms) 
{
}

void VescUart::setSerialPort(Stream *port)
{
	serialPort = port;
}

void VescUart::setDebugPort(Stream *port)
{
	debugPort = port;
}

int VescUart::receiveUartMessage(uint8_t *payloadReceived)
{

	// Messages <= 255 starts with "2", 2nd byte is length
	// Messages > 255 starts with "3" 2nd and 3rd byte is length combined with 1st >>8 and then &0xFF

	// Makes no sense to run this function if no serialPort is defined.
	if (serialPort == NULL)
		return -1;

	uint16_t counter = 0;
	uint16_t endMessage = 256;
	bool messageRead = false;
	uint8_t messageReceived[256];
	uint16_t lenPayload = 0;

	uint32_t timeout = millis() + _TIMEOUT; // Defining the timestamp for timeout (100ms before timeout)

	while (millis() < timeout && messageRead == false)
	{

		while (serialPort->available())
		{

			messageReceived[counter++] = serialPort->read();

			if (counter == 2)
			{

				switch (messageReceived[0])
				{
				case 2:
					endMessage = messageReceived[1] + 5; // Payload size + 2 for sice + 3 for SRC and End.
					lenPayload = messageReceived[1];
					break;

				case 3:
					// ToDo: Add Message Handling > 255 (starting with 3)
					if (debugPort != NULL)
					{
						debugPort->println("Message is larger than 256 bytes - not supported");
					}
					break;

				default:
					if (debugPort != NULL)
					{
						debugPort->println("Unvalid start bit");
					}
					break;
				}
			}

			if (counter >= sizeof(messageReceived))
			{
				break;
			}

			if (counter == endMessage && messageReceived[endMessage - 1] == 3)
			{
				messageReceived[endMessage] = 0;
				if (debugPort != NULL)
				{
					debugPort->println("End of message reached!");
				}
				messageRead = true;
				break; // Exit if end of message is reached, even if there is still more data in the buffer.
			}
		}
	}
	if (messageRead == false && debugPort != NULL)
	{
		debugPort->println("Timeout");
	}

	bool unpacked = false;

	if (messageRead)
	{
		unpacked = unpackPayload(messageReceived, endMessage, payloadReceived);
	}

	if (unpacked)
	{
		// Message was read
		return lenPayload;
	}
	else
	{
		// No Message Read
		return 0;
	}
}

bool VescUart::unpackPayload(uint8_t *message, int lenMes, uint8_t *payload)
{

	uint16_t crcMessage = 0;
	uint16_t crcPayload = 0;

	// Rebuild crc:
	crcMessage = message[lenMes - 3] << 8;
	crcMessage &= 0xFF00;
	crcMessage += message[lenMes - 2];

	if (debugPort != NULL)
	{
		debugPort->print("SRC received: ");
		debugPort->println(crcMessage);
	}

	// Extract payload:
	memcpy(payload, &message[2], message[1]);

	crcPayload = crc16(payload, message[1]);

	if (debugPort != NULL)
	{
		debugPort->print("SRC calc: ");
		debugPort->println(crcPayload);
	}

	if (crcPayload == crcMessage)
	{
		if (debugPort != NULL)
		{
			debugPort->print("Received: ");
			serialPrint(message, lenMes);
			debugPort->println();

			debugPort->print("Payload :      ");
			serialPrint(payload, message[1] - 1);
			debugPort->println();
		}

		return true;
	}
	else
	{
		return false;
	}
}

int VescUart::packSendPayload(uint8_t *payload, int lenPay)
{

	uint16_t crcPayload = crc16(payload, lenPay);
	int count = 0;
	uint8_t messageSend[256];

	if (lenPay <= 256)
	{
		messageSend[count++] = 2;
		messageSend[count++] = lenPay;
	}
	else
	{
		messageSend[count++] = 3;
		messageSend[count++] = (uint8_t)(lenPay >> 8);
		messageSend[count++] = (uint8_t)(lenPay & 0xFF);
	}

	memcpy(messageSend + count, payload, lenPay);
	count += lenPay;

	messageSend[count++] = (uint8_t)(crcPayload >> 8);
	messageSend[count++] = (uint8_t)(crcPayload & 0xFF);
	messageSend[count++] = 3;
	// messageSend[count] = NULL;

	if (debugPort != NULL)
	{
		debugPort->print("Package to send: ");
		serialPrint(messageSend, count);
	}

	// Sending package
	if (serialPort != NULL)
		serialPort->write(messageSend, count);

	// Returns number of send bytes
	return count;
}

bool VescUart::processReadPacket(uint8_t *message, int lenPay)
{

	COMM_PACKET_ID packetId;
	int32_t index = 0;
	packetId = (COMM_PACKET_ID)message[0];

	if (debugPort != NULL)

	{
		debugPort->printf("message length:%d \n", lenPay);
	}
	message++; // Removes the packetId from the actual message (payload)

	if (packetId == COMM_CUSTOM_APP_DATA)
	{
		uint8_t magicNum, command;
		magicNum = (uint8_t)message[index++];
		command = (uint8_t)message[index++];
		if (lenPay < 2)
		{
			if (debugPort != NULL)
				debugPort->printf("Float App: Missing Args\n");
			return false;
		}

		if (magicNum != 102)

		{
			serialPort->printf(" Magic number wrong.");
			return false;
		}

		//
		switch (command)
		{
		case ESP_COMMAND_ENGINE_SOUND_INFO:
		{
			sndData.pidOutput = buffer_get_float32_auto(message, &index);
			sndData.motorCurrent = buffer_get_float32_auto(message, &index);
			sndData.swState = (uint8_t)message[index++];
			sndData.dutyCycle = buffer_get_float32_auto(message, &index);
			sndData.erpm = buffer_get_float32_auto(message, &index);
			sndData.inputVoltage = buffer_get_float32_auto(message, &index);

			if (debugPort != NULL)
			{
				debugPort->printf(" Pid Value		:%.2f\r\n", sndData.pidOutput);
				debugPort->printf(" Motor Current	:%.2f\r\n", sndData.motorCurrent);
				debugPort->printf(" Switch State	:%d\r\n", (uint8_t)sndData.swState);
				debugPort->printf(" Duty Cycle	:%.2f\r\n", sndData.dutyCycle);
				debugPort->printf(" ERPM			:%.2f\r\n", sndData.erpm);
				debugPort->printf(" Input Voltage	:%.2f\r\n", sndData.inputVoltage);
			}

			return true;
		}

		case ESP_COMMAND_GET_ADVANCED:

		{
			advData.adv_lights_mode = (uint8_t)message[index++];
			advData.adv_idle_warning_time = (uint8_t)message[index++];
			advData.adv_engine_sound_enable = (bool)message[index++];
			advData.adv_engine_sound_volume = buffer_get_uint16(message, &index);
			advData.adv_over_speed_warning = (uint8_t)message[index++];
			advData.adv_startup_safety_warning = (bool)message[index++];
			if (debugPort != NULL)
			{
				debugPort->printf("lights_modee		:%d\r\n", advData.adv_lights_mode);
				debugPort->printf("idle_warning_time	:%d\r\n", advData.adv_idle_warning_time);
				debugPort->printf("engine_sound_enable	:%s\r\n", advData.adv_engine_sound_enable ? "true" : "false");
				debugPort->printf("advData.engine_sound_volume	:%d\r\n", advData.adv_engine_sound_volume);
				debugPort->printf("advData.over_speed_warning	:%d\r\n", advData.adv_over_speed_warning);
				debugPort->printf("advData.startup_safety_warning:%s\r\n", advData.adv_startup_safety_warning ? "true" : "false");
			}

			return true;
		}

			// single
		case ESP_COMMAND_GET_DUTYCYCLE:
		{
			sndData.dutyCycle = buffer_get_float32_auto(message, &index);
			return true;
		}
		case ESP_COMMAND_GET_ERPM:
		{
			sndData.erpm = buffer_get_float32_auto(message, &index);
			return true;
		}
		case ESP_COMMAND_GET_PID_OUTPUT:
		{
			sndData.pidOutput = buffer_get_float32_auto(message, &index);
			return true;
		}
		case ESP_COMMAND_GET_SWITCH_STATE:
		{
			sndData.swState = (uint8_t)message[index++];
			return true;
		}
		case ESP_COMMAND_GET_MOTOR_CURRENT:
		{
			sndData.motorCurrent = buffer_get_float32_auto(message, &index);
			return true;
		}

		case ESP_COMMAND_GET_INPUT_VOLTAGE:
		{
			sndData.inputVoltage = buffer_get_float32_auto(message, &index);
			return true;
		}

		// advanced
		case ESP_COMMAND_GET_LIGHT_MODE:
		{
			advData.adv_lights_mode = (uint8_t)message[index++];
			return true;
		}
		case ESP_COMMAND_GET_IDLE_WARN_TIME:
		{
			advData.adv_idle_warning_time = (uint8_t)message[index++];
			return true;
		}
		case ESP_COMMAND_GET_ENGINE_SOUND_VOLUME:
		{
			advData.adv_engine_sound_volume = buffer_get_uint16(message, &index);
			return true;
		}
		case ESP_COMMAND_GET_ENGIEN_SOUND_ENABLE:
		{
			advData.adv_engine_sound_enable = (bool)message[index++];
			return true;
		}
		case ESP_COMMAND_GET_OVER_SPEED_WARN:
		{
			advData.adv_over_speed_warning = (uint8_t)message[index++];
			return true;
		}
		case ESP_COMMAND_GET_START_UP_WARN:
		{
			advData.adv_startup_safety_warning = (bool)message[index++];
			return true;
		}

		default:
		{
			debugPort->printf(" Unknow Float command !");
			return false;
		}
		}
	}
}

void VescUart::serialPrint(uint8_t *data, int len)
{
	if (debugPort != NULL)
	{
		for (int i = 0; i <= len; i++)
		{
			debugPort->print(data[i]);
			debugPort->print(" ");
		}
		debugPort->println("");
	}
}

uint8_t VescUart::get_fw_version(void)
{
	
	uint8_t message[10];
	COMM_PACKET_ID packetId;
	int32_t index = 0;
	int payloadSize = 3;
	//send command to vesc 
	uint8_t payload[payloadSize];
	payload[index++] = {COMM_CUSTOM_APP_DATA};
	payload[index++] = 101;
	payload[index++] = 0;//FLOAT_COMMAND_GET_INFO
	packSendPayload(payload, payloadSize);

	//process received data 
	index = 0;
	int messageLength = receiveUartMessage(message);
	if (debugPort != NULL)
		debugPort->printf("message Length :%d\r\n", messageLength);

	if (messageLength >= 3)
	{
		packetId = (COMM_PACKET_ID)message[0];
		if (packetId == COMM_CUSTOM_APP_DATA)
		{
			if (message[1] == 101 && message[2] == 0 )
			{   
				
				if (debugPort != NULL)
					debugPort->printf("float version: %d\n", (uint8_t) message[3]);
				return (uint8_t) message[3];
			} 
		} 
	}

	if (debugPort != NULL)
		debugPort->printf("float version: 0\n");
	return 0 ;
	
}

bool VescUart::soundUpdate(void)
{
	if (debugPort != NULL)
	{
		debugPort->println("Send Command:sound data");
	}

	int32_t index = 0;
	int payloadSize = 3;
	uint8_t payload[payloadSize];
	payload[index++] = {COMM_CUSTOM_APP_DATA};
	payload[index++] = 102; // surfdado's magic number
	payload[index++] = {ESP_COMMAND_ENGINE_SOUND_INFO};
	packSendPayload(payload, payloadSize);

	uint8_t message[50];
	int messageLength = receiveUartMessage(message);
	if (debugPort != NULL)
		debugPort->printf("message Length :%d\r\n", messageLength);
	if (messageLength >= 27)
	{
		return processReadPacket(message, messageLength);
	}
	return false;
}

bool VescUart::advancedUpdate(void)
{
	if (debugPort != NULL)
	{
		debugPort->println("Send Command");
	}
	int32_t index = 0;
	int payloadSize = 3;
	uint8_t payload[payloadSize];
	payload[index++] = {COMM_CUSTOM_APP_DATA};
	payload[index++] = 102;							 // surfdado's magic number
	payload[index++] = {ESP_COMMAND_GET_ADVANCED}; // float command
	packSendPayload(payload, payloadSize);

	uint8_t message[256];
	int messageLength = receiveUartMessage(message);
	if (debugPort != NULL)
		debugPort->printf("message Length :%d\r\n", messageLength);
	if (messageLength >= 7)
	{
		return processReadPacket(message, messageLength);
	}

	return false;
}

uint8_t VescUart::get_sound_triggered(void)
{
	if (debugPort != NULL)
		debugPort->println("get_sound_triggered");

	uint8_t message[10];
	COMM_PACKET_ID packetId;
	int32_t index = 0;
	int payloadSize = 3;
	//send command to vesc 
	uint8_t payload[payloadSize];
	payload[index++] = {COMM_CUSTOM_APP_DATA};
	payload[index++] = 102;
	payload[index++] = {ESP_COMMAND_SOUND_GET};
	packSendPayload(payload, payloadSize);

	//process received data 
	index = 0;
	int messageLength = receiveUartMessage(message);
	if (debugPort != NULL)
		debugPort->printf("message Length :%d\r\n", messageLength);

	if (messageLength >= 3)
	{
		packetId = (COMM_PACKET_ID)message[0];
		if (packetId == COMM_CUSTOM_APP_DATA)
		{
			if (message[1] == 102 && message[2] == ESP_COMMAND_SOUND_GET)
			{   
				return (uint8_t) message[3];
			}
		} 
	}

	return 0 ;
}

float VescUart::get_pid_output(void)
{
	if (debugPort != NULL)
		debugPort->println("Get pid output");

	int32_t index = 0;
	int payloadSize = 3;
	uint8_t payload[payloadSize];
	payload[index++] = {COMM_CUSTOM_APP_DATA};
	payload[index++] = 102;
	payload[index++] = {ESP_COMMAND_GET_PID_OUTPUT};
	packSendPayload(payload, payloadSize);

	uint8_t message[10];
	int messageLength = receiveUartMessage(message);
	if (debugPort != NULL)
		debugPort->printf("message Length :%d\r\n", messageLength);

	if (messageLength >= 6)
		return processReadPacket(message, messageLength) ? sndData.pidOutput : 0.0;

	return 0.0;
}

float VescUart::get_motor_current(void)
{
	if (debugPort != NULL)
		debugPort->println("Get motor current");

	int32_t index = 0;
	int payloadSize = 3;
	uint8_t payload[payloadSize];
	payload[index++] = {COMM_CUSTOM_APP_DATA};
	payload[index++] = 102;
	payload[index++] = {ESP_COMMAND_GET_MOTOR_CURRENT};
	packSendPayload(payload, payloadSize);

	uint8_t message[10];
	int messageLength = receiveUartMessage(message);
	if (debugPort != NULL)
		debugPort->printf("message Length :%d\r\n", messageLength);

	if (messageLength >= 6)
		return processReadPacket(message, messageLength) ? sndData.motorCurrent : 0.0;

	return 0.0;
}

uint8_t VescUart::get_switch_state(void)
{
	if (debugPort != NULL)
		debugPort->println("Get float switch state");

	int32_t index = 0;
	int payloadSize = 3;
	uint8_t payload[payloadSize];
	payload[index++] = {COMM_CUSTOM_APP_DATA};
	payload[index++] = 102;
	payload[index++] = {ESP_COMMAND_GET_SWITCH_STATE};
	packSendPayload(payload, payloadSize);

	uint8_t message[10];
	int messageLength = receiveUartMessage(message);
	if (debugPort != NULL)
		debugPort->printf("message Length :%d\r\n", messageLength);

	if (messageLength >= 3)
		return processReadPacket(message, messageLength) ? sndData.swState : 0;

	return 0;
}
float VescUart::get_duty_cycle(void)
{
	if (debugPort != NULL)
		debugPort->println("Get duty cycle");

	int32_t index = 0;
	int payloadSize = 3;
	uint8_t payload[payloadSize];
	payload[index++] = {COMM_CUSTOM_APP_DATA};
	payload[index++] = 102;
	payload[index++] = {ESP_COMMAND_GET_DUTYCYCLE};
	packSendPayload(payload, payloadSize);

	uint8_t message[10];
	int messageLength = receiveUartMessage(message);
	if (debugPort != NULL)
		debugPort->printf("message Length :%d\r\n", messageLength);

	if (messageLength >= 6)
		return processReadPacket(message, messageLength) ? sndData.dutyCycle : 0.0;

	return 0.0;
}
float VescUart::get_erpm(void)
{
	if (debugPort != NULL)
		debugPort->println("Get Erpm");

	int32_t index = 0;
	int payloadSize = 3;
	uint8_t payload[payloadSize];
	payload[index++] = {COMM_CUSTOM_APP_DATA};
	payload[index++] = 102;
	payload[index++] = {ESP_COMMAND_GET_ERPM};
	packSendPayload(payload, payloadSize);

	uint8_t message[10];
	int messageLength = receiveUartMessage(message);
	if (debugPort != NULL)
		debugPort->printf("message Length :%d\r\n", messageLength);

	if (messageLength >= 6)
		return processReadPacket(message, messageLength) ? sndData.erpm : 0.0;

	return 0.0;
}

float VescUart::get_input_voltage(void)
{
	if (debugPort != NULL)
		debugPort->println("Get input voltage");

	int32_t index = 0;
	int payloadSize = 3;
	uint8_t payload[payloadSize];
	payload[index++] = {COMM_CUSTOM_APP_DATA};
	payload[index++] = 102;
	payload[index++] = {ESP_COMMAND_GET_INPUT_VOLTAGE};
	packSendPayload(payload, payloadSize);

	uint8_t message[10];
	int messageLength = receiveUartMessage(message);
	if (debugPort != NULL)
		debugPort->printf("message Length :%d\r\n", messageLength);

	if (messageLength >= 6)
		return processReadPacket(message, messageLength) ? sndData.inputVoltage : -1;

	return -1;
}
// advanced data
bool VescUart::get_adv_engine_sound_enable(void)
{
if (debugPort != NULL)
		debugPort->println("Get advanced engine sound enable");

	int32_t index = 0;
	int payloadSize = 3;
	uint8_t payload[payloadSize];
	payload[index++] = {COMM_CUSTOM_APP_DATA};
	payload[index++] = 102;
	payload[index++] = {ESP_COMMAND_GET_ENGIEN_SOUND_ENABLE};
	packSendPayload(payload, payloadSize);

	uint8_t message[10];
	int messageLength = receiveUartMessage(message);
	if (debugPort != NULL)
		debugPort->printf("message Length :%d\r\n", messageLength);

	if (messageLength >= 4)
		return processReadPacket(message, messageLength) ? advData.adv_engine_sound_enable : false;

	return false;
}
bool VescUart::get_adv_startup_safety_warning(void)
{
	if (debugPort != NULL)
		debugPort->println("get_adv_startup_safety_warning");

	int32_t index = 0;
	int payloadSize = 3;
	uint8_t payload[payloadSize];
	payload[index++] = {COMM_CUSTOM_APP_DATA};
	payload[index++] = 102;
	payload[index++] = {ESP_COMMAND_GET_START_UP_WARN};
	packSendPayload(payload, payloadSize);

	uint8_t message[10];
	int messageLength = receiveUartMessage(message);
	if (debugPort != NULL)
		debugPort->printf("message Length :%d\r\n", messageLength);

	if (messageLength >= 4)
		return processReadPacket(message, messageLength) ? advData.adv_startup_safety_warning: false;

	return false;
}
uint16_t VescUart::get_adv_engine_sound_volume(void)
{

	if (debugPort != NULL)
		debugPort->println("get_adv_engine_sound_volume");

	int32_t index = 0;
	int payloadSize = 3;
	uint8_t payload[payloadSize];
	payload[index++] = {COMM_CUSTOM_APP_DATA};
	payload[index++] = 102;
	payload[index++] = {ESP_COMMAND_GET_ENGINE_SOUND_VOLUME};
	packSendPayload(payload, payloadSize);

	uint8_t message[10];
	int messageLength = receiveUartMessage(message);
	if (debugPort != NULL)
		debugPort->printf("message Length :%d\r\n", messageLength);

	if (messageLength >= 5)
		return processReadPacket(message, messageLength) ? advData.adv_engine_sound_volume: 100;

	return 100;
}
uint8_t VescUart::get_adv_over_speed_warning(void)
{

	if (debugPort != NULL)
		debugPort->println("get_adv_over_speed_warning");

	int32_t index = 0;
	int payloadSize = 3;
	uint8_t payload[payloadSize];
	payload[index++] = {COMM_CUSTOM_APP_DATA};
	payload[index++] = 102;
	payload[index++] = {ESP_COMMAND_GET_OVER_SPEED_WARN};
	packSendPayload(payload, payloadSize);

	uint8_t message[10];
	int messageLength = receiveUartMessage(message);
	if (debugPort != NULL)
		debugPort->printf("message Length :%d\r\n", messageLength);

	if (messageLength >= 3)
		return processReadPacket(message, messageLength) ? advData.adv_over_speed_warning: 0;

	return 0;
}
