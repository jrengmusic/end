#include "terminal/Input.h"

namespace terminal
{
/*____________________________________________________________________________*/

Input::Input (Model& modelRef, Session& sessionRef) noexcept
    : model (modelRef)
    , session (sessionRef)
{
}

bool Input::sendKeyPress (const juce::KeyPress& key) noexcept
{
    const bool applicationCursor { model.getValue (jam::IDtype::modes, jam::ID::applicationCursor) };

    const int activeScreen { model.getValue (jam::IDtype::session, jam::ID::activeScreen) };
    const auto& screenTag { activeScreen == jam::terminal::Screen::alternate
                                ? jam::IDtype::alternate
                                : jam::IDtype::normal };
    const uint32_t keyboardFlags { static_cast<uint32_t> (
        static_cast<int> (model.getValue (screenTag, jam::ID::keyboardFlags))) };

    const auto sequence { jam::terminal::Keyboard::map (key, applicationCursor, keyboardFlags) };
    bool consumed { false };

    if (sequence.isNotEmpty())
    {
        session.writeInput (sequence.toRawUTF8(), static_cast<int> (sequence.getNumBytesAsUTF8()));
        consumed = true;
    }

    return consumed;
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
