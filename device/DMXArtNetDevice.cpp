/*
  ==============================================================================

	DMXArtNetDevice.cpp
	Created: 10 Apr 2017 12:44:42pm
	Author:  Ben

  ==============================================================================
*/

#include "JuceHeader.h"
#include "DMXArtNetDevice.h"

DMXArtNetDevice::DMXArtNetDevice(bool enableReceive) :
	DMXDevice("ArtNet", ARTNET, enableReceive),
	Thread("ArtNetReceive"),
	sender(true),
	localPort(nullptr)
{

	if (enableReceive)
	{
		localPort = inputCC->addIntParameter("Local Port", "Local port to receive ArtNet data. This needs to be enabled in order to receive data", 6454, 0, 65535);
		//inputNet = inputCC->addIntParameter("Net", "The net to receive from, from 0 to 15", 0, 0, 127);
		//inputSubnet = inputCC->addIntParameter("Subnet", "The subnet to receive from, from 0 to 15", 0, 0, 15);
		//inputUniverse = inputCC->addIntParameter("Universe", "The Universe to receive from, from 0 to 15", 0, 0, 15);
	}

	remoteHost = outputCC->addStringParameter("Remote Host", "IP to which send the Art-Net to", "127.0.0.1");
	remotePort = outputCC->addIntParameter("Remote Port", "Local port to receive ArtNet data", 6454, 0, 65535);
	//outputNet = outputCC->addIntParameter("Net", "The net to send to, from 0 to 15", 0, 0, 127);
	//outputSubnet = outputCC->addIntParameter("Subnet", "The subnet to send to, from 0 to 15", 0, 0, 15);
	//outputUniverse = outputCC->addIntParameter("Universe", "The Universe to send to, from 0 to 15", 0, 0, 15);


	memset(receiveBuffer, 0, MAX_PACKET_LENGTH);
	memset(artnetPacket + DMX_HEADER_LENGTH, 0, DMX_NUM_CHANNELS);

	sender.bindToPort(0);

	updateConnectedParam();
	setupReceiver();
}

DMXArtNetDevice::~DMXArtNetDevice()
{
	//if (Engine::mainEngine != nullptr) Engine::mainEngine->removeEngineListener(this);
	if (receiver != nullptr) receiver->shutdown();
	sender.shutdown();
	stopThread(1000);
}

void DMXArtNetDevice::refreshEnabled()
{
	setupReceiver();
}

void DMXArtNetDevice::setupReceiver()
{
	stopThread(500);
	if (receiver != nullptr) receiver->shutdown();

	if (inputCC == nullptr || !inputCC->enabled->boolValue())
	{
		clearWarning();
		return;
	}

	if (isConnected != nullptr) isConnected->setValue(false);

	receiver.reset(new DatagramSocket());
	receiver->setEnablePortReuse(true);
	bool result = receiver->bindToPort(localPort->intValue());
	if (result)
	{
		//receiver->setEnablePortReuse(false);
		clearWarning();
		NLOG(niceName, "Listening for ArtNet on port " << localPort->intValue());
	}
	else
	{
		setWarningMessage("Error binding port " + localPort->stringValue() + ", is it already taken ?");
		return;
	}

	startThread();
	isConnected->setValue(true);
}

//void DMXArtNetDevice::sendDMXValue(int channel, int value)
//{
//	artnetPacket[channel - 1 + DMX_HEADER_LENGTH] = (uint8)value;
//	DMXDevice::sendDMXValue(channel, value);
//}
//
//void DMXArtNetDevice::sendDMXRange(int startChannel, Array<int> values)
//{
//	int numValues = values.size();
//	for (int i = 0; i < numValues; ++i)
//	{
//		int channel = startChannel + i;
//		if (channel < 0) continue;
//		if (channel > 512) break;
//
//		artnetPacket[channel - 1 + DMX_HEADER_LENGTH] = (uint8)(values[i]);
//	}
//
//
//	DMXDevice::sendDMXRange(startChannel, values);
//
//}

void DMXArtNetDevice::sendDMXValuesInternal(int net, int subnet, int universe, uint8* values, int numChannels)
{
	sequenceNumber = (sequenceNumber + 1) % 256;

	artnetPacket[12] = sequenceNumber;
	artnetPacket[13] = 0;
	artnetPacket[14] = (subnet << 4) | universe;
	artnetPacket[15] = net;
	artnetPacket[16] = 2;
	artnetPacket[17] = 0;

	memcpy(artnetPacket + DMX_HEADER_LENGTH, values, numChannels);

	sender.write(remoteHost->stringValue(), remotePort->intValue(), artnetPacket, 18 + numChannels);
}
//void DMXArtNetDevice::endLoadFile()
//{
//	Engine::mainEngine->removeEngineListener(this);
//	setupReceiver();
//}

void DMXArtNetDevice::onControllableFeedbackUpdate(ControllableContainer* cc, Controllable* c)
{
	DMXDevice::onControllableFeedbackUpdate(cc, c);
	if (inputCC != nullptr && c == inputCC->enabled || c == localPort) setupReceiver();
}

void DMXArtNetDevice::run()
{
	if (!enabled) return;
	if (receiver == nullptr) return;

	while (!threadShouldExit())
	{
		String rAddress = "";
		int rPort = 0;

		int bytesRead = receiver->read(receiveBuffer, MAX_PACKET_LENGTH, false, rAddress, rPort);

		bool artExtPacket = false;

		if (bytesRead > 0)
		{
			for (uint8 i = 0; i < 8; ++i)
			{
				//DBG("Check " << (char)receiveBuffer[i] << " <> " << (char)artnetPacket[i]);
				if (receiveBuffer[i] != artnetPacket[i])
				{
					if (receiveBuffer[i] != artextPacket[i])
					{
						NLOGWARNING(niceName, "Received packet is not valid ArtNet");
						break;
					}
					else
					{
						artExtPacket = true;
					}
				}
			}

			int opcode = receiveBuffer[8] | receiveBuffer[9] << 8;

			switch (opcode)
			{
			case DMX_OPCODE:
			{
				//int sequence = receiveBuffer[12];


				int universe = receiveBuffer[14] & 0xF;
				int subnet = (receiveBuffer[14] >> 4) & 0xF;
				int net = receiveBuffer[15] & 0x7F;

				//LOG("Received with universe : " << universe << "/" << subnet << "/" << net);


#if JUCE_DEBUG
				int dmxDataLength = receiveBuffer[17] | receiveBuffer[16] << 8;
				jassert(dmxDataLength == DMX_NUM_CHANNELS);
#endif

				String sName = rAddress + ":" + String(rPort);
				Array<uint8> values = Array<uint8>(receiveBuffer + DMX_HEADER_LENGTH, DMX_NUM_CHANNELS);
				setDMXValuesIn(net, subnet, universe, values, sName);

			}
			break;

			case DMX_SYNC_OPCODE:
				DBG("Received Sync opcode, " << bytesRead);
				break;

			default:
			{
				DBG("ArtNet OpCode not handled : " << opcode << "( 0x" << String::toHexString(opcode) << ")");
			}
			break;
			}
		}
		else
		{
			wait(10); //100fps
		}
	}

	if (receiver != nullptr) receiver->shutdown();
}
