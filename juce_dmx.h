/*
  ==============================================================================

  ==============================================================================
*/


/*******************************************************************************
 The block below describes the properties of this module, and is read by
 the Projucer to automatically generate project code that uses it.
 For details about the syntax and how to create or use a module, see the
 JUCE Module Format.txt file.


 BEGIN_JUCE_MODULE_DECLARATION

  ID:               juce_dmx
  vendor:           benkuper
  version:          1.0.0
  name:             DMX
  description:      DMX Handling, using OrganicUI
  website:          https://github.com/benkuper/juce_dmx
  license:          GPLv3

  dependencies:    juce_organicui, juce_serial

 END_JUCE_MODULE_DECLARATION

*******************************************************************************/

#pragma once
#define ORGANIC_DMX_H_INCLUDED

//==============================================================================

#ifdef _MSC_VER
#pragma warning (push)
// Disable warnings for long class names, padding, and undefined preprocessor definitions.
#pragma warning (disable: 4251 4786 4668 4820)
#endif


#include <juce_organicui/juce_organicui.h>
#include <juce_serial/juce_serial.h>

using namespace juce;

#include "DMXManager.h"
#include "DMXUniverse.h"
#include "DMXUniverseManager.h"
#include "device/DMXDevice.h"
#include "device/DMXSerialDevice.h"
#include "device/DMXArtNetDevice.h"
#include "device/DMXEnttecProDevice.h"
#include "device/DMXOpenUSBDevice.h"
extern "C"
{
#include "device/sacn/e131.h"
}
#include "device/DMXSACNDevice.h"

#include "ui/DMXUniverseEditor.h"