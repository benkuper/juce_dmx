/*
  ==============================================================================

	DMXUniverseManager.cpp
	Created: 10 Dec 2022 6:34:52pm
	Author:  bkupe

  ==============================================================================
*/

#include "JuceHeader.h"

DMXUniverseManager::DMXUniverseManager(bool useParams) :
	BaseManager("Universes"),
	useParams(useParams),
	firstUniverse(0),
	showArtNetAddressing(true)
{
	selectItemWhenCreated = false;
}

DMXUniverseManager::~DMXUniverseManager()
{
}

DMXUniverseItem* DMXUniverseManager::createItem()
{
	return new DMXUniverseItem(useParams, firstUniverse);
}

void DMXUniverseManager::setFirstUniverse(int index)
{
	firstUniverse = index;
	for (auto& u : items) { u->setFirstUniverse(index); };
}

void DMXUniverseManager::setShowArtNetAddressing(bool show)
{
	showArtNetAddressing = show;
	for (auto* u : items) u->setShowArtNetAddressing(show);
}

void DMXUniverseManager::addItemInternal(DMXUniverseItem* item, var data)
{
	item->setShowArtNetAddressing(showArtNetAddressing);
}
