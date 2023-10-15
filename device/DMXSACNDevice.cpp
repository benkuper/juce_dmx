/*
  ==============================================================================

	DMXSACNDevice.cpp
	Created: 7 Apr 2021 7:49:19pm
	Author:  bkupe

  ==============================================================================
*/

#include "JuceHeader.h"
#include "DMXSACNDevice.h"

DMXSACNDevice::DMXSACNDevice() :
	DMXDevice("SACN", SACN, true),
	Thread("sACN Receive"),
	senderHandle(-1)
{
	localPort = inputCC->addIntParameter("Local Port", "Local port to receive SACN data. This needs to be enabled in order to receive data", E131_DEFAULT_PORT, 0, 65535);
	//receiveMulticast = inputCC->addBoolParameter("Multicast", "If checked, this will receive in Multicast Mode", false);
	//inputUniverse = inputCC->addIntParameter("Universe", "The Universe to receive from, from 0 to 15", 1, 1, 64000);
	inputCC->editorIsCollapsed = true;
	inputCC->enabled->setValue(false);

	nodeName = outputCC->addStringParameter("Node Name", "Name to advertise", "Chataigne");
	//sendMulticast = outputCC->addBoolParameter("Multicast", "If checked, this will send in Multicast Mode", false);
	remoteHost = outputCC->addStringParameter("Remote Host", "IP to which send the Art-Net to", "127.0.0.1");
	remotePort = outputCC->addIntParameter("Remote Port", "Local port to receive SACN data", E131_DEFAULT_PORT, 0, 65535);
	//outputUniverse = outputCC->addIntParameter("Universe", "The Universe to send to, from 0 to 15", 1, 1, 64000);
	priority = outputCC->addIntParameter("Priority", "Priority of the packets to send", 100, 0, 200);
	setupSender();

	updateConnectedParam();

}

DMXSACNDevice::~DMXSACNDevice()
{
	//if (Engine::mainEngine != nullptr) Engine::mainEngine->removeEngineListener(this);

	signalThreadShouldExit();
	if (receiver != nullptr) receiver->shutdown();
	stopThread(1000);
}

void DMXSACNDevice::setupReceiver()
{
	signalThreadShouldExit();
	if (receiver != nullptr) receiver->shutdown();
	stopThread(500);


	receiver.reset();

	if (!inputCC->enabled->boolValue())
	{
		clearWarning();
		return;
	}

	isConnected->setValue(false);


	receiver.reset(new DatagramSocket());
	bool result = receiver->bindToPort(localPort->intValue());
	if (result)
	{
		receiver->setEnablePortReuse(false);

		clearWarning();
		NLOG(niceName, "Listening for sACN on port " << localPort->intValue());
	}
	else
	{
		setWarningMessage("Error binding port " + localPort->stringValue() + ", is it already taken ?");
		return;
	}

	isConnected->setValue(true);

	startThread();
}

void DMXSACNDevice::setupSender()
{
	if (isCurrentlyLoadingData) return;
	if (!outputCC->enabled->boolValue()) return;

	senderHandle = e131_socket();
	//if (sendMulticast->boolValue()) sender.joinMulticast(getMulticastIPForUniverse(outputUniverse->intValue()));
	//else
	//sender.leaveMulticast(getMulticastIPForUniverse(outputUniverse->intValue()));


}

void DMXSACNDevice::setupMulticast(Array<DMXUniverse*> in, Array<DMXUniverse*> out)
{
	//Receiver
	//if (receiver != nullptr) for (auto& i : multicastIn) receiver->leaveMulticast(i);

	//multicastIn.clear();
	//for (auto& u : in)
	//{
	//	String s = getMulticastIPForUniverse(u->universe);
	//	multicastIn.add(s);
	//	if (receiver != nullptr) receiver->joinMulticast(s);
	//}


	////Sender
	//for (auto& i : multicastIn) sender.leaveMulticast(i);

	//multicastOut.clear();
	//for (auto& u : out)
	//{
	//	String s = getMulticastIPForUniverse(u->universe);
	//	multicastOut.add(s);
	//	sender.joinMulticast(s);
	//}
}

//void DMXSACNDevice::sendDMXValue(int channel, int value)
//{
//	senderPacket.dmp.prop_val[channel] = value;
//	DMXDevice::sendDMXValue(channel, value);
//}
//
//void DMXSACNDevice::sendDMXRange(int startChannel, Array<int> values)
//{
//	for (int i = 0; i < values.size(); i++)
//	{
//		int channel = startChannel + i;
//		if (channel < 0) continue;
//		if (channel > 512) break;
//
//		senderPacket.dmp.prop_val[channel] = values[i];
//	}
//
//	DMXDevice::sendDMXRange(startChannel, values);
//}

