/*
  ==============================================================================

	DMXArtNetDevice.cpp
	Created: 10 Apr 2017 12:44:42pm
	Author:  Ben

  ==============================================================================
*/

#include "JuceHeader.h"
#include "DMXArtNetDevice.h"

#if ! JUCE_WINDOWS
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>

static unsigned int getArtNetInterfaceIndex(const String& ip)
{
	ifaddrs* addresses = nullptr;
	if (getifaddrs(&addresses) != 0) return 0;
	unsigned int index = 0;
	for (auto* address = addresses; address != nullptr; address = address->ifa_next)
	{
		if (address->ifa_addr != nullptr && address->ifa_addr->sa_family == AF_INET
			&& reinterpret_cast<sockaddr_in*>(address->ifa_addr)->sin_addr.s_addr == inet_addr(ip.toRawUTF8()))
		{
			index = if_nametoindex(address->ifa_name);
			break;
		}
	}
	freeifaddrs(addresses);
	return index;
}
#endif

DMXArtNetDevice::DMXArtNetDevice(bool enableReceive) :
	DMXDevice("ArtNet", ARTNET, enableReceive),
	Thread("ArtNetReceive"),
	localPort(nullptr),
	sequenceNumber(0)
{
	networkInterface = new NetworkInterfaceParameter();
	for (int i = networkInterface->enumValues.size(); --i >= 0;)
	{
		auto* option = networkInterface->enumValues[i];
		if (option->value.toString().containsChar(':')) networkInterface->removeOption(option->key);
	}
	addParameter(networkInterface);

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

	setupSender();

	updateConnectedParam();
	setupReceiver();
}

DMXArtNetDevice::~DMXArtNetDevice()
{
	senderThread.stopThread(1000);
	//if (Engine::mainEngine != nullptr) Engine::mainEngine->removeEngineListener(this);
	if (receiver != nullptr) receiver->shutdown();
	stopThread(1000);
}

void DMXArtNetDevice::refreshEnabled()
{
	setupReceiver();
	setupSender();
}

void DMXArtNetDevice::setupReceiver()
{
	stopThread(500);
	if (receiver != nullptr) receiver->shutdown();
	receiver.reset();
	receiverInterfaceIndex = 0;
	if (isConnected != nullptr) isConnected->setValue(false);

	if (!enabled || inputCC == nullptr || !inputCC->enabled->boolValue())
	{
		clearWarning("Input");
		return;
	}

	receiver.reset(new DatagramSocket());
	receiver->setEnablePortReuse(true);
	const String localIP = networkInterface->getIP();
#if JUCE_WINDOWS
	bool result = receiver->bindToPort(localPort->intValue(), localIP);
#else
	// POSIX sockets must bind to the wildcard address to receive broadcasts.
	bool result = receiver->bindToPort(localPort->intValue());
	if (result && localIP.isNotEmpty())
	{
		receiverInterfaceIndex = getArtNetInterfaceIndex(localIP);
#if JUCE_MAC
		result = receiverInterfaceIndex != 0 && setsockopt(receiver->getRawSocketHandle(), IPPROTO_IP,
			IP_BOUND_IF, &receiverInterfaceIndex, sizeof(receiverInterfaceIndex)) == 0;
#else
		const int enablePacketInfo = 1;
		result = receiverInterfaceIndex != 0 && setsockopt(receiver->getRawSocketHandle(), IPPROTO_IP,
			IP_PKTINFO, &enablePacketInfo, sizeof(enablePacketInfo)) == 0;
#endif
	}
#endif
	if (result)
	{
		//receiver->setEnablePortReuse(false);
		clearWarning("Input");
		NLOG(niceName, "Listening for ArtNet on port " << localPort->intValue());
	}
	else
	{
		receiver.reset();
		setWarningMessage("Could not bind ArtNet input to port " + localPort->stringValue()
			+ " on network interface " + networkInterface->getValueKey(), "Input");
		return;
	}

	startThread();
	isConnected->setValue(true);
}

