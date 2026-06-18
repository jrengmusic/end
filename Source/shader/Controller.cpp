#include "Controller.h"

namespace shader
{
/*____________________________________________________________________________*/

//==============================================================================
Controller::~Controller() { shutdownOpenGL(); }

//==============================================================================
void Controller::shutdownOpenGL() { openGLContext.detach(); }

void Controller::attach (juce::Component& component)
{
    shaders.addListener (this);

    openGLContext.setOpenGLVersionRequired (juce::OpenGLContext::openGL3_2);
    openGLContext.setComponentPaintingEnabled (true);
    openGLContext.setContinuousRepainting (true);
    openGLContext.setRenderer (this);
    openGLContext.attachTo (component);
}

void Controller::detach()
{
    shaders.removeListener (this);
    openGLContext.detach();
}

bool Controller::isAttached() const noexcept { return openGLContext.isAttached(); }

//==============================================================================
void Controller::newOpenGLContextCreated()
{
    initialise();
    loadShaders();
}

void Controller::renderOpenGL()
{
    juce::OpenGLHelpers::clear (juce::Colours::transparentBlack);

    //==============================================================================
    ++frameCounter;
}

void Controller::openGLContextClosing() { shutdown(); }

//==============================================================================
void Controller::initialise()
{
#if JUCE_MAC or JUCE_WINDOWS
    jam::BackgroundBlur::enableWindowTransparency();
#endif
}

void Controller::shutdown() {}

//==============================================================================
void Controller::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    loadShaders();
}

void Controller::loadShaders()
{
    // const auto& files { *config::File::Shaders::getInstance() };
    //
    // juce::String common { shaders.getProperty (ID::common).toString() };
    //
    // cout (shaders.toXmlString());
    //
    // jam::Model::forEachProperty (shaders,
    //                              [&files, this] (const juce::Identifier& id, const juce::var& var)
    //                              {
    //                                  if (id != ID::common)
    //                                  {
    //                                      if (files.contains (id.toString()))
    //                                      {
    //                                          auto p { std::make_unique<Program>() };
    //
    //                                          programs.insert_or_assign (id, std::move (p));
    //                                      }
    //                                  }
    //                              });
}

void Controller::buildQuad() {}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace shader
