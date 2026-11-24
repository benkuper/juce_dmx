/*
  ==============================================================================

	DMXUniverseManager.h
	Created: 10 Dec 2022 6:34:52pm
	Author:  bkupe

  ==============================================================================
*/

#pragma once

class DMXUniverseManager :
	public Manager<DMXUniverseItem>
{
public:
	DMXUniverseManager(bool useParams = false);
	~DMXUniverseManager();

	bool useParams;
	int firstUniverse;
	bool showArtNetAddressing;

	DMXUniverseItem* createItem() override;

	void setFirstUniverse(int index);
	void setShowArtNetAddressing(bool show);
	void addItemInternal(DMXUniverseItem* item, var data) override;
};