void DMXArtNetDevice::setupSender()
{
	if (isCurrentlyLoadingData) return;
	const ScopedLock lock(senderLock);
	sender.reset();
	clearWarning("Output");
	if (!enabled || !outputCC->enabled->boolValue()) return;
	const String localIP = networkInterface->getIP();
	auto socket = std::make_unique<DatagramSocket>(true);
	bool result = socket->bindToPort(0, localIP);
#if ! JUCE_WINDOWS
	if (result && localIP.isNotEmpty())
	{
		const auto index = getArtNetInterfaceIndex(localIP);
#if JUCE_MAC
		result = index != 0 && setsockopt(socket->getRawSocketHandle(), IPPROTO_IP,
			IP_BOUND_IF, &index, sizeof(index)) == 0;
#else
		const String destination = remoteHost->stringValue();
		// Local unicast is delivered through loopback even when addressed to
		// another adapter. Preserve it while pinning network traffic to the NIC.
		const bool localDestination = destination == "localhost" || destination.startsWith("127.")
			|| destination == localIP || NetworkHelpers::isIPLocal(destination);
		const auto networkIndex = htonl(localDestination ? 0 : index);
		result = index != 0 && setsockopt(socket->getRawSocketHandle(), IPPROTO_IP,
			IP_UNICAST_IF, &networkIndex, sizeof(networkIndex)) == 0;
#endif
	}
#endif
	if (!result)
	{
		setWarningMessage("Could not bind ArtNet output to network interface "
			+ networkInterface->getValueKey(), "Output");
		return;
	}
	sender = std::move(socket);
}

void DMXArtNetDevice::afterLoadJSONDataInternal()
{
	setupReceiver();
	setupSender();
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
	const ScopedLock lock(senderLock);
	if (sender == nullptr) return;
	sequenceNumber = (sequenceNumber + 1) % 256;

	artnetPacket[12] = sequenceNumber;
	artnetPacket[13] = 0;
	artnetPacket[14] = (subnet << 4) | universe;
	artnetPacket[15] = net;
	artnetPacket[16] = 2;
	artnetPacket[17] = 0;

	memcpy(artnetPacket + DMX_HEADER_LENGTH, values, numChannels);

	sender->write(remoteHost->stringValue(), remotePort->intValue(), artnetPacket, 18 + numChannels);
}
//void DMXArtNetDevice::endLoadFile()
//{
//	Engine::mainEngine->removeEngineListener(this);
//	setupReceiver();
//}

void DMXArtNetDevice::onContainerParameterChanged(Parameter* p)
{
	if (!isCurrentlyLoadingData && p == networkInterface) refreshEnabled();
}

void DMXArtNetDevice::onControllableFeedbackUpdate(ControllableContainer* cc, Controllable* c)
{
	DMXDevice::onControllableFeedbackUpdate(cc, c);
	if (isCurrentlyLoadingData) return;
	if ((inputCC != nullptr && c == inputCC->enabled) || c == localPort) setupReceiver();
	else if (c == outputCC->enabled || c == remoteHost) setupSender();
}

void DMXArtNetDevice::run()
{
	if (!enabled) return;
	if (receiver == nullptr) return;

#if ! JUCE_WINDOWS && ! JUCE_MAC
	const auto localAddress = inet_addr(networkInterface->getIP().toRawUTF8());
	const auto loopbackIndex = if_nametoindex("lo");
#endif

	while (!threadShouldExit())
	{
		String rAddress = "";
		int rPort = 0;

		int bytesRead = 0;
#if ! JUCE_WINDOWS && ! JUCE_MAC
		if (receiverInterfaceIndex != 0)
		{
			sockaddr_in source {};
			iovec buffer { receiveBuffer, sizeof(receiveBuffer) };
			alignas(cmsghdr) char control[CMSG_SPACE(sizeof(in_pktinfo))] {};
			msghdr message {};
			message.msg_name = &source;
			message.msg_namelen = sizeof(source);
			message.msg_iov = &buffer;
			message.msg_iovlen = 1;
			message.msg_control = control;
			message.msg_controllen = sizeof(control);
			bytesRead = static_cast<int>(recvmsg(receiver->getRawSocketHandle(), &message, MSG_DONTWAIT));
			bool matchesInterface = false;
			for (auto* info = CMSG_FIRSTHDR(&message); bytesRead > 0 && info != nullptr; info = CMSG_NXTHDR(&message, info))
			{
				if (info->cmsg_level == IPPROTO_IP && info->cmsg_type == IP_PKTINFO
					&& info->cmsg_len >= CMSG_LEN(sizeof(in_pktinfo)))
				{
					const auto* packetInfo = reinterpret_cast<in_pktinfo*>(CMSG_DATA(info));
					const auto index = static_cast<unsigned int>(packetInfo->ipi_ifindex);
					matchesInterface = index == receiverInterfaceIndex
						|| (index == loopbackIndex && packetInfo->ipi_spec_dst.s_addr == localAddress);
				}
			}
			if (bytesRead > 0 && !matchesInterface) continue;
			rAddress = inet_ntoa(source.sin_addr);
			rPort = ntohs(source.sin_port);
		}
		else
#endif
			bytesRead = receiver->read(receiveBuffer, MAX_PACKET_LENGTH, false, rAddress, rPort);

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
