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
	inputCC(nullptr),
	outputCC(nullptr)
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
	addChildControllableContainer(outputCC, true);
	//alwaysSend = outputCC->addBoolParameter("Always Send", "If checked, the device will always send the stored values to the constant rate set by the target rate parameter.\nIf you experience some lags, try unchecking this option.", true);
	//targetRate = outputCC->addIntParameter("Target send rate", "If fixed rate is checked, this is the frequency in Hz of the sending rate", 40, 1, 20000);
	//
	//if (alwaysSend->boolValue()) startTimer(1000/targetRate->intValue());

}

DMXDevice::~DMXDevice()
{
	if (DMXManager::getInstanceWithoutCreating() != nullptr) DMXManager::getInstance()->removeDMXManagerListener(this);
}


void DMXDevice::setEnabled(bool value)
{
	if (enabled == value) return;
	enabled = value;
	refreshEnabled();
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

//void DMXDevice::sendDMXValue(int channel, int value) //channel 1-512
//{
//	{
//		ScopedLock lock(dmxLock);
//		if (channel < 0 || channel > 512) return;
//		dmxDataOut[channel - 1] = (uint8)value;
//	}
//	
//	if (!alwaysSend->boolValue()) sendDMXValues();
//}
//
//void DMXDevice::sendDMXRange(int startChannel, Array<int> values)
//{
//	{
//		ScopedLock lock(dmxLock);
//		int numValues = values.size();
//		for (int i = 0; i < numValues; ++i)
//		{
//			int channel = startChannel + i;
//			if (channel < 0) continue;
//			if (channel > 512) break;
//
//			dmxDataOut[channel - 1] = (uint8)(values[i]);
//		}
//	}
//	
//	if (!alwaysSend->boolValue()) sendDMXValues();
//	
//}

void DMXDevice::setDMXValuesIn(int net, int subnet, int universe, Array<uint8> values, const String& sourceName)
{
	jassert(values.size() == DMX_NUM_CHANNELS);
	dmxDeviceListeners.call(&DMXDeviceListener::dmxDataInChanged, this, net, subnet, universe, values, sourceName);
}


void DMXDevice::onControllableFeedbackUpdate(ControllableContainer* cc, Controllable* c)
{
	if (cc == inputCC && c == inputCC->enabled) updateConnectedParam();
}

int DMXDevice::getFirstUniverse()
{
	return 0;
}

void DMXDevice::sendDMXValues(DMXUniverse* u, int numChannels)
{
	if (!outputCC->enabled->boolValue()) return;
	sendDMXValues(u->net, u->subnet, u->universe, u->values.getRawDataPointer(), numChannels);
}

void DMXDevice::sendDMXValues(int net, int subnet, int universe, uint8* values, int numChannels)
{
	if (!outputCC->enabled->boolValue()) return;
	ScopedLock lock(dmxLock);
	sendDMXValuesInternal(net, subnet, universe, values, jmin(numChannels, DMX_NUM_CHANNELS));
}



void DMXDevice::clearDevice()
{
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
