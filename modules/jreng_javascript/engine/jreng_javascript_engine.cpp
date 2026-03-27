namespace jreng
{ /*____________________________________________________________________________*/

struct JavaScriptEngine::Impl : private juce::WebBrowserComponent
{
    Impl() = default;

    ~Impl() override
    {
        if (tempHtml.existsAsFile())
            tempHtml.deleteFile();
    }

    void load (const juce::String& javascriptSource,
               const juce::String& htmlTemplate,
               std::function<void()> onReadyCallback)
    {
        readyCallback = std::move (onReadyCallback);
        isReady = false;

        auto html { htmlTemplate.replace ("%%LIBRARY%%", javascriptSource) };
        tempHtml = juce::File::createTempFile ("jreng_js.html");
        tempHtml.replaceWithText (html);
        goToURL (juce::URL (tempHtml).toString (false));
    }

    void evaluate (const juce::String& code,
                   std::function<void (const juce::String&)> callback)
    {
        jassert (isReady);

        evaluateJavascript (code, [callback] (const WebBrowserComponent::EvaluationResult& result)
        {
            if (auto* r { result.getResult() })
                callback (r->toString());
        });
    }

    juce::Component* getViewComponent() noexcept
    {
        return static_cast<juce::Component*> (this);
    }

    void pageFinishedLoading (const juce::String& url) override
    {
        juce::ignoreUnused (url);
        isReady = true;

        if (readyCallback != nullptr)
        {
            readyCallback();
            readyCallback = nullptr;
        }
    }

    juce::File tempHtml;
    bool isReady { false };
    std::function<void()> readyCallback;
};

// Default HTML template — minimal shell
static const juce::String defaultHtmlTemplate {
    "<!DOCTYPE html>\n"
    "<html><head><meta charset=\"utf-8\">\n"
    "<script>%%LIBRARY%%</script>\n"
    "</head><body></body></html>\n"
};

JavaScriptEngine::JavaScriptEngine() = default;
JavaScriptEngine::~JavaScriptEngine() = default;

void JavaScriptEngine::loadLibrary (const juce::String& javascriptSource,
                                    std::function<void()> onReady)
{
    loadLibrary (javascriptSource, defaultHtmlTemplate, std::move (onReady));
}

void JavaScriptEngine::loadLibrary (const juce::String& javascriptSource,
                                    const juce::String& htmlTemplate,
                                    std::function<void()> onReady)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (impl == nullptr)
        impl = std::make_unique<Impl>();

    impl->load (javascriptSource, htmlTemplate, std::move (onReady));
}

void JavaScriptEngine::execute (const juce::String& code,
                                std::function<void (const juce::String&)> callback)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    jassert (impl != nullptr);

    if (impl != nullptr)
        impl->evaluate (code, std::move (callback));
}

bool JavaScriptEngine::isReady() const noexcept
{
    bool result { false };

    if (impl != nullptr)
        result = impl->isReady;

    return result;
}

juce::Component* JavaScriptEngine::getView() noexcept
{
    juce::Component* result { nullptr };

    if (impl != nullptr)
        result = impl->getViewComponent();

    return result;
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace jreng
