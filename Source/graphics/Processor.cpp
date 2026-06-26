#include "Processor.h"

namespace graphics
{
/*____________________________________________________________________________*/

Processor::Processor()
{
    registerEvents();
    config.addListener (this);
    appModel.addListener (this);
}

Processor::~Processor()
{
    stopTimer();
    appModel.removeListener (this);
    config.removeListener (this);
    shutdownOpenGL();
}

//==============================================================================
void Processor::shutdownOpenGL() { context.detach(); }

void Processor::attach (juce::Component& component)
{
    context.setOpenGLVersionRequired (juce::OpenGLContext::openGL4_1);
    context.setMultisamplingEnabled (true);
    context.setRenderer (this);
    context.attachTo (component);
}

void Processor::detach() { context.detach(); }

bool Processor::isAttached() const noexcept { return context.isAttached(); }

//==============================================================================
void Processor::newOpenGLContextCreated()
{
#if JUCE_MAC or JUCE_WINDOWS
    jam::BackgroundBlur::enableWindowTransparency();
#endif

    quad = std::make_unique<Quad>();
    compositor.prepare (context);
    refreshParameters();
}

void Processor::renderOpenGL() { compositor.process (*quad); }

void Processor::openGLContextClosing()
{
    compositor.reset();
    quad.reset();
}

void Processor::timerCallback() { context.triggerRepaint(); }

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
                                      context.executeOnGLThread (
                                          [this] (juce::OpenGLContext&)
                                          {
                                              compositor.loadShaders (IDtype::background);

                                              auto* comp { context.getTargetComponent() };
                                              jassert (comp != nullptr);

                                              auto scale { context.getRenderingScale() };
                                              compositor.resize (
                                                  juce::roundToInt (comp->getWidth() * scale),
                                                  juce::roundToInt (comp->getHeight() * scale));
                                          },
                                          false);
                                  });

    events.add<const juce::var&> (ID::postProcessing,
                                  [this] (const juce::var&)
                                  {
                                      context.executeOnGLThread (
                                          [this] (juce::OpenGLContext&)
                                          {
                                              compositor.loadShaders (IDtype::postProcessing);

                                              auto* comp { context.getTargetComponent() };
                                              jassert (comp != nullptr);

                                              auto scale { context.getRenderingScale() };
                                              compositor.resize (
                                                  juce::roundToInt (comp->getWidth() * scale),
                                                  juce::roundToInt (comp->getHeight() * scale));
                                          },
                                          false);
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
            context.executeOnGLThread (
                [this, width, height] (juce::OpenGLContext&)
                {
                    const auto scale { context.getRenderingScale() };
                    compositor.resize (juce::roundToInt (width * scale),
                                       juce::roundToInt (height * scale));
                },
                false);
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

                                      auto screen { compositor.getScreenSize() };

                                      if (screen.x > 0 and screen.y > 0)
                                      {
                                          context.executeOnGLThread (
                                              [this, screen] (juce::OpenGLContext&)
                                              {
                                                  compositor.resize (screen.x, screen.y);
                                              },
                                              false);
                                      }
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
