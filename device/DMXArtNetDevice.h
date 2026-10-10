/*
  ==============================================================================

	DMXArtNetDevice.h
	Created: 10 Apr 2017 12:44:42pm
	Author:  Ben

  ==============================================================================
*/

#pragma once

#include "DMXInputMerger.h"

#define DMX_OPCODE 0x5000
#define DMX_SYNC_OPCODE 0x5200
#define PROTOCOL_VERSION 14
#define DMX_HEADER_LENGTH 18
#define MAX_PACKET_LENGTH DMX_HEADER_LENGTH + DMX_NUM_CHANNELS

class DMXArtNetDevice :
	public DMXDevice,
	//public EngineListener,
	public Thread //receiving
{
public:
	DMXArtNetDevice(bool enableReceive = true);
	~DMXArtNetDevice();

	NetworkInterfaceParameter* networkInterface;
	IntParameter* localPort;
	EnumParameter* mergeMode = nullptr;
	DMXInputMerger inputMerger { DMXInputMerger::Protocol::ArtNet };
	//StringParameter * nodeName;

	StringParameter* remoteHost;
	IntParameter* remotePort;

	std::unique_ptr<DatagramSocket> receiver;
	std::unique_ptr<DatagramSocket> sender;
	CriticalSection senderLock;
	unsigned int receiverInterfaceIndex = 0;

	uint8 sequenceNumber;
	uint8 artnetPacket[MAX_PACKET_LENGTH]{ 'A','r','t','-','N','e','t',0, 0x00 , 0x50,  0, PROTOCOL_VERSION };
	uint8 artextPacket[MAX_PACKET_LENGTH]{ 'A','r','t','-','E','x','t',0, 0x00 , 0x50,  0, PROTOCOL_VERSION };
	uint8 receiveBuffer[MAX_PACKET_LENGTH];


	void refreshEnabled() override;
	void setupReceiver();
	void setupSender();
	void afterLoadJSONDataInternal() override;

	//void sendDMXValue(int channel, int value) override;
	//void sendDMXRange(int startChannel, Array<int> values) override;

	void sendDMXValuesInternal(int net, int subnet, int universe, uint8* values, int numChannels) override;

	//	void endLoadFile() override;

	void onContainerParameterChanged(Parameter* p) override;
	void onControllableFeedbackUpdate(ControllableContainer* cc, Controllable* c) override;

	void run() override;
};

