namespace jreng::debug
{
/*____________________________________________________________________________*/
/*
 * (c) Credland Technical Limited.
 * MIT License
 *
 * JCF_DEBUG - Debugging helpers for JUCE.  Demo application.
 *
 * Don't forget to install the VisualStudio or Xcode debug scripts as
 * well.  These ensure that your IDEs debugger displays the contents
 * of ValueTrees, Strings and Arrays in a useful way!
 *
 *
 * Credland Technical Limited provide a range of consultancy and contract
 * services including:
 * - JUCE software development and support
 * - information security consultancy
 *
 * Contact via http://www.credland.net/
 */

class ValueTreeMonitor : public juce::ResizableWindow
{
public:
    /**
     @class Display a separate desktop window for viewed and editing a value
     tree's property fields.

     Instantate a ValueTreeMonitor instance, then call
     ValueTreeMonitor::setSource(ValueTree &) and it'll display your
     tree.

     For example:

     @code
     vd = new ValueTreeMonitor();
     vd->setSource(myTree);
     @endcode

     @note This code isn't pretty - it's for debugging, not production use!
     */
    ValueTreeMonitor()
        : juce::ResizableWindow ("ValueTree Monitor", true)
    {
        construct();
    }

    ValueTreeMonitor (juce::ValueTree& tree)
        : juce::ResizableWindow ("ValueTree Monitor", true)
    {
        construct();
        setSource (tree);
    }

    ~ValueTreeMonitor() override
    {
        main->setTree (juce::ValueTree());
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.brighter (0.05f));
    }

    /**
     * Call setSource(ValueTree&) to show a particular ValueTree in
     * the editor. If you attach all the ValueTree's in your program
     * to a common root, you'll be able to view the whole thing in
     * one editor.
     */
    void setSource (juce::ValueTree& treeToShow)
    {
        main->setTree (treeToShow);
    }

private:
    void construct()
    {
        main = std::make_unique<ValueTreeMonitorComponent>();
        setContentNonOwned (main.get(), true);
        setResizable (true, false);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
        setUsingNativeTitleBar (true);
    }

    //==============================================================================

    class ValueTreeMonitorComponent : public juce::Component
    {
    public:
        class PropertyEditor : public juce::PropertyPanel
        {
        public:
            PropertyEditor();
            void setSource (juce::ValueTree& newSource);

        private:
            juce::Value noEditValue;
            juce::ValueTree tree;

            //==============================================================================
            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PropertyEditor)
        };

        //==============================================================================

        class Item
            : public juce::TreeViewItem
            , public juce::ValueTree::Listener
        {
        public:
            Item (PropertyEditor* propertiesEditor, juce::ValueTree tree);
            ~Item();
            bool mightContainSubItems();
            void itemOpennessChanged (bool isNowOpen);
            void updateSubItems();
            void paintItem (juce::Graphics& g, int w, int h);
            void itemSelectionChanged (bool isNowSelected);
            /* Enormous list of ValueTree::Listener options... */
            void valueTreePropertyChanged (juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& property);
            void valueTreeChildAdded (juce::ValueTree& parentTree, juce::ValueTree& childWhichHasBeenAdded);
            void valueTreeChildRemoved (juce::ValueTree& parentTree, juce::ValueTree& childWhichHasBeenRemoved, int);
            void valueTreeChildOrderChanged (juce::ValueTree& parentTreeWhoseChildrenHaveMoved, int, int);
            void valueTreeParentChanged (juce::ValueTree& treeWhoseParentHasChanged);
            void valueTreeRedirected (juce::ValueTree& treeWhichHasBeenChanged);
            /* Works only if the ValueTree isn't updated between calls to getUniqueName. */
            juce::String getUniqueName() const;

        private:
            PropertyEditor* propertiesEditor;
            juce::ValueTree t;
            juce::Array<juce::Identifier> currentProperties;
            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Item)
        };

        //==============================================================================

        ValueTreeMonitorComponent();
        ~ValueTreeMonitorComponent() override;
        void resized() override;
        void setTree (juce::ValueTree newTree);

    public:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueTreeMonitorComponent)

        std::unique_ptr<Item> rootItem;
        juce::ValueTree tree;
        juce::TreeView treeView;
        PropertyEditor propertyEditor;
        juce::StretchableLayoutManager layout;
        juce::StretchableLayoutResizerBar layoutResizer;
    };

    std::unique_ptr<ValueTreeMonitorComponent> main;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueTreeMonitor)
};

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace jreng::debug
