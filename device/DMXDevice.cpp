/*
  ==============================================================================

	DMXDevice.cpp
	Created: 7 Apr 2017 11:22:47am
	Author:  Ben

  ==============================================================================
*/

#include "JuceHeader.h"
#include "DMXDevice.h"

DMXDevice::DMXDevice(const String& name, Type _type, bool canReceive) :
	ControllableContainer(name),
	type(_type),
	enabled(true),
	isConnected(nullptr),
	canReceive(canReceive),
	sendExtraWaitMS(0),
	inputCC(nullptr),
	outputCC(nullptr),
	senderThread(this)
{
	saveAndLoadRecursiveData = true;

	DMXManager::getInstance()->addDMXManagerListener(this);


	if (canReceive)
	{
		inputCC = new EnablingControllableContainer("Input");
		inputCC->enabled->setValue(false);
		addChildControllableContainer(inputCC, true);
	}

	outputCC = new EnablingControllableContainer("Output");
	sendRate = outputCC->addIntParameter("Send Rate", "The rate at which to send data.", 40, 1, 200);
	sendRate->canBeDisabledByUser = true;

	addChildControllableContainer(outputCC, true);

	updateSenderThread();
}

DMXDevice::~DMXDevice()
{
	senderThread.stopThread(1000);
	if (DMXManager::getInstanceWithoutCreating() != nullptr) DMXManager::getInstance()->removeDMXManagerListener(this);
}


void DMXDevice::setEnabled(bool value)
{
	if (enabled == value) return;
	enabled = value;
	refreshEnabled();
	updateSenderThread();
}

void DMXDevice::updateSenderThread()
{
	bool shouldRun = enabled && outputCC->enabled->boolValue() && sendRate->enabled;

	if (shouldRun != senderThread.isThreadRunning())
	{
		if (shouldRun) senderThread.startThread();
		else senderThread.stopThread(1000);
	}
}

void DMXDevice::updateConnectedParam()
{
	if (shouldHaveConnectionParam())
	{
		isConnected = addBoolParameter("Connected", "If checked, the device is connected", false);
		isConnected->isControllableFeedbackOnly = true;
		isConnected->isSavable = false;
		isConnected->hideInEditor = true;
	}
	else
	{
		removeControllable(isConnected);
		isConnected = nullptr;
	}

	dmxDeviceListeners.call(&DMXDeviceListener::dmxDeviceSetupChanged, this);
}

bool DMXDevice::shouldHaveConnectionParam()
{
	return canReceive && inputCC != nullptr && inputCC->enabled->boolValue();
}


void DMXDevice::setDMXValuesIn(int net, int subnet, int universe, Array<uint8> values, const String& sourceName)
{
	jassert(values.size() == DMX_NUM_CHANNELS);
	dmxDeviceListeners.call(&DMXDeviceListener::dmxDataInChanged, this, net, subnet, universe, values, sourceName);
}



void DMXDevice::onControllableStateChanged(Controllable* c)
{
	if (c == sendRate) updateSenderThread();
}

void DMXDevice::onControllableFeedbackUpdate(ControllableContainer* cc, Controllable* c)
{
	if (inputCC != nullptr && c == inputCC->enabled) updateConnectedParam();
	else if (c == outputCC->enabled) updateSenderThread();
}

int DMXDevice::getFirstUniverse()
{
	return 0;
}

void DMXDevice::setDMXValues(DMXUniverse* u, int numChannels)
{
	setDMXValues(u->net, u->subnet, u->universe, u->values.getRawDataPointer(), numChannels);
}

void DMXDevice::setDMXValues(int net, int subnet, int universe, uint8* values, int numChannels)
{
	if (!outputCC->enabled->boolValue()) return;


	DMXUniverse* targetU = nullptr;
	for (auto& u : universesToSend)
	{
		if (u->net == net && u->subnet == subnet && u->universe == universe)
		{
			targetU = u;
			break;
		}
	}

	if (targetU == nullptr)
	{
		targetU = new DMXUniverse(net, subnet, universe);
		universesToSend.add(targetU);
	}

	targetU->updateValues(Array<uint8>(values, numChannels));

	if (!sendRate->enabled)
	{
		sendDMXValues();
	}
}

void DMXDevice::sendDMXValues()
{
	if (!enabled) return;
	for (auto& u : universesToSend) sendDMXValuesInternal(u->net, u->subnet, u->universe, u->values.getRawDataPointer(), DMX_NUM_CHANNELS);
	universesToSend.clear();
}


void DMXDevice::clearDevice()
{
	senderThread.stopThread(1000);
}

DMXDevice* DMXDevice::create(Type type)
{
	switch (type)
	{
	case OPENDMX:
		return new DMXOpenUSBDevice();
		break;

	case ENTTEC_DMXPRO:
		return new DMXEnttecProDevice();
		break;

	case ENTTEC_MK2:
		return new DMXEnttecProDevice(); //tmp but seems to work for 1 universe
		break;

	case ARTNET:
		return new DMXArtNetDevice();
		break;

	case SACN:
		return new DMXSACNDevice();
		break;

	default:
		DBG("Not handled");
		break;
	}

	return nullptr;
}

DMXDevice::SenderThread::SenderThread(DMXDevice* d) :
	Thread("DMX Sender Thread"),
	device(d)
{
}

DMXDevice::SenderThread::~SenderThread() {
	stopThread(1000);
}

void DMXDevice::SenderThread::run() {
	while (!threadShouldExit())
	{
		wait((1000 / device->sendRate->intValue()) + device->sendExtraWaitMS);
		device->sendDMXValues();
	}
}
