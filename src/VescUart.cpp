#include <stdint.h>
#include "VescUart.h"

VescUart::VescUart(uint32_t timeout_ms) : _TIMEOUT(timeout_ms)
{ 
	//reset value 

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


bool VescUart::processReadPacket(uint8_t *message ,int lenPay )
{

	COMM_PACKET_ID packetId;
	int32_t index = 0;
	packetId = (COMM_PACKET_ID)message[0];

	if (debugPort != NULL)

	{
		debugPort->printf("message length:%d \n", lenPay);
	}
	message++; // Removes the packetId from the actual message (payload)

	switch (packetId)
	{

	case COMM_FW_VERSION:
		
		return true;
		break;
	case COMM_CUSTOM_APP_DATA:

		uint8_t magicNum, command;
		magicNum = (uint8_t)message[index++];
		command = (uint8_t)message[index++];
		if (lenPay< 2)
		{
			if (debugPort != NULL)
				debugPort->printf("Float App: Missing Args\n");
			return false;
		}

		if (magicNum != 101)

		{
			serialPort->printf(" Magic number wrong.");
			return false;
		}
		else
		{
			if (command == FLOAT_COMMAND_ENGINE_SOUND_INFO)
			{
				sndData.pidOutput = buffer_get_float32_auto(message, &index);
				sndData.motorCurrent = buffer_get_float32_auto(message, &index);
				sndData.floatState = (FloatState)message[index++];
				sndData.swState = (SwitchState)message[index++];
				sndData.dutyCycle = buffer_get_float32_auto(message, &index);
				sndData.erpm = buffer_get_float32_auto(message, &index);
				sndData.inputVoltage = buffer_get_float32_auto(message, &index);
				//sound triggered data
				sndData.sound_horn_triggered = (bool) message[index++];
				sndData.sound_excuse_me_trigger = (bool) message[index++];
				sndData.sound_police_triggered =(bool) message[index++];
				if (debugPort != NULL)
				{
					debugPort->printf(" Pid Value		:%.2f\r\n", sndData.pidOutput);
					debugPort->printf(" Motor Current	:%.2f\r\n", sndData.motorCurrent);
					debugPort->printf(" FLOAT State	:%d\r\n", (uint8_t)sndData.floatState);
					debugPort->printf(" Switch State	:%d\r\n", (uint8_t)sndData.swState);
					debugPort->printf(" Duty Cycle	:%.2f\r\n", sndData.dutyCycle);
					debugPort->printf(" ERPM			:%.2f\r\n", sndData.erpm);
					debugPort->printf(" Input Voltage	:%.2f\r\n", sndData.inputVoltage);
					debugPort->printf(" Sound horn triggered	:%s\r\n", sndData.sound_horn_triggered?"true":"false");
					debugPort->printf(" sound excuse me triggered :%s\r\n", sndData.sound_excuse_me_trigger?"true":"false");
					debugPort->printf(" sound police triggered	:%s\r\n", sndData.sound_police_triggered?"true":"false");
				}

				return true;
			}
			else if (command == FLOAT_COMMAND_GET_ADVANCED )
			{	advData.lights_mode=(FLOAT_LIGHT_MODE) message[index++];
				advData.idle_warning_time=(FLOAT_IDLE_TIME) message[index++];
				advData.engine_sound_enable=(bool)message[index++];
				advData.engine_sound_volume=buffer_get_uint16(message,&index);
				advData.over_speed_warning=(uint8_t) message[index++];
				advData.startup_safety_warning=(bool)message[index++];
				if (debugPort != NULL)
				{
					debugPort->printf("lights_modee		:%d\r\n", advData.lights_mode);
					debugPort->printf("idle_warning_time	:%d\r\n", advData.idle_warning_time);
					debugPort->printf("engine_sound_enable	:%s\r\n", advData.engine_sound_enable?"true":"false");
					debugPort->printf("advData.engine_sound_volume	:%d\r\n",advData.engine_sound_volume);
					debugPort->printf("advData.over_speed_warning	:%d\r\n", advData.over_speed_warning);
					debugPort->printf("advData.startup_safety_warning:%s\r\n", advData.startup_safety_warning?"true":"false");
				}

				return true;
			}
			else
			{
				debugPort->printf(" Unknow command !");
				return false;
			}
		}
	 break;
	default:
		debugPort->printf(" Unknow command !");
		return false;
		break;
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


bool VescUart::getSoundData(void)
{
	if (debugPort != NULL)
	{
		debugPort->println("Send Command:sound data" );
	}

	int32_t index = 0;
	int payloadSize =3;
	uint8_t payload[payloadSize];
	payload[index++] = {COMM_CUSTOM_APP_DATA};
	payload[index++] = 101; //surfdado's magic number
	payload[index++] = {FLOAT_COMMAND_ENGINE_SOUND_INFO};
	packSendPayload(payload, payloadSize);

	uint8_t message[256];
	int messageLength = receiveUartMessage(message);
	
	if (messageLength >=28)
	{
		return processReadPacket(message ,messageLength);
	}
	return false;

}


bool VescUart::getAdvancedData(void)
{
	if (debugPort != NULL)
	{
		debugPort->println("Send Command");
	}
	int32_t index = 0;
	int payloadSize = 3;
	uint8_t payload[payloadSize];
	payload[index++] = {COMM_CUSTOM_APP_DATA};
	payload[index++] = 101; //surfdado's magic number 
	payload[index++] = {FLOAT_COMMAND_GET_ADVANCED}; //float command 
	packSendPayload(payload, payloadSize);

	uint8_t message[256];
	int messageLength = receiveUartMessage(message);
	
	if (messageLength >=10)
	{
		return processReadPacket(message , messageLength);
	}
	return false;
}
bool VescUart::is_app_disable_output(void)
{
	if (debugPort != NULL)
	{
		debugPort->println("Send Command:sound data" );
	}
	int32_t index = 0;
	int payloadSize = 1;
	uint8_t payload[payloadSize];
	payload[index++] = {COMM_APP_DISABLE_OUTPUT};
	packSendPayload(payload, payloadSize);
	
	uint8_t message[10];
	COMM_PACKET_ID packetId;
	int32_t index = 0;
	int messageLength = receiveUartMessage(message);
	packetId = (COMM_PACKET_ID)message[0];
	
	if (debugPort != NULL)

	{
		debugPort->printf("message length:%d \n", messageLength);
	}

 debugPort->printf("message :%d \n", message[2]);
//  if (messageLength > 10)
//  {
// 		if (packetId == COMM_APP_DISABLE_OUTPUT)
// 		{
// 			debugPort->printf("message :%d \n", message[2]);
// 			return true;
// 		}
//  }
}
void VescUart::reset_sound_triggered( float_commands cmd )
{
	int32_t index = 0;
	int payloadSize = 3;
	uint8_t payload[payloadSize];
	payload[index++] = {COMM_CUSTOM_APP_DATA};
	payload[index++] = 101; // surfdado's magic number
	if (cmd >= 11)
	{
		payload[index++] = cmd;
		debugPort->printf("reset sound %d\r\n",cmd );
		packSendPayload(payload, payloadSize);
	}
	else
	{
		debugPort->println("command not meet !");
	}
	
}

bool VescUart::is_sound_horn_triggered(void)
{
	return sndData.sound_horn_triggered;

}

bool VescUart::is_sound_excuse_me_triggered(void)
{
return  sndData.sound_excuse_me_trigger;
}

bool VescUart::is_sound_police_triggered(void)
{
return sndData.sound_police_triggered;
}

bool VescUart::gte_engine_sound_enable(void)
{

return advData.engine_sound_enable;
}
bool VescUart::get_startup_safety_warning(void)
{
return advData.startup_safety_warning;
}
uint16_t VescUart::get_engine_sound_volume(void)
{

return advData.engine_sound_volume;
}
uint8_t VescUart::get_over_speed_warning(void)
{

return advData.over_speed_warning;
}
