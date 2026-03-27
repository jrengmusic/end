namespace jreng::Mermaid
{ /*____________________________________________________________________________*/

class Parser
{
public:
    Parser();

    void onReady (std::function<void()> callback);

    void convertToSVG (const juce::String& mermaidCode,
                       std::function<void (const juce::String&)> onResult);

    juce::Component* getView() noexcept { return engine.getView(); }

private:
    jreng::JavaScriptEngine engine;
    bool isMermaidReady { false };
    std::function<void()> readyCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Parser)
};

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng::Mermaid
