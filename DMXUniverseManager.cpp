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
    useParams(useParams)
{
    selectItemWhenCreated = false;
}

DMXUniverseManager::~DMXUniverseManager()
{
}

DMXUniverse* DMXUniverseManager::createItem()
{
    return new DMXUniverse(useParams);
}
