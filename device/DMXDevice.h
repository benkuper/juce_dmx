/*
  ==============================================================================

	DMXDevice.h
	Created: 7 Apr 2017 11:22:47am
	Author:  Ben

  ==============================================================================
*/
#pragma once


class DMXDevice :
	public ControllableContainer,
	public DMXManager::DMXManagerListener
{
public:
	enum Type { OPENDMX, ENTTEC_DMXPRO, ENTTEC_MK2, ARTNET, SACN };
	DMXDevice(const String& name, Type type, bool canReceive);
	virtual ~DMXDevice();

	Type type;

	bool enabled;
	bool canReceive;

	BoolParameter* isConnected;

	IntParameter* sendRate;

	EnablingControllableContainer* inputCC;
	EnablingControllableContainer* outputCC;

	OwnedArray<DMXUniverse, juce::CriticalSection> universesToSend;
	virtual void setEnabled(bool value);
	virtual void refreshEnabled() {}

	void updateSenderThread();

	void updateConnectedParam();
	virtual bool shouldHaveConnectionParam();


	virtual void setupMulticast(Array<DMXUniverse*> multicastIn, Array<DMXUniverse*> multicastOut) {}

	virtual void setDMXValues(DMXUniverse* u, int numChannels = DMX_NUM_CHANNELS);
	virtual void setDMXValues(int net, int subnet, int universe, uint8* values, int numChannels = DMX_NUM_CHANNELS);
	void sendDMXValues();
	virtual void sendDMXValuesInternal(int net, int subnet, int universe, uint8* values, int numChannels = DMX_NUM_CHANNELS) = 0;

	void setDMXValuesIn(int net, int subnet, int universe, Array<uint8> values, const String& sourceName = "");

	void onControllableStateChanged(Controllable* c) override;
	void onControllableFeedbackUpdate(ControllableContainer* cc, Controllable* c) override;

	virtual int getFirstUniverse();

	virtual void clearDevice();


	static DMXDevice* create(Type type);

	class SenderThread :
		public Thread
	{
	public:
		SenderThread(DMXDevice* d);
		~SenderThread();

		void run();

		DMXDevice* device;
	};

	SenderThread senderThread;

	class DMXDeviceListener
	{
	public:
		virtual ~DMXDeviceListener() {}

		virtual void dmxDeviceSetupChanged(DMXDevice* device) {}
		virtual void dmxDataInChanged(DMXDevice*, int net, int subnet, int universe, Array<uint8> values, const String& sourceName = "") {}
	};

	DECLARE_INSPECTACLE_SAFE_LISTENER(DMXDevice, dmxDevice);

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DMXDevice)

};