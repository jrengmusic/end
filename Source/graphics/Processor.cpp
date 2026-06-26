#include "Processor.h"

namespace graphics
{
/*____________________________________________________________________________*/

Processor::Processor()
{
    compositor.reportError = [this] (const juce::String& msg) { appModel.setMessage (msg); };
    registerEvents();
    config.addListener (this);
    appModel.addListener (this);
}

Processor::~Processor()
{
    stopTimer();
    appModel.removeListener (this);
    config.removeListener (this);
    compositor.detach();
}

//==============================================================================
void Processor::attach (juce::Component& component)
{
    compositor.attach (*this, component);
}

void Processor::detach() { compositor.detach(); }

bool Processor::isAttached() const noexcept { return compositor.isAttached(); }

//==============================================================================
void Processor::newOpenGLContextCreated()
{
#if JUCE_MAC or JUCE_WINDOWS
    jam::BackgroundBlur::enableWindowTransparency();
#endif

    compositor.prepare();
    refreshParameters();
}

void Processor::renderOpenGL() { compositor.process(); }

void Processor::openGLContextClosing() { compositor.reset(); }

void Processor::timerCallback() { compositor.triggerRepaint(); }

//==============================================================================
void Processor::parameterChanged (const juce::Identifier& id, const juce::var& newValue)
{
    if (events.contains (id))
        events.get (id, newValue);
}

//==============================================================================
void Processor::refreshParameters()
{
    const auto& graphics { config.getChildWithName (IDtype::graphics) };

    jam::Model::forEachProperty (graphics,
                                 [this] (const juce::Identifier& id, const juce::var& newValue)
                                 {
                                     parameterChanged (id, newValue);
                                 });
}

void Processor::registerEvents()
{
    events.add<const juce::var&> (ID::background,
                                  [this] (const juce::var&)
                                  {
                                      auto shaderTree { config.getChildWithName (IDtype::background) };
                                      compositor.loadShaders (shaderTree);
                                  });

    events.add<const juce::var&> (ID::size,
                                  [this] (const juce::var& newValue)
                                  {
                                      end::Size size { static_cast<int> (newValue) };
                                      auto [w, h] = size;
                                      resizer.set (IDtype::view, w, h);
                                  });

    resizer.addTrigger (
        juce::Identifier { jam::ID::stop },
        [this] (int width, int height)
        {
            compositor.resize (width, height);
        });

    events.add<const juce::var&> (ID::frameRate,
                                  [this] (const juce::var& newValue)
                                  {
                                      int fps { static_cast<int> (newValue) };
                                      startTimerHz (fps);
                                      compositor.setFrameRate (fps);
                                  });

    events.add<const juce::var&> (ID::resolutionScale,
                                  [this] (const juce::var& newValue)
                                  {
                                      compositor.setResolutionScale (
                                          static_cast<float> (newValue));
                                  });

    events.add<const juce::var&> (ID::filter,
                                  [this] (const juce::var& newValue)
                                  {
                                      compositor.setTextureFilter (
                                          end::Filter::get (newValue.toString()));
                                  });

    events.add<const juce::var&> (ID::backgroundOpacity,
                                  [this] (const juce::var& newValue)
                                  {
                                      compositor.setOpacity (
                                          static_cast<float> (newValue));
                                  });
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace graphics
