/*  ===========================================================================
*
*   This file is part of HISE.
*   Copyright 2016 Christoph Hart
*
*   HISE is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   HISE is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with HISE.  If not, see <http://www.gnu.org/licenses/>.
*
*   Commercial licenses for using HISE in an closed source project are
*   available on request. Please visit the project's website to get more
*   information about commercial licensing:
*
*   http://www.hise.audio/
*
*   HISE is based on the JUCE library,
*   which must be separately licensed for closed source applications:
*
*   http://www.juce.com
*
*   ===========================================================================
*/

namespace hise { using namespace juce;


Component* ScriptComponentList::Panel::createContentComponent(int /*index*/)
{
	auto jp = dynamic_cast<JavascriptProcessor*>(getConnectedProcessor());
	auto c = jp->getContent();

	return new ScriptComponentList(c);
}




ScriptComponentListItem::ScriptComponentListItem(const ValueTree& v, UndoManager& um_, ScriptingApi::Content* c, const String& searchTerm_) : 
	tree(v),
	undoManager(um_),
	content(c),
	searchTerm(searchTerm_)
{
	

	static const Identifier coPro("ContentProperties");

	if (tree.getType() == coPro)
		id = "Components";
	else
		id = tree.getProperty("id");

	connectedComponent = content->getComponentWithName(id);

	tree.addListener(this);
}

void ScriptComponentListItem::paintItem(Graphics& g, int width, int height)
{
	auto area = Rectangle<int>(0, 0, width - 1, height - 1);

	g.setColour(isSelected() ? Colour(SIGNAL_COLOUR).withAlpha(0.4f) : Colours::black.withAlpha(0.2f));

	g.fillRect(area);
	g.setColour(Colours::white.withAlpha(0.1f));
	g.drawRect(area, 1);




	if (connectedComponent != nullptr)
	{
		static const Identifier sip("saveInPreset");

		const bool saveInPreset = connectedComponent->getScriptObjectProperties()->getProperty(sip);

		Colour c3 = saveInPreset ? Colours::green : Colours::red;

		c3 = c3.withAlpha(JUCE_LIVE_CONSTANT_OFF(0.3f));

		g.setColour(c3);

		const float offset = JUCE_LIVE_CONSTANT_OFF(8.0f);
		Rectangle<float> circle(offset, offset, (float)ITEM_HEIGHT - 2.0f * offset, (float)ITEM_HEIGHT - 2.0f * offset);

		g.fillEllipse(circle);

		g.drawEllipse(circle, 1.0f);
	}

	g.setColour(Colours::white);


	if (connectedComponent == nullptr || !connectedComponent->isShowing())
	{
		g.setColour(Colours::white.withAlpha(0.4f));
	}

	g.setFont(GLOBAL_BOLD_FONT());

	int xOffset = ITEM_HEIGHT + 2;

	g.drawText(id, xOffset, 0, width - 4, height, Justification::centredLeft, true);

	xOffset += GLOBAL_BOLD_FONT().getStringWidth(id) + 10;

	g.setColour(Colours::white.withAlpha(0.2f));

	g.drawText(tree.getProperty("type"), 4 + xOffset, 0, width - 4, height, Justification::centredLeft, true);
}

void ScriptComponentListItem::itemSelectionChanged(bool isNowSelected)
{
	if (!fitsSearch)
	{
		setSelected(false, false, dontSendNotification);
		return;
	}

	auto b = content->getScriptProcessor()->getMainController_()->getScriptComponentEditBroadcaster();

	auto sc = content->getComponentWithName(id);

	if (sc != nullptr)
	{
		if (isNowSelected)
		{
			b->addToSelection(sc);
		}
		else
		{
			b->removeFromSelection(sc);
		}
	}
}


var ScriptComponentListItem::getDragSourceDescription()
{
	return "ScriptComponent";
}

bool ScriptComponentListItem::isInterestedInDragSource(const DragAndDropTarget::SourceDetails& dragSourceDetails)
{
	return dragSourceDetails.description == "ScriptComponent";
}

void ScriptComponentListItem::itemDropped(const DragAndDropTarget::SourceDetails&, int insertIndex)
{
	OwnedArray<ValueTree> selectedTrees;
	getSelectedTreeViewItems(*getOwnerView(), selectedTrees);

	moveItems(*getOwnerView(), selectedTrees, tree, insertIndex, undoManager);

	content->updateAndSetLevel(ScriptingApi::Content::FullRecompile);
}

void ScriptComponentListItem::moveItems(TreeView& treeView, const OwnedArray<ValueTree>& items, ValueTree newParent, int insertIndex, UndoManager& undoManager)
{
	if (items.size() > 0)
	{
		ScopedPointer<XmlElement> oldOpenness(treeView.getOpennessState(false));

		for (int i = items.size(); --i >= 0;)
		{
			ValueTree& v = *items.getUnchecked(i);

			if (v.getParent().isValid() && newParent != v && !newParent.isAChildOf(v))
			{
				if (v.getParent() == newParent && newParent.indexOf(v) < insertIndex)
					--insertIndex;

				auto cPos = ContentValueTreeHelpers::getLocalPosition(v);
				ContentValueTreeHelpers::getAbsolutePosition(v, cPos);

				auto pPos = ContentValueTreeHelpers::getLocalPosition(v.getParent());
				ContentValueTreeHelpers::getAbsolutePosition(v.getParent(), pPos);

				ContentValueTreeHelpers::updatePosition(v, cPos, pPos);

				v.getParent().removeChild(v, &undoManager);
				newParent.addChild(v, insertIndex, &undoManager);
			}
		}

		if (oldOpenness != nullptr)
			treeView.restoreOpennessState(*oldOpenness, false);
	}
}

void ScriptComponentListItem::getSelectedTreeViewItems(TreeView& treeView, OwnedArray<ValueTree>& items)
{
	const int numSelected = treeView.getNumSelectedItems();

	for (int i = 0; i < numSelected; ++i)
		if (const ScriptComponentListItem* vti = dynamic_cast<ScriptComponentListItem*> (treeView.getSelectedItem(i)))
			items.add(new ValueTree(vti->tree));
}

void ScriptComponentListItem::updateSelection(ScriptComponentSelection newSelection)
{
	bool select = false;

	if (connectedComponent != nullptr)
	{
		select = newSelection.contains(connectedComponent);
	}

	setSelected(select, false, dontSendNotification);

	for (int i = 0; i < getNumSubItems(); i++)
	{
		static_cast<ScriptComponentListItem*>(getSubItem(i))->updateSelection(newSelection);
	}
}

#define ADD_WIDGET(widgetIndex, widgetClass) case (int)widgetIndex: ScriptingApi::Content::Helpers::createNewComponentData(content, pTree, widgetClass::getStaticObjectName().toString(), ScriptingApi::Content::Helpers::getUniqueIdentifier(content, widgetClass::getStaticObjectName().toString()).toString()); break;

ScriptComponentList::ScriptComponentList(ScriptingApi::Content* c) :
	ScriptComponentEditListener(dynamic_cast<Processor*>(c->getScriptProcessor())),
	undoButton("Undo"),
	redoButton("Redo"),
	foldButton("Fold"),
	unfoldButton("Unfold"),
	content(c)
{
	addAsScriptEditListener();

	content->addRebuildListener(this);

	addAndMakeVisible(fuzzySearchBox = new TextEditor());
	fuzzySearchBox->addListener(this);
	fuzzySearchBox->setColour(TextEditor::ColourIds::backgroundColourId, Colours::white.withAlpha(0.2f));
	fuzzySearchBox->setFont(GLOBAL_FONT());
	fuzzySearchBox->setSelectAllWhenFocused(true);
	fuzzySearchBox->setColour(TextEditor::ColourIds::focusedOutlineColourId, Colour(SIGNAL_COLOUR));

	addAndMakeVisible(tree);

	tree.setDefaultOpenness(true);
	tree.setMultiSelectEnabled(true);
	tree.setColour(TreeView::backgroundColourId, Colours::transparentBlack);
	tree.setColour(TreeView::ColourIds::dragAndDropIndicatorColourId, Colour(SIGNAL_COLOUR));
	tree.setColour(TreeView::ColourIds::selectedItemBackgroundColourId, Colours::transparentBlack);
	tree.setColour(TreeView::ColourIds::linesColourId, Colours::black.withAlpha(0.1f));
	resetRootItem();

	tree.setRootItemVisible(false);

	addAndMakeVisible(undoButton);
	addAndMakeVisible(redoButton);
	addAndMakeVisible(foldButton);
	addAndMakeVisible(unfoldButton);
	undoButton.addListener(this);
	redoButton.addListener(this);
	foldButton.addListener(this);
	unfoldButton.addListener(this);

	undoButton.setLookAndFeel(&alaf);
	redoButton.setLookAndFeel(&alaf);
	foldButton.setLookAndFeel(&alaf);
	unfoldButton.setLookAndFeel(&alaf);

	tree.addMouseListener(this, true);

	static const unsigned char searchIcon[] = { 110, 109, 0, 0, 144, 68, 0, 0, 48, 68, 98, 7, 31, 145, 68, 198, 170, 109, 68, 78, 223, 103, 68, 148, 132, 146, 68, 85, 107, 42, 68, 146, 2, 144, 68, 98, 54, 145, 219, 67, 43, 90, 143, 68, 66, 59, 103, 67, 117, 24, 100, 68, 78, 46, 128, 67, 210, 164, 39, 68, 98, 93, 50, 134, 67, 113, 58, 216, 67, 120, 192, 249, 67, 83, 151,
		103, 67, 206, 99, 56, 68, 244, 59, 128, 67, 98, 72, 209, 112, 68, 66, 60, 134, 67, 254, 238, 144, 68, 83, 128, 238, 67, 0, 0, 144, 68, 0, 0, 48, 68, 99, 109, 0, 0, 208, 68, 0, 0, 0, 195, 98, 14, 229, 208, 68, 70, 27, 117, 195, 211, 63, 187, 68, 146, 218, 151, 195, 167, 38, 179, 68, 23, 8, 77, 195, 98, 36, 92, 165, 68, 187, 58,
		191, 194, 127, 164, 151, 68, 251, 78, 102, 65, 0, 224, 137, 68, 0, 0, 248, 66, 98, 186, 89, 77, 68, 68, 20, 162, 194, 42, 153, 195, 67, 58, 106, 186, 193, 135, 70, 41, 67, 157, 224, 115, 67, 98, 13, 96, 218, 193, 104, 81, 235, 67, 243, 198, 99, 194, 8, 94, 78, 68, 70, 137, 213, 66, 112, 211, 134, 68, 98, 109, 211, 138, 67,
		218, 42, 170, 68, 245, 147, 37, 68, 128, 215, 185, 68, 117, 185, 113, 68, 28, 189, 169, 68, 98, 116, 250, 155, 68, 237, 26, 156, 68, 181, 145, 179, 68, 76, 44, 108, 68, 16, 184, 175, 68, 102, 10, 33, 68, 98, 249, 118, 174, 68, 137, 199, 2, 68, 156, 78, 169, 68, 210, 27, 202, 67, 0, 128, 160, 68, 0, 128, 152, 67, 98, 163,
		95, 175, 68, 72, 52, 56, 67, 78, 185, 190, 68, 124, 190, 133, 66, 147, 74, 205, 68, 52, 157, 96, 194, 98, 192, 27, 207, 68, 217, 22, 154, 194, 59, 9, 208, 68, 237, 54, 205, 194, 0, 0, 208, 68, 0, 0, 0, 195, 99, 101, 0, 0 };

	
	searchPath.loadPathFromData(searchIcon, sizeof(searchIcon));
	searchPath.applyTransform(AffineTransform::rotation(float_Pi));

	searchPath.scaleToFit(4.0f, 12.0f, 16.0f, 16.0f, true);

	

	startTimer(500);


}

ScriptComponentList::~ScriptComponentList()
{
	fuzzySearchBox->removeListener(this);

	removeAsScriptEditListener();
	content->removeRebuildListener(this);

	tree.setRootItem(nullptr);
}

void ScriptComponentList::scriptComponentPropertyChanged(ScriptComponent* sc, Identifier /*idThatWasChanged*/, const var& /*newValue*/)
{
	if (sc != nullptr)
	{
		auto item = tree.findItemFromIdentifierString(sc->name.toString());

		if (item != nullptr)
		{
			item->repaintItem();
		}
	}
}

void ScriptComponentList::scriptComponentSelectionChanged()
{
	if (rootItem != nullptr)
	{
		rootItem->updateSelection(getScriptComponentEditBroadcaster()->getSelection());
	}
}

void ScriptComponentList::mouseUp(const MouseEvent& event)
{
	

	if(event.mods.isRightButtonDown())
	{
		auto b = getScriptComponentEditBroadcaster();

		enum PopupMenuOptions
		{
			CreateScriptVariableDeclaration = 1,
			CreateCustomCallbackDefinition,
			CopyProperties,
			PasteProperties,
			numOptions
		};

		PopupLookAndFeel plaf;
		PopupMenu m;
		m.setLookAndFeel(&plaf);

		m.addItem(PopupMenuOptions::CreateScriptVariableDeclaration, "Create script variable definition");
		m.addItem(PopupMenuOptions::CreateCustomCallbackDefinition, "Create custom callback definition");

		auto clipboardData = JSON::parse(SystemClipboard::getTextFromClipboard());

		const bool isSingleSelection = tree.getNumSelectedItems() == 1;

		m.addItem(PopupMenuOptions::CopyProperties, "Copy properties", isSingleSelection);
		m.addItem(PopupMenuOptions::PasteProperties, "Paste properties to selection", clipboardData.isObject());

		ValueTree pTree;
		
		if (isSingleSelection)
		{
			pTree = static_cast<ScriptComponentListItem*>(tree.getSelectedItem(0))->tree;

			m.addSectionHeader("Add new widget");
			m.addItem((int)ScriptEditHandler::Widgets::Knob, "Add new Slider");
			m.addItem((int)ScriptEditHandler::Widgets::Button, "Add new Button");
			m.addItem((int)ScriptEditHandler::Widgets::Table, "Add new Table");
			m.addItem((int)ScriptEditHandler::Widgets::ComboBox, "Add new ComboBox");
			m.addItem((int)ScriptEditHandler::Widgets::Label, "Add new Label");
			m.addItem((int)ScriptEditHandler::Widgets::Image, "Add new Image");
			m.addItem((int)ScriptEditHandler::Widgets::Viewport, "Add new Viewport");
			m.addItem((int)ScriptEditHandler::Widgets::Plotter, "Add new Plotter");
			m.addItem((int)ScriptEditHandler::Widgets::ModulatorMeter, "Add new ModulatorMeter");
			m.addItem((int)ScriptEditHandler::Widgets::Panel, "Add new Panel");
			m.addItem((int)ScriptEditHandler::Widgets::AudioWaveform, "Add new AudioWaveform");
			m.addItem((int)ScriptEditHandler::Widgets::SliderPack, "Add new SliderPack");
			m.addItem((int)ScriptEditHandler::Widgets::FloatingTile, "Add new FloatingTile");
		}
		

		ScriptComponentSelection componentListToUse = b->getSelection();

		const int result = m.show();

		switch (result)
		{
		
		case CreateScriptVariableDeclaration:
		{
			auto st = ScriptingApi::Content::Helpers::createScriptVariableDeclaration(componentListToUse);

			debugToConsole(dynamic_cast<Processor*>(content->getScriptProcessor()), String(b->getNumSelected()) + " script definitions created and copied to the clipboard");

			SystemClipboard::copyTextToClipboard(st);
			break;
		}
		case CreateCustomCallbackDefinition:
		{
			auto st = ScriptingApi::Content::Helpers::createCustomCallbackDefinition(componentListToUse);



			debugToConsole(dynamic_cast<Processor*>(content->getScriptProcessor()), String(b->getNumSelected()) + " callback definitions created and copied to the clipboard");

			SystemClipboard::copyTextToClipboard(st);
			break;
		}
		case CopyProperties:
		{
			if (tree.getNumSelectedItems() == 1)
			{
				auto sc = static_cast<ScriptComponentListItem*>(tree.getSelectedItem(0))->connectedComponent;

				if (sc != nullptr)
				{
					SystemClipboard::copyTextToClipboard(sc->getScriptObjectPropertiesAsJSON());
				}
			}

			
			break;
		}
		case PasteProperties:
		{
			ScriptingApi::Content::Helpers::pasteProperties(b->getSelection(), clipboardData);
			break;
		}
		ADD_WIDGET(ScriptEditHandler::Widgets::Knob, ScriptingApi::Content::ScriptSlider);
		ADD_WIDGET(ScriptEditHandler::Widgets::Button, ScriptingApi::Content::ScriptButton);
		ADD_WIDGET(ScriptEditHandler::Widgets::Label, ScriptingApi::Content::ScriptLabel);
		ADD_WIDGET(ScriptEditHandler::Widgets::AudioWaveform, ScriptingApi::Content::ScriptAudioWaveform);
		ADD_WIDGET(ScriptEditHandler::Widgets::ComboBox, ScriptingApi::Content::ScriptComboBox);
		ADD_WIDGET(ScriptEditHandler::Widgets::FloatingTile, ScriptingApi::Content::ScriptFloatingTile);
		ADD_WIDGET(ScriptEditHandler::Widgets::Image, ScriptingApi::Content::ScriptImage);
		ADD_WIDGET(ScriptEditHandler::Widgets::ModulatorMeter, ScriptingApi::Content::ModulatorMeter);
		ADD_WIDGET(ScriptEditHandler::Widgets::Plotter, ScriptingApi::Content::ScriptedPlotter);
		ADD_WIDGET(ScriptEditHandler::Widgets::Panel, ScriptingApi::Content::ScriptPanel);
		ADD_WIDGET(ScriptEditHandler::Widgets::SliderPack, ScriptingApi::Content::ScriptSliderPack);
		ADD_WIDGET(ScriptEditHandler::Widgets::Table, ScriptingApi::Content::ScriptTable);
		ADD_WIDGET(ScriptEditHandler::Widgets::Viewport, ScriptingApi::Content::ScriptedViewport);
		default:
			break;
		}

	}
}


void ScriptComponentList::mouseDoubleClick(const MouseEvent& e)
{
	auto i = dynamic_cast<ScriptComponentListItem*>(tree.getItemAt(e.getMouseDownY()));

	if (i != nullptr)
	{
		if (i->connectedComponent)
			ScriptingApi::Content::Helpers::gotoLocation(i->connectedComponent);
	}
}

void ScriptComponentList::paint(Graphics& g)
{
	g.setColour(Colour(0xff353535));

	auto r = fuzzySearchBox->getBounds().withLeft(0);

	g.fillRect(r);

	g.setColour(Colours::white.withAlpha(0.6f));
	g.fillPath(searchPath);
}

void ScriptComponentList::resetRootItem()
{
	auto v = content->getContentProperties();

	openState = tree.getOpennessState(true);

	tree.setRootItem(rootItem = new ScriptComponentListItem(v, undoManager, content, searchTerm));

	if (openState != nullptr)
	{
		tree.restoreOpennessState(*openState, false);
	}
}

void ScriptComponentList::resized()
{
	Rectangle<int> r(getLocalBounds().reduced(8));

	Rectangle<int> searchBox = r.removeFromTop(24);

	fuzzySearchBox->setBounds(searchBox.withLeft(24));

	Rectangle<int> buttons(r.removeFromBottom(22));
	undoButton.setBounds(buttons.removeFromLeft(60));
	buttons.removeFromLeft(6);
	redoButton.setBounds(buttons.removeFromLeft(60));
	buttons.removeFromLeft(6);
	foldButton.setBounds(buttons.removeFromLeft(60));
	buttons.removeFromLeft(6);
	unfoldButton.setBounds(buttons.removeFromLeft(60));
	buttons.removeFromLeft(6);

	r.removeFromBottom(4);
	tree.setBounds(r);
}

#undef ADD_WIDGET

bool ScriptComponentList::keyPressed(const KeyPress& key)
{
	if (key == KeyPress::deleteKey)
	{
		deleteSelectedItems();
		return true;
	}

	if (key == KeyPress::escapeKey)
	{
		getScriptComponentEditBroadcaster()->clearSelection(sendNotification);
		return true;
	}

	if (key == KeyPress('z', ModifierKeys::commandModifier, 0))
	{
		undoButton.triggerClick();
		return true;
	}

	if (key == KeyPress('z', ModifierKeys::commandModifier | ModifierKeys::shiftModifier, 0))
	{
		redoButton.triggerClick();
		return true;
	}
	if (key.getKeyCode() == 'J')
	{
		Array<var> list;

		for (int i = 0; i < tree.getNumSelectedItems(); i++)
		{
			auto t = static_cast<ScriptComponentListItem*>(tree.getSelectedItem(i))->tree;
			auto v = ValueTreeConverters::convertContentPropertiesToDynamicObject(t);
			list.add(v);
		}

		JSONEditor* editor = new JSONEditor(var(list));

		editor->setEditable(true);

		auto& tmpContent = content;

		auto callback = [tmpContent, this](const var& newData)
		{
			auto b = this->getScriptComponentEditBroadcaster();

			if (auto ar = newData.getArray())
			{
				auto selection = b->getSelection();

				jassert(ar->size() == selection.size());

				for (int i = 0; i < selection.size(); i++)
				{
					auto sc = selection[i];

					auto newJson = ar->getUnchecked(i);

					ScriptingApi::Content::Helpers::setComponentValueTreeFromJSON(content, sc->name, newJson);
				}

				content->updateAndSetLevel(ScriptingApi::Content::UpdateLevel::FullRecompile);
			}



			return;
		};

		editor->setCallback(callback, true);

		editor->setName("Editing JSON");

		editor->setSize(400, 400);

		findParentComponentOfClass<FloatingTile>()->showComponentInRootPopup(editor, this, getLocalBounds().getCentre());

		editor->grabKeyboardFocus();

		return true;
	}

	return Component::keyPressed(key);
}


} // namespace hise
