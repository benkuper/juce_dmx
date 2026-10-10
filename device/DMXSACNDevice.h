/*
  ==============================================================================

	DMXSACNDevice.h
	Created: 7 Apr 2021 7:49:19pm
	Author:  bkupe

  ==============================================================================
*/

#pragma once

#include "DMXInputMerger.h"

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

	NetworkInterfaceParameter* networkInterface;
	BoolParameter* useMulticast;
	IntParameter* localPort;
	EnumParameter* mergeMode;
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
	DMXInputMerger inputMerger { DMXInputMerger::Protocol::SACN };

	//Sender
	int senderHandle;
	HashMap<int, e131_packet_t> senderPackets;
	//e131_addr_t senderDest;
	std::unique_ptr<DatagramSocket> sender;
	CriticalSection senderLock;


	Array<String> multicastIn;
	Array<String> multicastOut;
	HashMap<int, String> outMulticastMap;


	void refreshEnabled() override;

	void setupReceiver();
	void setReceiverMembership(const String& address, bool join);
	void setupSender();
	void afterLoadJSONDataInternal() override;

	void setupMulticast(Array<DMXUniverse*> in, Array<DMXUniverse*> out) override;

	//void sendDMXValue(int channel, int value) override;
	//void sendDMXRange(int startChannel, Array<int> values) override;

	void sendDMXValuesInternal(int net, int subnet, int universe, uint8* values, int numChannels) override;
	void updateSenderPacketMetadata(e131_packet_t& packet) const;

	//	void endLoadFile() override;

	String getMulticastIPForUniverse(int universe) const;

	int getFirstUniverse() override;

	void onContainerParameterChanged(Parameter* p) override;
	void onControllableFeedbackUpdate(ControllableContainer* cc, Controllable* c) override;

	void run() override;
};
