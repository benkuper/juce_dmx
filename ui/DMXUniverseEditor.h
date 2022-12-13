/*
  ==============================================================================

	DMXUniverseEditor.h
	Created: 10 Dec 2022 8:54:52pm
	Author:  bkupe

  ==============================================================================
*/

#pragma once

class DMXValueParameterUI :
	public IntStepperUI
{
public:
	DMXValueParameterUI(Array<DMXValueParameter*> dmxP);
	~DMXValueParameterUI();

	Array<DMXValueParameter*> dmxParameters;
	void updateUIParamsInternal() override;

	void addPopupMenuItemsInternal(PopupMenu* p) override;
	void handleMenuSelectedID(int result) override;
};

class DMXUniverseItemEditor :
	public BaseItemEditor
{
public:
	DMXUniverseItemEditor(Array<Inspectable*> inspectables, bool isRoot);
	~DMXUniverseItemEditor();

	DMXUniverseItem* dmxUniverse;

	std::unique_ptr<IntParameterLabelUI> netUI;
	std::unique_ptr<IntParameterLabelUI> subnetUI;
	std::unique_ptr<IntParameterLabelUI> universeUI;

	void resizedInternalHeaderItemInternal(Rectangle<int>& r) override;
};
