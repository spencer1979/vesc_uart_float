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
	} else 
	{
		return 0;

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

		if (magicNum != ESP32_COMMAND_ID)

		{
			serialPort->printf(" Magic number wrong.\n");
			return false;
		}
		if (magicNum == ESP32_COMMAND_ID)
		{

			//
			switch (command)
			{
			case ESP_COMMAND_ENGINE_SOUND_INFO:
			{
				/**
					float pidOutput;
					uint8_t swState;
					float erpm;
					float inputVoltage;
					float motorCurrent

				 */
				engineData.pidOutput = buffer_get_float32_auto(message, &index);
				engineData.swState = (uint8_t)message[index++];
				engineData.erpm = buffer_get_float32_auto(message, &index);
				engineData.inputVoltage=buffer_get_float32_auto(message, &index);
				engineData.motorCurrent=buffer_get_float32_auto(message, &index);
				if (debugPort != NULL)
				{
					debugPort->printf(" Pid Value		:%.2f\n", engineData.pidOutput);
					debugPort->printf(" Switch State	:%d\n", (uint8_t)engineData.swState);
					debugPort->printf(" ERPM			:%.2f\n", engineData.erpm);
					debugPort->printf(" Input Voltage			:%.2f\n", engineData.inputVoltage);
					debugPort->printf(" motor current			:%.2f\n", engineData.motorCurrent);
				}

				return true;
			}

			case ESP_COMMAND_GET_ADV_INFO:

			{
				/**
				 *
				  uint8_t lights_mode;
					uint8_t idle_warning_time;
					uint16_t engine_sound_volume;
					uint8_t over_speed_warning;
  					float battery_level;
  					float low_battery_warning_level;
  					uint8_t engine_sampling_data;
				 */
				settingData.lights_mode = (uint8_t)message[index++];
				settingData.idle_warning_time = (uint8_t)message[index++];
				settingData.engine_sound_volume = buffer_get_uint16(message, &index);
				settingData.over_speed_warning = (uint8_t)message[index++];
				settingData.battery_level=buffer_get_float32_auto( message, &index);
				settingData.low_battery_warning_level=(float) message[index++];
				settingData.engine_sampling_data=(uint8_t)message[index++];
				if (debugPort != NULL)
				{
					debugPort->printf("lights_modee		:%d\n", settingData.lights_mode);
					debugPort->printf("idle_warning_time	:%d\n", settingData.idle_warning_time);
					debugPort->printf("engine sound volume	:%d\n", settingData.engine_sound_volume);
					debugPort->printf("settingData.over_speed_warning	:%d\r\n", settingData.over_speed_warning);
					debugPort->printf("settingData.battery level	:%.2f \r\n", settingData.battery_level);
					debugPort->printf("settingData.low_battery_warning_level	:%.2f \r\n", settingData.low_battery_warning_level);
					debugPort->printf("settingData.engine_sampling_data	:%d \r\n", settingData.engine_sampling_data);
					
					}

				return true;
			}

			case ESP_COMMAND_ENABLE_ITEM_INFO:
			{	
				uint8_t temp = (uint8_t)message[index++];
				if (enableItemData != temp )
				{
					enableItemData = temp;
				}

				if (debugPort != NULL)
				{
					debugPort->printf("Enable item data is : %d \n", enableItemData);
				}
				return true;
			}

			case ESP_COMMAND_SOUND_GET:
			{
				soundTriggered = (uint8_t)message[index++];
				if (debugPort != NULL)
				{
					debugPort->printf("sound triggered data is : %d \n", soundTriggered);
				}
				return true;
			}

			default:
			{
				debugPort->printf(" Unknow Float command !");
				return true;
			}
			}
		}
	}
	return false ;
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

bool VescUart::get_vesc_ready(void)
{
	
	uint8_t message[256];
	COMM_PACKET_ID packetId;
	int32_t index = 0;
	int payloadSize = 3;
	//send command to vesc 
	uint8_t payload[payloadSize];
	payload[index++] = {COMM_CUSTOM_APP_DATA};
	payload[index++] = ESP32_COMMAND_ID;
	payload[index++] = ESP_COMMAND_GET_READY;//FLOAT_COMMAND_GET_INFO
	packSendPayload(payload, payloadSize);

	//process received data 
	index = 0;
	int messageLength = receiveUartMessage(message);

	if (messageLength == 0)
	{
		return false;
	}

	if (debugPort != NULL)
		debugPort->printf("message Length :%d\r\n", messageLength);

	if (messageLength == 4)
	{
		packetId = (COMM_PACKET_ID)message[0];
		if (packetId == COMM_CUSTOM_APP_DATA)
		{
			if (message[1] == ESP32_COMMAND_ID && message[2] == ESP_COMMAND_GET_READY )
			{   
				
				if (debugPort != NULL)
					debugPort->printf("VESC ready ?: %s\n", (bool) message[3] ? "true":"false");
				return (bool) message[3];
			} 
		} 
	}

	if (debugPort != NULL)
		debugPort->printf("float version: 0\n");
	
	return false ;
	
}

