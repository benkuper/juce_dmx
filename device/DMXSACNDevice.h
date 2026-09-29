/*
  ==============================================================================

	DMXSACNDevice.h
	Created: 7 Apr 2021 7:49:19pm
	Author:  bkupe

  ==============================================================================
*/

#pragma once

#define NUM_CHANNELS 512

#pragma warning(push) 
#pragma warning(disable:4201) 
#include "sacn/e131.h"
#pragma warning(pop)

class DMXSACNDevice :
	public DMXDevice,
	//	public EngineListener,
	public Thread //receiving
{
public:
	DMXSACNDevice();
	~DMXSACNDevice();

	//EnumParameter * networkInterface;
	IntParameter* localPort;
	//BoolParameter* receiveMulticast;
	//IntParameter* inputUniverse;


	StringParameter* remoteHost;
	IntParameter* remotePort;
	StringParameter* nodeName;
	StringParameter* cid;
	Uuid fallbackCid;
	//BoolParameter* sendMulticast;
	//IntParameter* outputUniverse;
	IntParameter* priority;

	//Receiver
	//int receiverHandle;
	std::unique_ptr<DatagramSocket> receiver;
	e131_packet_t receivedPacket;
	e131_error_t receivedError;
	HashMap<String, uint8> lastReceivedSequenceByStream;

	//Sender
	int senderHandle;
	HashMap<int, e131_packet_t> senderPackets;
	//e131_addr_t senderDest;
	DatagramSocket sender;


	Array<String> multicastIn;
	Array<String> multicastOut;
	HashMap<int, String> outMulticastMap;


	void refreshEnabled() override;

	void setupReceiver();
	void setupSender();

	void setupMulticast(Array<DMXUniverse*> in, Array<DMXUniverse*> out) override;

	//void sendDMXValue(int channel, int value) override;
	//void sendDMXRange(int startChannel, Array<int> values) override;

	void sendDMXValuesInternal(int net, int subnet, int universe, uint8* values, int numChannels) override;
	void updateSenderPacketMetadata(e131_packet_t& packet) const;

	//	void endLoadFile() override;

	String getMulticastIPForUniverse(int universe) const;

	int getFirstUniverse() override;

	void onControllableFeedbackUpdate(ControllableContainer* cc, Controllable* c) override;

	void run() override;
};
