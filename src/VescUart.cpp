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


bool VescUart::processReadPacket(uint8_t *message)
{

	COMM_PACKET_ID packetId;
	int32_t index = 0;

	packetId = (COMM_PACKET_ID)message[0];
	
	message++; // Removes the packetId from the actual message (payload)

	switch (packetId)
	{

	case COMM_CUSTOM_APP_DATA:
		uint8_t magicNum, command;
		magicNum = (uint8_t)message[index++];
		command = (uint8_t)message[index++];

		if (magicNum != 101)

		{
			serialPort->printf(" Magic number wrong.");
			return false;
			break;
		}
		else
		{
			switch (command)
			{
			case FLOAT_COMMAND_ENGINE_SOUND_INFO:
				sndData.pidOutput = buffer_get_float32_auto(message, &index);
				sndData.motorCurrent = buffer_get_float32_auto(message, &index);
				sndData.floatState = (FloatState)buffer_get_uint16(message, &index);
				sndData.swState = (SwitchState)buffer_get_uint16(message, &index);
				sndData.dutyCycle = buffer_get_float32_auto(message, &index);
				sndData.erpm = buffer_get_float32_auto(message, &index);
				sndData.inputVoltage = buffer_get_float32_auto(message, &index);
				if (debugPort != NULL)
					debugPort->printf(" Pid Value		:%.2f\r\n", sndData.pidOutput);
				debugPort->printf(" Motor Current	:%.2f\r\n", sndData.motorCurrent);
				debugPort->printf(" FLOAT State	:%d\r\n", (uint8_t)sndData.floatState);
				debugPort->printf(" Switch State	:%d\r\n", (uint8_t)sndData.swState);
				debugPort->printf(" Duty Cycle	:%.2f\r\n", sndData.dutyCycle);
				debugPort->printf(" ERPM			:%.2f\r\n", sndData.erpm);
				debugPort->printf(" Input Voltage	:%.2f\r\n", sndData.inputVoltage);
			}
			return true;
			break;
		case FLOAT_COMMAND_GET_ADAVANCE:
			/* code */
			return true;
			break;

		default:
			debugPort->printf(" Unknow command !");
			return false;
			break;
		}
	
	break;


	case FLOAT_COMMAND_ENGINE_SOUND_INFO :

	
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


bool VescUart::getSoundData()
{
	if (debugPort != NULL)
	{
		debugPort->println("Send Command:" + String(FLOAT_COMMAND_GET_INFO) +"sound data" );
	}

	int32_t index = 0;
	int payloadSize =3;
	uint8_t payload[payloadSize];
	payload[index++] = {COMM_CUSTOM_APP_DATA};
	payload[index++] = 101; //surfdado's magic number
	payload[index++] = {FLOAT_COMMAND_ENGINE_SOUND_INFO};
	//payload[index++] = 10; //surfdado's magic number 
	packSendPayload(payload, payloadSize);

	uint8_t message[256];
	int messageLength = receiveUartMessage(message);
	
	if (messageLength >0)
	{
		return processReadPacket(message);
	}
	return false;

}


bool VescUart::getAdvancedData()
{
	if (debugPort != NULL)
	{
		debugPort->println("Send Command:" + String(FLOAT_COMMAND_GET_ADAVANCE) +"for advanced data");
	}
	int32_t index = 0;
	int payloadSize = 3;
	uint8_t payload[payloadSize];
	payload[index++] = {COMM_CUSTOM_APP_DATA};
	payload[index++] = 101; //surfdado's magic number 
	payload[index++] = {FLOAT_COMMAND_GET_ADAVANCE}; //float command 
	packSendPayload(payload, payloadSize);

	uint8_t message[256];
	int messageLength = receiveUartMessage(message);
	
	if (messageLength >34)
	{
		return processReadPacket(message);
	}
	return false;
}