bool VescUart::soundUpdate(void)
{
	if (debugPort != NULL)
	{
		debugPort->println("soundUpdate();");
	}

	int32_t index = 0;
	int payloadSize = 3;
	uint8_t payload[payloadSize];
	payload[index++] = {COMM_CUSTOM_APP_DATA};
	payload[index++] = ESP32_COMMAND_ID; // surfdado's magic number
	payload[index++] = {ESP_COMMAND_ENGINE_SOUND_INFO};
	packSendPayload(payload, payloadSize);

	uint8_t message[256];
	int messageLength = receiveUartMessage(message);
	if (debugPort != NULL)
		debugPort->printf("message Length :%d\r\n", messageLength);
	if (messageLength == 20 )
	{
		return processReadPacket(message, messageLength);
	}
	return false;
}

bool VescUart::advancedUpdate(void)
{
	if (debugPort != NULL)
	{
		debugPort->printf("Send Command\n");
	}
	int32_t index = 0;
	int payloadSize = 3;
	uint8_t payload[payloadSize];
	payload[index++] = {COMM_CUSTOM_APP_DATA};
	payload[index++] = ESP32_COMMAND_ID;							 // surfdado's magic number
	payload[index++] = {ESP_COMMAND_GET_ADV_INFO}; // float command
	packSendPayload(payload, payloadSize);

	uint8_t message[256];
	int messageLength = receiveUartMessage(message);
	if (debugPort != NULL)
		debugPort->printf("message Length :%d\r\n", messageLength);
	if (messageLength == 14 )
	{
		return processReadPacket(message, messageLength);
	}

	return false;
}


float VescUart::get_erpm(void)
{
	if (debugPort != NULL)
		debugPort->printf("Get Erpm\n");

return engineData.erpm;

}
float VescUart::get_battery_level(void)
{
	if (debugPort != NULL)
		debugPort->printf("Get battery_level %.2f \n", settingData.battery_level);

return settingData.battery_level;

}
float VescUart::get_low_battery_warning_level(void)
{
	if (debugPort != NULL)
		debugPort->printf("Get battery_level %.2f \n", settingData.low_battery_warning_level);

return settingData.low_battery_warning_level;

}
float VescUart::get_input_voltage(void)
{
	if (debugPort != NULL)
		debugPort->printf("Get input voltage %.2f \n", engineData.inputVoltage);

return engineData.inputVoltage;

}
float VescUart::get_pid_output(void){

	if (debugPort != NULL)
		debugPort->printf("Get pid output %.2f\n",engineData.pidOutput);
return engineData.pidOutput;

}
float VescUart::get_motor_current(void){

	if (debugPort != NULL)
		debugPort->printf("Get motor current %.2f\n",engineData.motorCurrent);
return engineData.motorCurrent;

}

uint8_t VescUart::get_switch_state(void)
{
	if (debugPort != NULL)
		debugPort->printf("Get float switch state :%d\n",engineData.swState);
	return engineData.swState;
}

//advanced data 

uint16_t VescUart::get_engine_sound_volume(void)
{
if (debugPort != NULL)
		debugPort->printf("get_adv_engine_sound_volume:%d \n" ,settingData.engine_sound_volume );

	return settingData.engine_sound_volume;
}
uint8_t VescUart::get_over_speed_value(void)
{

	if (debugPort != NULL)
		debugPort->printf("over_speed_value:%d\n" , settingData.over_speed_warning);

   return settingData.over_speed_warning;

}
uint8_t VescUart::get_idle_warning_time(void)
{

   if (debugPort != NULL)
		debugPort->printf("get_idle_warning_time :%d\n", settingData.idle_warning_time);

   return settingData.idle_warning_time;
}


uint8_t VescUart::get_engine_sampling(void)
{ if (debugPort != NULL)
		debugPort->printf("get_engine_sampling:%d\n", settingData.engine_sampling_data);

   return settingData.engine_sampling_data;

}
uint8_t VescUart::get_enable_item_data(void)
{
   if (debugPort != NULL)
		debugPort->printf("get_enable_item_data :\n");

   int32_t index = 0;
   int payloadSize = 3;
   uint8_t message[256];
   uint8_t payload[payloadSize];
   payload[index++] = {COMM_CUSTOM_APP_DATA};
   payload[index++] = ESP32_COMMAND_ID;
   payload[index++] = {ESP_COMMAND_ENABLE_ITEM_INFO}; // enable item data
   // write
   packSendPayload(payload, payloadSize);
   // read
   int messageLength = receiveUartMessage(message);
   if (debugPort != NULL)
		debugPort->printf("get message length is :%d\n", messageLength);
   if (messageLength ==4)
   {
		 processReadPacket(message, messageLength);
		
   }
   
   return enableItemData;
}

uint8_t VescUart::get_sound_triggered(void)
{
	if (debugPort != NULL)
		debugPort->println("get_sound_triggered");
		if (debugPort != NULL)
	{
		debugPort->printf("Send Command\n");
	}
	int32_t index = 0;
	int payloadSize = 3;
	uint8_t payload[payloadSize];
	payload[index++] = {COMM_CUSTOM_APP_DATA};
	payload[index++] = ESP32_COMMAND_ID;					
	payload[index++] = {ESP_COMMAND_SOUND_GET}; // get button triggered data 
	packSendPayload(payload, payloadSize);

	uint8_t message[256];
	int messageLength = receiveUartMessage(message);
	if (debugPort != NULL)
		debugPort->printf("get message length is :%d\n", messageLength);
	if (messageLength>=4)
	{
		if( processReadPacket(message, messageLength)  ) 
		return soundTriggered;
	}
	return 0;
}
