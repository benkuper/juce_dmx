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
	networkInterface = new NetworkInterfaceParameter();
	// E1.31 uses IPv4 multicast addresses.
	for (int i = networkInterface->enumValues.size(); --i >= 0;)
	{
		auto* option = networkInterface->enumValues[i];
		if (option->value.toString().containsChar(':')) networkInterface->removeOption(option->key);
	}
	addParameter(networkInterface);

	localPort = inputCC->addIntParameter("Local Port", "Local port to receive SACN data. This needs to be enabled in order to receive data", E131_DEFAULT_PORT, 0, 65535);
	//receiveMulticast = inputCC->addBoolParameter("Multicast", "If checked, this will receive in Multicast Mode", false);
	//inputUniverse = inputCC->addIntParameter("Universe", "The Universe to receive from, from 0 to 15", 1, 1, 64000);
	inputCC->editorIsCollapsed = true;
	inputCC->enabled->setValue(false);

	nodeName = outputCC->addStringParameter("Node Name", "Name to advertise", "Chataigne");
	cid = outputCC->addStringParameter("CID", "Unique E1.31 component identifier for this sACN source", "");
	cid->setValue(fallbackCid.toString(), false, true, true);
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
	senderThread.stopThread(1000);
	//if (Engine::mainEngine != nullptr) Engine::mainEngine->removeEngineListener(this);

	signalThreadShouldExit();
	if (receiver != nullptr) receiver->shutdown();
	stopThread(1000);
}

void DMXSACNDevice::refreshEnabled()
{
	setupReceiver();
	setupSender();
}

void DMXSACNDevice::setupReceiver()
{
	signalThreadShouldExit();
	if (receiver != nullptr) receiver->shutdown();
	stopThread(500);


	receiver.reset();
	lastReceivedSequenceByStream.clear();
	if (isConnected != nullptr) isConnected->setValue(false);

	if (!enabled || !inputCC->enabled->boolValue())
	{
		clearWarning("Input");
		return;
	}

	isConnected->setValue(false);


	receiver.reset(new DatagramSocket());
	receiver->setEnablePortReuse(true);
	bool result = receiver->bindToPort(localPort->intValue());

	if (result)
	{
		clearWarning("Input");
		for (const auto& multicastAddress : multicastIn)
		{
			// Keep the wildcard bind: binding to a unicast address prevents
			// multicast reception on some platforms. Select the NIC in membership.
			ip_mreq membership {};
			membership.imr_multiaddr.s_addr = inet_addr(multicastAddress.toRawUTF8());
			const String localIP = networkInterface->getIP();
			membership.imr_interface.s_addr = localIP.isEmpty() ? htonl(INADDR_ANY) : inet_addr(localIP.toRawUTF8());
			if (setsockopt(receiver->getRawSocketHandle(), IPPROTO_IP, IP_ADD_MEMBERSHIP,
				reinterpret_cast<const char*>(&membership), sizeof(membership)) != 0)
			{
				LOGWARNING("Could not join sACN multicast group " << multicastAddress);
				setWarningMessage("Could not join sACN multicast group " + multicastAddress, "Input");
			}
		}

		NLOG(niceName, "Listening for sACN on port " << localPort->intValue());
	}
	else
	{
		setWarningMessage("Error binding port " + localPort->stringValue() + ", is it already taken ?", "Input");
		return;
	}

	isConnected->setValue(true);

	startThread();
}

void DMXSACNDevice::setupSender()
{
	if (isCurrentlyLoadingData) return;
	const ScopedLock lock(senderLock);
	sender.reset();
	clearWarning("Output");
	if (!enabled || !outputCC->enabled->boolValue()) return;

	const String localIP = networkInterface->getIP();
	auto socket = std::make_unique<DatagramSocket>();
	if (!socket->bindToPort(0, localIP))
	{
		setWarningMessage("Could not bind sACN output to network interface " + localIP, "Output");
		return;
	}

	// A local bind alone does not select the outgoing multicast interface.
	in_addr address {};
	address.s_addr = localIP.isEmpty() ? htonl(INADDR_ANY) : inet_addr(localIP.toRawUTF8());
	if (setsockopt(socket->getRawSocketHandle(), IPPROTO_IP, IP_MULTICAST_IF,
		reinterpret_cast<const char*>(&address), sizeof(address)) != 0)
	{
		setWarningMessage("Could not select sACN multicast output interface " + localIP, "Output");
		return;
	}
	sender = std::move(socket);
}

void DMXSACNDevice::afterLoadJSONDataInternal()
{
	setupReceiver();
	setupSender();
}

