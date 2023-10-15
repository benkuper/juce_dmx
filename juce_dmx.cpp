/*
  ==============================================================================



  ==============================================================================
*/

#ifdef ORGANIC_DMX_H_INCLUDED
/* When you add this cpp file to your project, you mustn't include it in a file where you've
   already included any other headers - just put it inside a file on its own, possibly with your config
   flags preceding it, but don't include anything else. That also includes avoiding any automatic prefix
   header files that the compiler may be using.
*/
#error "Incorrect use of JUCE cpp file"
#endif

#include "juce_dmx.h"

#include "DMXManager.cpp"
#include "DMXUniverse.cpp"
#include "DMXUniverseManager.cpp"
#include "device/DMXDevice.cpp"
#include "device/DMXSerialDevice.cpp"
#include "device/DMXArtNetDevice.cpp"
#include "device/DMXEnttecProDevice.cpp"
#include "device/DMXOpenUSBDevice.cpp"
extern "C"
{
#include "device/sacn/e131.c"
}
#include "device/DMXSACNDevice.cpp"

#include "ui/DMXUniverseEditor.cpp"