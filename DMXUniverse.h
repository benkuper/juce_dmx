/*
  ==============================================================================

	DMXUniverse.h
	Created: 10 Dec 2022 6:31:37pm
	Author:  bkupe

  ==============================================================================
*/

#pragma once
#define DMX_MAX_NET 128
#define DMX_MAX_SUBNET_PER_NET 16
#define DMX_MAX_UNIVERSES_PER_SUBNET 16
#define DMX_MAX_UNIVERSES DMX_MAX_NET * DMX_MAX_SUBNET_PER_NET * DMX_MAX_UNIVERSES_PER_SUBNET

#define DMX_NUM_CHANNELS 512

enum DMXByteOrder { BIT8, MSB, LSB };

class DMXValueParameter :
	public IntParameter
{
public:
	DMXValueParameter(const String& name, const String& description, int channel, int value, const DMXByteOrder t) :
		IntParameter(name, description, value, 0, t == BIT8 ? 255 : 65535),
		type(t),
		channel(channel)
	{

	}

	~DMXValueParameter() {}

	var getJSONDataInternal() override
	{
		var data = IntParameter::getJSONDataInternal();
		data.getDynamicObject()->setProperty("channel", channel);
		data.getDynamicObject()->setProperty("DMXType", type);
		return data;
	}

	void loadJSONDataInternal(var data) override
	{
		Parameter::loadJSONDataInternal(data);
		channel = data.getProperty("channel", false);
		setType(static_cast<DMXByteOrder>((int)data.getProperty("DMXType", BIT8)));
	}

	void setType(DMXByteOrder t)
	{
		type = t;

		if (type != BIT8) isOverriden = true;
		setRange(0, t == BIT8 ? 255 : 65535);

		notifyStateChanged();
	}

	void setValueFrom2Channels(int channel1, int channel2)
	{
		setValue(type == MSB ? ((channel1 << 8) + channel2) : ((channel2 << 8) + channel1));
		DBG("Set values from channels " << channel1 << ", " << channel2 << " : " << (int)value);

	}


	DMXByteOrder type;
	int channel;

	ControllableUI* createDefaultUI(Array<Controllable*> controllables = {}) override;

	static DMXValueParameter* create() { return new DMXValueParameter("DMX Value Parameter", "", 0, 1, BIT8); }
	virtual String getTypeString() const override { return getTypeStringStatic(); }
	static String getTypeStringStatic() { return "DMX Value"; }
};

class DMXUniverse
{
public:
	DMXUniverse(int net, int subnet, int universe);

	int net;
	int subnet;
	int universe;

	bool isDirty;

	SpinLock valuesLock;
	Array<uint8> values;
	virtual void updateValue(int channel, uint8 value, bool dirtyIfDifferent = false);
	virtual void updateValues(Array<uint8> values, bool dirtyIfDifferent = false);

	int getUniverseIndex();
	static int getUniverseIndex(int net, int subnet, int universe);

	bool checkSignature(int net, int subnet, int universe);
	String toString() const;
};


class DMXUniverseItem :
	public BaseItem,
	public DMXUniverse
{
public:
	DMXUniverseItem(bool useParams = false);
	~DMXUniverseItem();

	IntParameter* netParam;
	IntParameter* subnetParam;
	IntParameter* universeParam;

	bool useParams;
	Array<DMXValueParameter*> valueParams;

	virtual void updateValue(int channel, uint8 value, bool dirtyIfDifferent = false) override;
	virtual void updateValues(Array<uint8> values, bool dirtyIfDifferent = false) override;

	void onContainerParameterChangedInternal(Parameter*) override;
	InspectableEditor* getEditorInternal(bool isRoot, Array<Inspectable*> inspectables = Array<Inspectable*>()) override;

	DECLARE_TYPE("DMXUniverse")
};