namespace jreng
{ /*____________________________________________________________________________*/

/**
 * @class JavaScriptEngine
 * @brief Headless JavaScript execution engine using platform OS WebView.
 *
 * Wraps juce::WebBrowserComponent via composition. Loads any JS library
 * as a string, executes arbitrary code, returns string results via callback.
 *
 * Two consumption modes:
 * - String extraction: execute() returns result via callback. WebView invisible.
 * - Visual rendering: getView() returns juce::Component* for canvas-based output.
 *
 * Lazy: WebBrowserComponent created on first loadLibrary() call. No cost when unused.
 *
 * @note MESSAGE THREAD — all methods.
 */
class JavaScriptEngine
{
public:
    JavaScriptEngine();
    ~JavaScriptEngine();

    /**
     * @brief Loads a JS library with auto-generated minimal HTML shell.
     * @param javascriptSource  The complete JS library source code as a string.
     * @param onReady           Called when the library is loaded and ready for execution.
     * @note MESSAGE THREAD.
     */
    void loadLibrary (const juce::String& javascriptSource,
                      std::function<void()> onReady);

    /**
     * @brief Loads a JS library with a custom HTML template.
     * @param javascriptSource  The complete JS library source code as a string.
     * @param htmlTemplate      HTML template with %%LIBRARY%% placeholder for JS injection.
     * @param onReady           Called when the page is loaded and ready.
     * @note MESSAGE THREAD.
     */
    void loadLibrary (const juce::String& javascriptSource,
                      const juce::String& htmlTemplate,
                      std::function<void()> onReady);

    /**
     * @brief Executes JavaScript code and returns the result as a string.
     * @param code      The JS code to execute.
     * @param callback  Called with the string result of the evaluation.
     * @note MESSAGE THREAD.
     */
    void execute (const juce::String& code,
                  std::function<void (const juce::String&)> callback);

    /**
     * @brief Returns true when the engine has loaded a library and is ready.
     * @note MESSAGE THREAD.
     */
    bool isReady() const noexcept;

    /**
     * @brief Returns the WebView as a juce::Component for visual rendering.
     *
     * Caller adds to component hierarchy when JS produces visual output
     * (canvas, SVG DOM, etc.). Returns nullptr before loadLibrary().
     *
     * @return Pointer to the internal component, or nullptr if not yet created.
     * @note MESSAGE THREAD.
     */
    juce::Component* getView() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JavaScriptEngine)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace jreng
