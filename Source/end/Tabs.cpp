#include "end/Tabs.h"
#include "lookAndFeel/ENDLookAndFeel.h"

Tabs::Tabs (jam::Model& m, juce::ValueTree tabsState)
    : jam::Model::Component { *this, m, tabsState }
{
    setTabBarDepth (0);
    state.addListener (this);
}

Tabs::~Tabs() { state.removeListener (this); }

Panes& Tabs::add (jam::UUID uuid)
{
    auto* panes { new Panes (uuid, model) };

    // No PANE leaf exists yet at this point — this tab's own uuid is a
    // safe, non-magic placeholder for jam::button::Bar::addTab()'s
    // non-empty-name assertion; applyTabTitle() below resolves to empty
    // until a real title becomes derivable (rename, or the first pane's
    // terminal reporting cwd/foregroundProcess — valueTreePropertyChanged's
    // own bubbling reaction re-triggers applyTabTitle() at that point).
    addTab (uuid.toString(), juce::Colours::transparentBlack, panes, true);
    setCurrentTabIndex (getNumTabs() - 1);

    // Verb-direct placement (Attachment Contract) — this Component IS the
    // active Session's own adopted TABS tree, so the graft is a direct
    // appendChild onto this class's own state, no RAII token in between.
    state.appendChild (panes->state, nullptr);

    // Persist an inline rename (jam::button::Tab::showEditor(), wired to the tab button's
    // right-click handler — pre-existing rename UI, jam_Bar.cpp's own addTab()) onto
    // jam::ID::name, the rename slot getTitle() reads. dontSendNotification pushes
    // (applyTabTitle()) never touch onTextChange — only a genuine edit commit does.
    if (auto* tab { getBar().getTabButton (getNumTabs() - 1) })
    {
        tab->label.onTextChange = [panes, tab]
        {
            panes->state.setProperty (jam::ID::name, tab->label.getText(), nullptr);
        };
    }

    applyTabTitle (panes->state);
    lookAndFeelChanged();

    return *panes;
}

void Tabs::remove (jam::UUID uuid)
{
    for (int i { 0 }; i < getNumTabs(); ++i)
    {
        auto* panes { static_cast<Panes*> (getTabContentComponent (i)) };

        if (static_cast<int64_t> (panes->state.getProperty (jam::ID::id)) == uuid.value)
        {
            state.removeChild (panes->state, nullptr);
            removeTab (i);
            setCurrentTabIndex (juce::jmin (i, getNumTabs() - 1));
            lookAndFeelChanged();
            break;
        }
    }
}

Panes& Tabs::get (jam::UUID uuid)
{
    Panes* found { nullptr };

    for (int i { 0 }; i < getNumTabs() and found == nullptr; ++i)
    {
        auto* panes { static_cast<Panes*> (getTabContentComponent (i)) };

        if (static_cast<int64_t> (panes->state.getProperty (jam::ID::id)) == uuid.value)
            found = panes;
    }

    jassert (found != nullptr);
    return *found;
}

Panes* Tabs::getActivePanes() noexcept
{
    return static_cast<Panes*> (getCurrentContentComponent());
}

void Tabs::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    // Gated to the title-relevant property set only — jam::PaneManager
    // writes its own vocabulary (jam::ID::edge/position on RESIZER,
    // jam::ID::bounds on PANE) onto this SAME tab tree during every
    // layout() pass, and a broader reaction here self-retriggers
    // resized() -> layout() -> this listener, forever.
    if (property == jam::ID::name or property == jam::ID::cwd or property == ID::foregroundProcess)
        applyTabTitle (findAncestorTab (tree));
}

juce::String Tabs::getTitle (const juce::ValueTree& tabState)
{
    const juce::String rename { tabState.getProperty (jam::ID::name).toString() };
    juce::String title;

    if (rename.isNotEmpty())
    {
        title = rename;
    }
    else
    {
        // Per-tab last-focused-pane memory (Panes's own ID::focusedPane
        // param, 0 sentinel = none yet) — never the SESSIONS node's
        // singular focused_pane, which names the focused pane across ALL
        // tabs, not this tab's own remembered source.
        const jam::UUID sourceUuid { static_cast<int64_t> (
            tabState.getProperty (ID::focusedPane)) };
        const auto sourcePane {
            jam::Model::getChildWithID (tabState, juce::var (sourceUuid.value))
        };

        if (sourcePane.isValid())
        {
            const auto terminalState { sourcePane.getChildWithName (IDtype::terminal) };
            jassert (terminalState.isValid());

            const auto textState { terminalState.getChildWithName (jam::IDtype::text) };
            const juce::String foregroundProcess {
                textState.getProperty (ID::foregroundProcess).toString()
            };

            // cwd displayed as its own last path component — a full path is noise in a tab strip.
            title = foregroundProcess.isNotEmpty()
                        ? foregroundProcess
                        : juce::File (textState.getProperty (jam::ID::cwd).toString())
                              .getFileName();
        }
    }

    return title;
}

juce::ValueTree Tabs::findAncestorTab (juce::ValueTree tree)
{
    while (tree.isValid() and tree.getType() != IDtype::tab)
        tree = tree.getParent();

    return tree;
}

void Tabs::applyTabTitle (const juce::ValueTree& tabState)
{
    const auto title { getTitle (tabState) };

    for (int i { 0 }; i < getNumTabs(); ++i)
    {
        if (static_cast<Panes*> (getTabContentComponent (i))->getValueTree() == tabState)
        {
            setTabName (i, title);

            if (auto* tab { getBar().getTabButton (i) })
                tab->label.setText (title, juce::dontSendNotification);

            break;
        }
    }
}

void Tabs::lookAndFeelChanged()
{
    setTabBarDepth (lookAndFeel.getTabBarDepth (*this));
    setOrientation (lookAndFeel.getTabPosition());
}

void Tabs::currentTabChanged (int newCurrentTabIndex, const juce::String&)
{
    if (auto* panes { static_cast<Panes*> (getTabContentComponent (newCurrentTabIndex)) })
    {
        state.setProperty (ID::focusedTab, panes->getValueTree().getProperty (jam::ID::id), nullptr);

        // Guards the tab-creation path (Tabs::add()'s own setCurrentTabIndex()
        // fires this callback before its first pane exists) — every OTHER
        // caller (actual user tab-switch) always has at least one pane.
        if (panes->getPaneCount() > 0)
        {
            const jam::UUID rememberedPane { static_cast<int64_t> (
                panes->getValueTree().getProperty (ID::focusedPane)) };

            if (rememberedPane.value != 0)
                panes->get (rememberedPane).grabKeyboardFocus();
            else
                panes->get().grabKeyboardFocus();
        }
    }
}
