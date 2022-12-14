/*
  ==============================================================================

	DMXUniverse.cpp
	Created: 10 Dec 2022 6:31:37pm
	Author:  bkupe

  ==============================================================================
*/

#include "JuceHeader.h"
#include "DMXUniverse.h"

DMXUniverse::DMXUniverse(int net, int subnet, int universe) :
	net(net), subnet(subnet), universe(universe),
	isDirty(true)
{
	values.resize(DMX_NUM_CHANNELS);
}

int DMXUniverse::getUniverseIndex()
{
	return DMXUniverse::getUniverseIndex(net, subnet, universe);
}

int DMXUniverse::getUniverseIndex(int net, int subnet, int universe)
{
	return universe | subnet << 4 | net << 8;;
}

bool DMXUniverse::checkSignature(int _net, int _subnet, int _universe)
{
	return net == _net && subnet == _subnet && universe == _universe;
}

void DMXUniverse::updateValue(int channel, uint8 value, bool dirtyIfDifferent)
{
	jassert(channel >= 0 && channel < DMX_NUM_CHANNELS);

	GenericScopedLock lock(valuesLock);

	if (dirtyIfDifferent && values[channel] == value) return;
	
	values.set(channel, value);
	isDirty = true;
}

void DMXUniverse::updateValues(Array<uint8> newValues, bool dirtyIfDifferent)
{
	jassert(newValues.size() == DMX_NUM_CHANNELS);

	GenericScopedLock lock(valuesLock);

	if (dirtyIfDifferent && values == newValues) return;

	values.swapWith(newValues);
	isDirty = true;
}



DMXUniverseItem::DMXUniverseItem(bool useParams) :
	BaseItem("Universe", false, false),
	DMXUniverse(0, 0, 0),
	useParams(useParams)
{
	editorIsCollapsed = !useParams;

	netParam = addIntParameter("Net", "If appliccable the net for this universe", 0, 0, 15);
	subnetParam = addIntParameter("Subnet", "If applicable the subnet for this universe", 0, 0, 15);
	universeParam = addIntParameter("Universe", "The universe", 0, 0);

	if (useParams)
	{
		netParam->hideInEditor = true;
		subnetParam->hideInEditor = true;
		universeParam->hideInEditor = true;

		for (int i = 0; i < DMX_NUM_CHANNELS; i++)
		{
			DMXValueParameter* p = new DMXValueParameter("Channel " + String(i + 1), "Value for this channel", i, 0, DMXByteOrder::BIT8);
			valueParams.add(p);
			addParameter(p);
		}
	}
}


String DMXUniverse::toString() const
{
	return "[Net : " + String(net) + ", Subnet" + String(subnet) + ", Universe : " + String(universe) + "]";
}

DMXUniverseItem::~DMXUniverseItem()
{

}


void DMXUniverseItem::updateValue(int channel, uint8 value, bool dirtyIfDifferent)
{
	jassert(channel >= 0 && channel < DMX_NUM_CHANNELS);

	if (!useParams)
	{
		DMXUniverse::updateValue(channel, value, dirtyIfDifferent);
		return;
	}

	jassert(channel >= 0 && channel < DMX_NUM_CHANNELS);
	valueParams[channel]->setValue(value);
}

void DMXUniverseItem::updateValues(Array<uint8> newValues, bool dirtyIfDifferent)
{
	if (!useParams)
	{
		DMXUniverse::updateValues(newValues, dirtyIfDifferent);
		return;
	}

	jassert(newValues.size() == DMX_NUM_CHANNELS);

	for (int i = 0; i < DMX_NUM_CHANNELS; ++i)
	{
		if (DMXValueParameter* vp = valueParams[i])
		{
			if (vp->type == DMXByteOrder::BIT8) vp->setValue(newValues[i]);
			else if (i < DMX_NUM_CHANNELS - 1)
			{
				vp->setValueFrom2Channels(newValues[i], newValues[i + 1]);
				i++;
			}
		}
	}
}


void DMXUniverseItem::onContainerParameterChangedInternal(Parameter* p)
{
	if (p == netParam) net = netParam->intValue();
	else if (p == subnetParam) subnet = subnetParam->intValue();
	else if (p == universeParam) universe = universeParam->intValue();

	if (!useParams) return;
	int index = valueParams.indexOf((DMXValueParameter*)p);
	DMXUniverse::updateValue(index, p->intValue());

}

InspectableEditor* DMXUniverseItem::getEditorInternal(bool isRoot, Array<Inspectable*> inspectables)
{
	if (inspectables.isEmpty()) inspectables.add(this);
	return new DMXUniverseItemEditor(inspectables, isRoot);
}


ControllableUI* DMXValueParameter::createDefaultUI(Array<Controllable*> controllables)
{
	if (controllables.size() == 0) controllables = { this };
	return new DMXValueParameterUI(Inspectable::getArrayAs<Controllable, DMXValueParameter>(controllables));
}
