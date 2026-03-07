/*
 MIT License
  
 Copyright (c) 2018 Janos Buttgereit
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 */

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "build_info/jreng_build_info.h"
#include "console/jreng_console.h"
#include "fonts/Fonts_InputMonoNarrowRegular.h"
#include "fonts/Fonts_InputSansNarrowLight.h"
#include "valueTree_monitor/valueTree_monitor.h"

namespace jreng
{
/*____________________________________________________________________________*/

//==============================================================================

/**
write to variable name = value console with iostream
*/
#define cout(name) jreng::debug::Console::print (#name, (name))

//==============================================================================
/* ------------------------- ONLY CALLED WHEN DEBUG ------------------------- */
#if JUCE_DEBUG
namespace debug
{
/*____________________________________________________________________________*/

class Widget : public juce::ComponentListener
{
public:
    template<typename ProcessorType>
    Widget (juce::Component* editorToAdd,
            ProcessorType& processor,
            juce::ValueTree& state,
            juce::LookAndFeel* lookAndFeel,
            bool isUsingValueTreeMonitor = true,
            bool alwaysOnTop = true)
        : editor (editorToAdd)
        , isUsingMonitor (isUsingValueTreeMonitor)
    {
        console.reset (new Console);
        console->setLookAndFeel (lookAndFeel);
        auto desktop { juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea };
        auto x { desktop.getWidth() - console->getWidth() };
        auto y { desktop.getHeight() - console->getHeight() };
        auto w { console->getWidth() };
        auto h { console->getHeight() };

        if (isUsingMonitor)
            console->setBounds (x, y, w, h);
        else
            console->setBounds (x, y, w, toInt (desktop.getHeight() * 0.67f));

        console->setAlwaysOnTop (alwaysOnTop);

        if (isUsingMonitor)
        {
            monitor.reset (new ValueTreeMonitor (state));
            monitor->setLookAndFeel (lookAndFeel);
            monitor->setAlwaysOnTop (alwaysOnTop);
#if JUCE_MAC
            monitor->setBounds (x, 0, w, desktop.getHeight() - h - 56);
#elif JUCE_WINDOWS
            monitor->setBounds (x, 28, w, desktop.getHeight() - h - 56);
#endif
            monitor->setLookAndFeel (&editor->getLookAndFeel());
        }
#if JRENG_USING_BUILD_INFO
        buildInfo.reset (new BuildInfo (editor));
        buildInfo->setAlpha (0.5f);
#endif// JRENG_USING_BUILD_INFO

        editor->addComponentListener (this);
    }

    Widget (juce::Component* mainComponent,
            juce::ValueTree& state,
            bool alwaysOnTop = true)
        : editor (mainComponent)
    {
        console.reset (new Console());
        auto desktop { juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea };
        auto x { desktop.getWidth() - console->getWidth() };
        auto y { desktop.getHeight() - console->getHeight() };
        auto w { console->getWidth() };
        auto h { console->getHeight() };
        console->setBounds (x, y, w, h);
        monitor.reset (new ValueTreeMonitor (state));
#if JUCE_MAC
        monitor->setBounds (x, 0, w, desktop.getHeight() - h - 56);
        console->setAlwaysOnTop (alwaysOnTop);
        monitor->setAlwaysOnTop (alwaysOnTop);
#elif JUCE_WINDOWS
        monitor->setBounds (x, 28, w, desktop.getHeight() - h - 56);
#endif
        mainComponent->addComponentListener (this);
    }

    ~Widget() override = default;

    void componentMovedOrResized (juce::Component& component,
                                  bool wasMoved,
                                  bool wasResized) override
    {
#if JRENG_USING_BUILD_INFO
        if (buildInfo && wasResized)
        {
            auto editorBounds = editor->getLocalBounds();

            int y { 0 };

            if (auto panel { editor->findChildWithID (IDref::panel) })
                if (auto panelTop { panel->findChildWithID (IDref::panelTop) })
                    y = panelTop->getHeight();

            // Height for 2 lines: 10pt font ≈ 13px per line, 2 lines + spacing ≈ 32px
            // Y position: 30 pixels from top to avoid overlap with other UI
            buildInfo->setBounds (0, y, editorBounds.getWidth(), 35);
        }

        if (console)
            console->setLookAndFeel (&editor->getLookAndFeel());

        if (monitor)
            monitor->setLookAndFeel (&editor->getLookAndFeel());

        if (buildInfo)
            buildInfo->setLookAndFeel (&editor->getLookAndFeel());

#endif// JRENG_USING_BUILD_INFO
    }

    void setTree (juce::ValueTree& newTree)
    {
        if (monitor)
            monitor->setSource (newTree);
    }

    juce::ResizableWindow* getConsoleWindow()
    {
        return console.get();
    }

private:
#if JRENG_USING_BUILD_INFO
    std::unique_ptr<BuildInfo> buildInfo;
#endif// JRENG_USING_BUILD_INFO

    juce::Component* editor;

    std::unique_ptr<Console> console;
    std::unique_ptr<ValueTreeMonitor> monitor;

    bool isUsingMonitor;

    //==============================================================================

    // Trigger updates on events you already get without touching the app
    //    void componentVisibilityChanged (juce::Component& /*component*/) override { maybeUpdateLNF(); }
    //    void componentParentHierarchyChanged (juce::Component& /*component*/) override { maybeUpdateLNF(); }
    //    void componentBroughtToFront (juce::Component& /*component*/) override { maybeUpdateLNF(); }
    //
    //    void maybeUpdateLNF()
    //    {
    //        auto* current = &editor->getLookAndFeel();
    //        if (current != lastLNF)
    //        {
    //            lastLNF = current;
    //            if (console)
    //                console->setLookAndFeel (current);
    //            if (monitor)
    //                monitor->setLookAndFeel (current);
    //            if (buildInfo)
    //                buildInfo->setLookAndFeel(current);
    //        }
    //    }
    //
    //    juce::LookAndFeel* lastLNF = nullptr;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Widget)
};

//==============================================================================

/**
 * @brief Reports a fatal error and terminates the program.
 *
 * Always writes the message to stderr, then unconditionally
 * terminates the program. Safe to call from noexcept functions.
 *
 * @note If you hit this during development and see no output,
 *       it may be because the Debug Widget has already taken
 *       over the console. In that case, temporarily disable
 *       the widget creation (guarded by JUCE_DEBUG) to allow
 *       messages to appear in the IDE console.
 *
 * @param message The error message to print before termination.
 */
[[noreturn]] inline void error (const juce::String& message) noexcept
{
    std::cerr << message.toRawUTF8() << std::endl;
    std::abort();
}

#endif// JUCE_DEBUG
/**_____________________________END OF NAMESPACE______________________________*/
}// namespace debug
/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng
