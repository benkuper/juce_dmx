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
    firstUniverse(0)
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
    for(auto & i : items) i->setFirstUniverse(index);
}