void DMXSACNDevice::setupMulticast(Array<DMXUniverse*> in, Array<DMXUniverse*> out)
{
	//Receiver
	multicastIn.clear();
	for (auto& u : in)
	{
		String s = getMulticastIPForUniverse(u->universe);
		multicastIn.addIfNotAlreadyThere(s);
	}
	// Recreating the socket drops memberships on the previously selected NIC.
	setupReceiver();


	////Sender
	const ScopedLock lock(senderLock);

	multicastOut.clear();
	outMulticastMap.clear();
	for (auto& u : out)
	{
		String s = getMulticastIPForUniverse(u->universe);
		multicastOut.add(s);
		outMulticastMap.set(u->universe, s);
		//sender.joinMulticast(s);
	}
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
	const ScopedLock lock(senderLock);
	if (sender == nullptr) return;
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
		updateSenderPacketMetadata(p);
		senderPackets.set(universe, p);
	}

	e131_packet_t* senderPacket = &senderPackets.getReference(universe);
	int packetNumChannels = ntohs(senderPacket->dmp.prop_val_cnt) - 1;
	if (numChannels != packetNumChannels)
	{
		LOG("Num Channels changed for universe " << universe << ", reinit packet");
		e131_pkt_init(senderPacket, universe, numChannels);
	}
	updateSenderPacketMetadata(*senderPacket);

	//String ip = remoteHost->stringValue();
	//e131_unicast_dest(&senderDest, remoteHost->stringValue().getCharPointer(), remotePort->intValue());

	memcpy(senderPacket->dmp.prop_val + 1, values, numChannels);

	e131_error_t e = e131_pkt_validate(senderPacket);
	if (e != E131_ERR_NONE) LOGWARNING("Packet validation error : " << e131_strerror(e));

	//ssize_t sent = e131_send(senderHandle, senderPacket, sender.sen);
	String ip = outMulticastMap.contains(universe) ? outMulticastMap[universe] : remoteHost->stringValue();
	int sent = sender->write(ip, remotePort->intValue(), senderPacket, sizeof(e131_packet_t));

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

void DMXSACNDevice::updateSenderPacketMetadata(e131_packet_t& packet) const
{
	Uuid sourceCid(cid->stringValue());
	if (sourceCid.isNull()) sourceCid = fallbackCid;
	memcpy(packet.root.cid, sourceCid.getRawData(), sizeof(packet.root.cid));

	zeromem(packet.frame.source_name, sizeof(packet.frame.source_name));
	nodeName->stringValue().copyToUTF8(reinterpret_cast<char*>(packet.frame.source_name), sizeof(packet.frame.source_name));
	packet.frame.priority = static_cast<uint8>(priority->intValue());
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

void DMXSACNDevice::onContainerParameterChanged(Parameter* p)
{
	if (isCurrentlyLoadingData) return;
	if (p == networkInterface)
	{
		setupReceiver();
		setupSender();
	}
}

void DMXSACNDevice::onControllableFeedbackUpdate(ControllableContainer* cc, Controllable* c)
{
	DMXDevice::onControllableFeedbackUpdate(cc, c);
	if (isCurrentlyLoadingData) return;
	if (c == inputCC->enabled || c == localPort /*|| c == receiveMulticast || c == inputUniverse*/) setupReceiver();
	else if (c == outputCC->enabled)
	{
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

		const int universe = ((receivedPacket.frame.universe >> 8) & 0xFF) | ((receivedPacket.frame.universe & 0xFF) << 8);
		const String streamKey = String::toHexString(receivedPacket.root.cid, static_cast<int>(sizeof(receivedPacket.root.cid)), 0)
			+ ":" + String(universe);

		// E1.31 sequence numbers are independent for each CID/universe stream.
		if (lastReceivedSequenceByStream.contains(streamKey) && e131_pkt_discard(&receivedPacket, lastReceivedSequenceByStream[streamKey])) {
			LOGWARNING("Out-of-order sACN packet received for universe " << universe);
			continue;
		}

		lastReceivedSequenceByStream.set(streamKey, receivedPacket.frame.seq_number);

		//int numChannels = ((receivedPacket.dmp.prop_val_cnt >> 8) & 0xFF) | ((receivedPacket.dmp.prop_val_cnt & 0xFF) << 8);
		//int firstChannel = ((receivedPacket.dmp.first_addr >> 8) & 0xFF) | ((receivedPacket.dmp.first_addr & 0xFF) << 8);

		String sName((char*)receivedPacket.frame.source_name);

		setDMXValuesIn(0, 0, universe, Array<uint8>(receivedPacket.dmp.prop_val + 1, DMX_NUM_CHANNELS));
	}
}
