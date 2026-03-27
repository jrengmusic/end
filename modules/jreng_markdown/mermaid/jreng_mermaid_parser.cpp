namespace jreng::Mermaid
{ /*____________________________________________________________________________*/

Parser::Parser()
{
    auto htmlTemplate { BinaryData::getString ("mermaid.html") };
    auto mermaidJs { BinaryData::getString ("mermaid.min.js") };

    engine.loadLibrary (mermaidJs, htmlTemplate, [this]
    {
        engine.execute ("typeof mermaid", [this] (const juce::String& result)
        {
            if (result == "object")
            {
                isMermaidReady = true;

                if (readyCallback)
                    readyCallback();
            }
        });
    });
}

void Parser::onReady (std::function<void()> callback)
{
    if (isMermaidReady)
    {
        callback();
    }
    else
    {
        readyCallback = std::move (callback);
    }
}

void Parser::convertToSVG (const juce::String& mermaidCode,
                            std::function<void (const juce::String&)> onResult)
{
    const auto escaped { mermaidCode.replace ("\\", "\\\\")
                                    .replace ("\"", "\\\"")
                                    .replace ("\n", "\\n")
                                    .replace ("\r", "\\r") };

    engine.execute ("window.lastResult = null; validateAndRender([\"" + escaped + "\"])",
                    [] (const juce::String&) {});

    engine.execute ("window.lastResult[0]",
                    [onResult] (const juce::String& result) { onResult (result); });
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng::Mermaid
