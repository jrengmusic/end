/**
 * @file jreng_child_window.mm
 * @brief macOS implementation of ChildWindow — NSWindow child attachment.
 */

#if JUCE_MAC

namespace jreng
{
/*____________________________________________________________________________*/

bool ChildWindow::attach (juce::Component* child, juce::Component* parent)
{
    bool result { false };

    if (child != nullptr and parent != nullptr)
    {
        auto* childPeer  { child->getPeer() };
        auto* parentPeer { parent->getPeer() };

        if (childPeer != nullptr and parentPeer != nullptr)
        {
            NSView* childView  { (NSView*) childPeer->getNativeHandle() };
            NSView* parentView { (NSView*) parentPeer->getNativeHandle() };

            NSWindow* childWindow  { [childView window] };
            NSWindow* parentWindow { [parentView window] };

            if (childWindow != nil and parentWindow != nil)
            {
                // Mark as transient so window managers (PaperWM, etc.) ignore it.
                [childWindow setCollectionBehavior:NSWindowCollectionBehaviorTransient
                                                 | NSWindowCollectionBehaviorFullScreenAuxiliary];

                [parentWindow addChildWindow:childWindow ordered:NSWindowAbove];
                result = true;
            }
        }
    }

    return result;
}

bool ChildWindow::detach (juce::Component* child)
{
    bool result { false };

    if (child != nullptr)
    {
        auto* childPeer { child->getPeer() };

        if (childPeer != nullptr)
        {
            NSView* childView { (NSView*) childPeer->getNativeHandle() };
            NSWindow* childWindow { [childView window] };

            if (childWindow != nil)
            {
                NSWindow* parentWindow { [childWindow parentWindow] };

                if (parentWindow != nil)
                {
                    [parentWindow removeChildWindow:childWindow];
                    result = true;
                }
            }
        }
    }

    return result;
}

/**_____________________________END_OF_NAMESPACE______________________________*/
}// namespace jreng

#endif