void DMXSACNDevice::sendDMXValuesInternal(int net, int subnet, int universe, uint8* values, int numChannels)
{
	//String ip = sendMulticast->boolValue() ? getMulticastIPForUniverse(outputUniverse->intValue()) : remoteHost->stringValue();

	if (universe == 0)
	{
		LOGWARNING("Universe 0 is reserved for discovery, please use another one");
		return;
	}

	if (!senderPackets.contains(universe))
	{
		e131_packet_t p;
		e131_pkt_init(&p, universe, numChannels);
		memcpy(&p.frame.source_name, nodeName->stringValue().getCharPointer(), nodeName->stringValue().length());
		senderPackets.set(universe, p);
	}

	e131_packet_t* senderPacket = &senderPackets.getReference(universe);
	int packetNumChannels = ntohs(senderPacket->dmp.prop_val_cnt) - 1;
	if (numChannels != packetNumChannels)
	{
		LOG("Num Channels changed for universe " << universe << ", reinit packet");
		e131_pkt_init(senderPacket, universe, numChannels);
		memcpy(senderPacket->frame.source_name, nodeName->stringValue().getCharPointer(), nodeName->stringValue().length());
	}

	String ip = remoteHost->stringValue();
	e131_unicast_dest(&senderDest, remoteHost->stringValue().getCharPointer(), remotePort->intValue());

	memcpy(senderPacket->dmp.prop_val + 1, values, numChannels);

	e131_error_t e = e131_pkt_validate(senderPacket);
	if (e != E131_ERR_NONE) LOGWARNING("Packet validation error : " << e131_strerror(e));

	ssize_t sent = e131_send(senderHandle, senderPacket, &senderDest);
	if (sent <= 0)
	{
		LOGERROR("Error sending sACN data");
	}
	senderPacket->frame.seq_number++;


	//int numWritten = sender.write(ip, remotePort->intValue(), senderPacket, sizeof(e131_packet_t));



	//if (numWritten == -1)
	//{
	//	LOGWARNING("Error sending data");
	//}

}

//void DMXSACNDevice::endLoadFile()
//{
//	Engine::mainEngine->removeEngineListener(this);
//	setupReceiver();
//	setupSender();
//}

String DMXSACNDevice::getMulticastIPForUniverse(int universe) const
{
	return "239.255." + String((universe >> 8) & 0xFF) + "." + String(universe & 0xFF);
}

int DMXSACNDevice::getFirstUniverse()
{
	return 1;
}

void DMXSACNDevice::onControllableFeedbackUpdate(ControllableContainer* cc, Controllable* c)
{
	DMXDevice::onControllableFeedbackUpdate(cc, c);
	if (c == inputCC->enabled || c == localPort /*|| c == receiveMulticast || c == inputUniverse*/) setupReceiver();
	else if (cc == outputCC)
	{
		//if (c == sendMulticast) remoteHost->setEnabled(!sendMulticast->boolValue());
		setupSender();
	}
}

void DMXSACNDevice::run()
{
	if (!enabled) return;

	while (!threadShouldExit())
	{
		wait(10); //100fps

		//if (receiverHandle < 0) return;
		//ssize_t numRead = e131_recv(receiverHandle, &receivedPacket);
		int numRead = receiver->read(&receivedPacket, sizeof(receivedPacket), true);

		if (threadShouldExit()) return;

		if (numRead == -1)
		{
			LOGWARNING("Error receiving data");
			continue;
		}

		if ((receivedError = e131_pkt_validate(&receivedPacket)) != E131_ERR_NONE) {
			LOGWARNING("e131_pkt_validate: " << e131_strerror(receivedError));
			continue;
		}
		if (e131_pkt_discard(&receivedPacket, receivedSeq)) {
			LOGWARNING("warning: packet out of order received\n");
			continue;
		}

		receivedSeq = receivedPacket.frame.seq_number;

		int universe = ((receivedPacket.frame.universe >> 8) & 0xFF) | ((receivedPacket.frame.universe & 0xFF) << 8);
		//int numChannels = ((receivedPacket.dmp.prop_val_cnt >> 8) & 0xFF) | ((receivedPacket.dmp.prop_val_cnt & 0xFF) << 8);
		//int firstChannel = ((receivedPacket.dmp.first_addr >> 8) & 0xFF) | ((receivedPacket.dmp.first_addr & 0xFF) << 8);

		String sName((char*)receivedPacket.frame.source_name);

		setDMXValuesIn(0, 0, universe, Array<uint8>(receivedPacket.dmp.prop_val + 1, DMX_NUM_CHANNELS));
	}
}