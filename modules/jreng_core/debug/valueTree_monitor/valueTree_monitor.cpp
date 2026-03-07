namespace jreng::debug
{
/*____________________________________________________________________________*/
ValueTreeMonitor::ValueTreeMonitorComponent::ValueTreeMonitorComponent()
    : layoutResizer (&layout, 1, false)
{
    layout.setItemLayout (0, -0.1, -0.9, -0.6);
    layout.setItemLayout (1, 5, 5, 5);
    layout.setItemLayout (2, -0.1, -0.9, -0.1);

    setSize (600, 600);
    addAndMakeVisible (treeView);
    addAndMakeVisible (propertyEditor);
    addAndMakeVisible (layoutResizer);

    treeView.setDefaultOpenness (true);
}

ValueTreeMonitor::ValueTreeMonitorComponent::~ValueTreeMonitorComponent()
{
    treeView.setRootItem (nullptr);
}

void ValueTreeMonitor::ValueTreeMonitorComponent::resized()
{
    juce::Component* comps[] = { &treeView, &layoutResizer, &propertyEditor };
    layout.layOutComponents (comps, 3, 0, 0, getWidth(), getHeight(), true, true);
}

void ValueTreeMonitor::ValueTreeMonitorComponent::setTree (juce::ValueTree newTree)
{
    if (! newTree.isValid())
    {
        treeView.setRootItem (nullptr);
    }
    else if (tree != newTree)
    {
        tree = newTree;
        rootItem = std::make_unique<Item> (&propertyEditor, tree);
        treeView.setRootItem (rootItem.get());
    }
}

/*____________________________________________________________________________*/

ValueTreeMonitor::ValueTreeMonitorComponent::PropertyEditor::PropertyEditor()
{
    noEditValue = "not editable";
}

void ValueTreeMonitor::ValueTreeMonitorComponent::PropertyEditor::setSource (juce::ValueTree& newSource)
{
    clear();

    tree = newSource;

    const int maxChars = 200;

    juce::Array<juce::PropertyComponent*> pc;

    for (int i = 0; i < tree.getNumProperties(); ++i)
    {
        const juce::Identifier name = tree.getPropertyName (i).toString();
        juce::Value v = tree.getPropertyAsValue (name, nullptr);
        juce::TextPropertyComponent* tpc;

        if (v.getValue().isObject())
        {
            tpc = new juce::TextPropertyComponent (noEditValue, name.toString(), maxChars, false);
            tpc->setEnabled (false);
        }
        else
        {
            tpc = new juce::TextPropertyComponent (v, name.toString(), maxChars, false);
        }

        pc.add (tpc);
    }

    addProperties (pc);
}

/*____________________________________________________________________________*/

ValueTreeMonitor::ValueTreeMonitorComponent::Item::Item (PropertyEditor* propertiesEditor, juce::ValueTree tree)
    : propertiesEditor (propertiesEditor)
    , t (tree)
{
    t.addListener (this);
}

ValueTreeMonitor::ValueTreeMonitorComponent::Item::~Item()
{
    clearSubItems();
}

bool ValueTreeMonitor::ValueTreeMonitorComponent::Item::mightContainSubItems()
{
    return t.getNumChildren() > 0;
}

void ValueTreeMonitor::ValueTreeMonitorComponent::Item::itemOpennessChanged (bool isNowOpen)
{
    if (isNowOpen)
        updateSubItems();
}

void ValueTreeMonitor::ValueTreeMonitorComponent::Item::updateSubItems()
{
    std::unique_ptr<juce::XmlElement> openness = getOpennessState();
    clearSubItems();
    int children = t.getNumChildren();

    for (int i = 0; i < children; ++i)
        addSubItem (new Item (propertiesEditor, t.getChild (i)));

    if (openness.get())
        restoreOpennessState (*openness.get());
}

//void ValueTreeMonitor::ValueTreeMonitorComponent::Item::paintItem (juce::Graphics& g, int w, int h)
//{
//    //    juce::FontOptions font { Fonts::InputMonoNarrowRegular };
//    juce::FontOptions font { 12.0f };
//
//    const float padding = 20.0f;
//
//    juce::String typeName = t.getType().toString();
//
//    const float nameWidth = juce::TextLayout::getStringWidth (font, typeName);
//    const float propertyX = padding + nameWidth;
//    auto& laf { dynamic_cast<juce::Component*> (this)->getLookAndFeel() };
//
//    g.setFont (font);
//    g.setColour (laf.findColour (juce::PropertyComponent::labelTextColourId));
//    g.drawText (t.getType().toString(), 0, 0, w, h, juce::Justification::left, false);
//
//    juce::AttributedString property;
//    juce::Colour idColour { juce::Colours::pink };
//    juce::Colour valueColour { juce::Colours::lime };
//
//    for (int i = 0; i < t.getNumProperties(); ++i)
//    {
//        const juce::Identifier name = t.getPropertyName (i).toString();
//        juce::String propertyValue = t.getProperty (name).toString();
//        property.append (" " + name.toString(), idColour);
//        property.append ("=", laf.findColour (juce::PropertyComponent::labelTextColourId));
//        property.append (propertyValue, valueColour);
//    }
//
//    juce::Rectangle<float> propArea { propertyX, 0, w - propertyX, static_cast<float> (h) };
//    property.setFont (font);
//    property.draw (g, propArea);
//}

void ValueTreeMonitor::ValueTreeMonitorComponent::Item::paintItem (juce::Graphics& g, int w, int h)
{
    juce::FontOptions font (12.0f);
    const float padding = 20.0f;

    const juce::String typeName = t.getType().toString();
    const float nameWidth = juce::TextLayout().getStringWidth(font, typeName);
    const float propertyX = padding + nameWidth;

    auto& laf = getOwnerView()->getLookAndFeel();

    g.setFont (font);
    g.setColour (laf.findColour (juce::PropertyComponent::labelTextColourId));
    g.drawText (typeName, 0, 0, w, h, juce::Justification::left, false);

    juce::AttributedString property;

    const juce::Colour idColour     { juce::Colour (0xFF00CBFF) };
    const juce::Colour numberColour { juce::Colours::lime };
    const juce::Colour stringColour { juce::Colours::pink };
    const juce::Colour eqColour     { laf.findColour (juce::PropertyComponent::labelTextColourId) };

    for (int i = 0; i < t.getNumProperties(); ++i)
    {
        const juce::Identifier name = t.getPropertyName (i);
        const juce::String propertyValue = t.getProperty (name).toString();

        // property name
        property.append (" " + name.toString(), idColour);

        // equals sign
        property.append ("=", eqColour);

        // decide value colour
        juce::Colour valueColour = stringColour;
        {
            auto trimmed = propertyValue.trim();
            bool hasDigit = trimmed.containsAnyOf ("0123456789");
            bool numericChars = trimmed.removeCharacters ("0123456789.-+eE").isEmpty();
            if (hasDigit && numericChars)
                valueColour = numberColour;
        }

        property.append (propertyValue, valueColour);
    }

    juce::Rectangle<float> propArea { propertyX, 0.0f, (float) (w - propertyX), (float) h };
    property.setFont (font);
    property.draw (g, propArea);
}



void ValueTreeMonitor::ValueTreeMonitorComponent::Item::itemSelectionChanged (bool isNowSelected)
{
    if (isNowSelected)
    {
        t.removeListener (this);
        propertiesEditor->setSource (t);
        t.addListener (this);
    }
}

/* Enormous list of ValueTree::Listener options... */
void ValueTreeMonitor::ValueTreeMonitorComponent::Item::valueTreePropertyChanged (juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& property)
{
#if JUCE_MODULE_AVAILABLE_juce_audio_processors
    const juce::MessageManagerLock mmLock;
#endif

    if (t != treeWhosePropertyHasChanged)
        return;

    t.removeListener (this);
    //            if (isSelected())
    //                propertiesEditor->setSource(t);
    repaintItem();
    t.addListener (this);
}

void ValueTreeMonitor::ValueTreeMonitorComponent::Item::valueTreeChildAdded (juce::ValueTree& parentTree, juce::ValueTree& childWhichHasBeenAdded)
{
    if (parentTree == t)
        updateSubItems();

    treeHasChanged();
}

void ValueTreeMonitor::ValueTreeMonitorComponent::Item::valueTreeChildRemoved (juce::ValueTree& parentTree, juce::ValueTree& childWhichHasBeenRemoved, int)
{
    if (parentTree == t)
        updateSubItems();

    treeHasChanged();
}
void ValueTreeMonitor::ValueTreeMonitorComponent::Item::valueTreeChildOrderChanged (juce::ValueTree& parentTreeWhoseChildrenHaveMoved, int, int)
{
    if (parentTreeWhoseChildrenHaveMoved == t)
        updateSubItems();

    treeHasChanged();
}
void ValueTreeMonitor::ValueTreeMonitorComponent::Item::valueTreeParentChanged (juce::ValueTree& treeWhoseParentHasChanged)
{
    treeHasChanged();
}
void ValueTreeMonitor::ValueTreeMonitorComponent::Item::valueTreeRedirected (juce::ValueTree& treeWhichHasBeenChanged)
{
    if (treeWhichHasBeenChanged == t)
        updateSubItems();

    treeHasChanged();
}

/* Works only if the ValueTree isn't updated between calls to getUniqueName. */
juce::String ValueTreeMonitor::ValueTreeMonitorComponent::Item::getUniqueName() const
{
    if (! t.getParent().isValid())
        return "1";

    return juce::String (t.getParent().indexOf (t));
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng::debug
