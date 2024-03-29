/*
  ==============================================================================

	DMXOpenUSBDevice.h
	Created: 7 Apr 2017 11:22:57am
	Author:  Ben

  ==============================================================================
*/

#pragma once

class DMXOpenUSBDevice :
	public DMXSerialDevice
{
public:
	DMXOpenUSBDevice();
	~DMXOpenUSBDevice();

	uint32 prevMillis = 0;
	const uint8 startCode[1]{ 0 };

	void setPortConfig() override;
	void sendDMXValuesSerialInternal(int net, int subnet, int universe, uint8* values, int numChannels) override;
};