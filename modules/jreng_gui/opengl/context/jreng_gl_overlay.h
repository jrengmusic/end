#pragma once

namespace jreng
{
/*____________________________________________________________________________*/

class GLOverlay
    : public juce::Component
    , private juce::ComponentListener
{
public:
    GLOverlay()
    {
        setOpaque (false);
        setInterceptsMouseClicks (false, false);
    }

    ~GLOverlay() override
    {
        if (targetComponent != nullptr)
            targetComponent->removeComponentListener (this);

        renderer.reset();
    }

    void initialise (juce::Component& target)
    {
        targetComponent = &target;
        targetComponent->addComponentListener (this);

        renderer = std::make_unique<GLRenderer>();
        renderer->attachTo (*this);

        syncWithTarget();
    }

    void setRenderables (GLRenderer::ComponentIterator iter)
    {
        if (renderer != nullptr)
            renderer->setRenderables (std::move (iter));
    }

    void triggerRepaint()
    {
        if (renderer != nullptr)
            renderer->triggerRepaint();
    }

    void setClippingMask (const juce::Image& mask) noexcept
    {
        if (renderer != nullptr)
            renderer->setClippingMask (mask);
    }

    juce::Component* getTarget() const noexcept { return targetComponent; }

private:
    void componentMovedOrResized (juce::Component&, bool, bool) override
    {
        syncWithTarget();
    }

    void syncWithTarget()
    {
        if (targetComponent != nullptr)
        {
            setBounds (targetComponent->getBounds());
            setTransform (targetComponent->getTransform());
            toFront (false);
            triggerRepaint();
        }
    }

    std::unique_ptr<GLRenderer> renderer;
    juce::Component* targetComponent { nullptr };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLOverlay)
};

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng
