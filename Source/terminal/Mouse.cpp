#include "terminal/Mouse.h"

namespace terminal
{
/*____________________________________________________________________________*/

Mouse::Mouse (Model& modelRef, Session& sessionRef) noexcept
    : model (modelRef)
    , session (sessionRef)
{
}

void Mouse::setCellSize (int width, int height) noexcept
{
    cellWidth = width;
    cellHeight = height;
}

void Mouse::send (jam::terminal::Mouse::Type type, jam::terminal::Mouse::Button button, const juce::MouseEvent& event)
{
    // Shift-held skips reporting entirely — local selection (jam::CodeView's
    // own ContentView, reached through the additive mouse-listener
    // registration) is the only thing that happens.
    if (not event.mods.isShiftDown())
    {
        const auto cell { jam::Cell::Point::fromPixel (event.getPosition(), cellWidth, cellHeight) };

        const int tracking { model.getValue (jam::IDtype::modes, jam::ID::mouseTracking) };
        const bool sgr { model.getValue (jam::IDtype::modes, jam::ID::mouseSgr) };

        const auto sequence { jam::terminal::Mouse::map (
            type, button, cell.x, cell.y, event.mods, tracking, sgr) };

        if (not sequence.isEmpty())
            session.writeInput (sequence.getData(), static_cast<int> (sequence.getSize()));
    }
}

void Mouse::mouseDown (const juce::MouseEvent& event)
{
    lastPressedButton = event.mods.isRightButtonDown()    ? jam::terminal::Mouse::Button::right
                        : event.mods.isMiddleButtonDown() ? jam::terminal::Mouse::Button::middle
                                                          : jam::terminal::Mouse::Button::left;

    send (jam::terminal::Mouse::Type::press, lastPressedButton, event);
}

void Mouse::mouseUp (const juce::MouseEvent& event)
{
    send (jam::terminal::Mouse::Type::release, lastPressedButton, event);
}

void Mouse::mouseDrag (const juce::MouseEvent& event)
{
    send (jam::terminal::Mouse::Type::drag, lastPressedButton, event);
}

void Mouse::mouseMove (const juce::MouseEvent& event)
{
    // Button is ignored for Type::move (jam::terminal::Mouse::map()'s own
    // contract) — the value passed here is never read.
    send (jam::terminal::Mouse::Type::move, jam::terminal::Mouse::Button::left, event);
}

void Mouse::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (wheel.deltaY != 0.0f)
    {
        const auto button { wheel.deltaY > 0.0f ? jam::terminal::Mouse::Button::wheelUp
                                                : jam::terminal::Mouse::Button::wheelDown };

        send (jam::terminal::Mouse::Type::wheel, button, event);
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
