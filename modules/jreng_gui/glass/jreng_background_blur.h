#if JUCE_MAC || JUCE_WINDOWS

namespace jreng
{
/*____________________________________________________________________________*/

struct BackgroundBlur
{
    enum class Type
    {
#if JUCE_MAC
        coreGraphics,
        nsVisualEffect
#elif JUCE_WINDOWS
        dwmGlass
#endif
    };

#if JUCE_MAC
    static const bool isCoreGraphicsAvailable();
    static const bool
    apply (juce::Component* component, float blurRadius, juce::Colour tint, Type type = Type::coreGraphics);
    static const bool enableGLTransparency();
#elif JUCE_WINDOWS
    static const bool isDwmAvailable();
    static const bool
    apply (juce::Component* component, float blurRadius, juce::Colour tint, Type type = Type::dwmGlass);
    static const bool enableGLTransparency();
#endif
    static void setCloseCallback (std::function<void()> callback);
    static void hideWindowButtons (juce::Component* component);

private:
#if JUCE_MAC
    static const bool applyBackgroundBlur (juce::Component* component, float blurRadius, juce::Colour tint);
    static const bool applyNSVisualEffect (juce::Component* component, float blurRadius, juce::Colour tint);
#elif JUCE_WINDOWS
    static const bool applyDwmGlass (juce::Component* component, float blurRadius, juce::Colour tint);
#endif
};

/**_____________________________END_OF_NAMESPACE______________________________*/
}// namespace jreng

#endif
