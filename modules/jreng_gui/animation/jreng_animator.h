#pragma once

namespace jreng
{ /*____________________________________________________________________________*/

struct Animator
{
    struct Duration
    {
        static const int fadeIn { 150 };
        static const int fadeOut { 2 * fadeIn };
    };

    static void toggleFade (juce::Component* component,
                            bool shouldBeVisible,
                            int fadeInTimeMs = Duration::fadeIn,
                            int fadeOutTimeMs = Duration::fadeOut)
    {
        if (shouldBeVisible)
        {
            juce::Desktop::getInstance().getAnimator().fadeIn (component, fadeInTimeMs);
            component->setVisible (shouldBeVisible);
        }
        else
        {
            juce::Desktop::getInstance().getAnimator().fadeOut (component, fadeOutTimeMs);
            component->setVisible (shouldBeVisible);
        }
    }

    template<typename ComponentType>
    static void toggleFade (const std::unique_ptr<ComponentType>& component,
                            bool shouldBeVisible,
                            int fadeInTimeMs = Duration::fadeIn,
                            int fadeOutTimeMs = Duration::fadeOut)
    {
        toggleFade (component.get(), shouldBeVisible, fadeInTimeMs, fadeOutTimeMs);
    }

    static void toggleCrossFade (juce::Component* firstComponent,
                                 juce::Component* secondComponent,
                                 bool toggle,
                                 int durationMs = Duration::fadeOut)
    {
        if (toggle)
        {
            juce::Desktop::getInstance().getAnimator().fadeOut (secondComponent, durationMs);
            secondComponent->setVisible (not toggle);

            if (not juce::Desktop::getInstance().getAnimator().isAnimating())
            {
                juce::Desktop::getInstance().getAnimator().fadeIn (firstComponent, durationMs);
                firstComponent->setVisible (toggle);
            }
        }
        else
        {
            juce::Desktop::getInstance().getAnimator().fadeOut (firstComponent, durationMs);
            firstComponent->setVisible (not toggle);

            if (not juce::Desktop::getInstance().getAnimator().isAnimating())
            {
                juce::Desktop::getInstance().getAnimator().fadeIn (secondComponent, durationMs);
                secondComponent->setVisible (toggle);
            }
        }
    }
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace jreng
