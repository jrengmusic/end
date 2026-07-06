/**
 * @file terminal/Input.h
 * @brief Keyboard input encode path for one terminal session.
 */
#pragma once
#include <JuceHeader.h>
#include "terminal/Model.h"
#include "terminal/Session.h"

namespace terminal
{
/*____________________________________________________________________________*/

/**
 * @class Input
 * @brief Owns the keyboard encode path for one terminal::View.
 *
 * Reads applicationCursor (MODES) and the active screen's keyboardFlags
 * straight off @ref model at decode time (message-thread tree/param reads —
 * lock-free, the same Direction A read shape every other MODES/screen
 * consumer in this codebase uses), decodes the JUCE key event via
 * jam::terminal::Keyboard::map(), and forwards the resulting byte sequence
 * straight to @c session.writeInput() — keystrokes bypass the Model
 * entirely; jam::TextModel is mutated ONLY by Session::drain().
 *
 * @par Thread context
 * MESSAGE THREAD only — terminal::View::keyPressed() is the sole caller.
 */
class Input
{
public:
    /** @brief Constructs an Input bound to one session's Model/Session pair.
     *  @param modelRef    Session's owned terminal::Model — MODES/active-
     *                     screen state source.
     *  @param sessionRef  Owning Session — writeInput() destination.
     */
    Input (Model& modelRef, Session& sessionRef) noexcept;

    /** @brief Decodes @p key via jam::terminal::Keyboard::map()
     *  (applicationCursor + keyboardFlags read from @ref model's MODES/
     *  active-screen state at decode time) and sends the resulting byte
     *  sequence straight to session.writeInput().
     *  @param key  The JUCE key event to decode.
     *  @return true when the key produced a non-empty sequence (consumed);
     *          false otherwise (JUCE falls through to any other handler).
     *  @note MESSAGE THREAD.
     */
    bool sendKeyPress (const juce::KeyPress& key) noexcept;

private:
    /** @brief Session's owned terminal::Model — MODES/active-screen reads. */
    Model& model;

    /** @brief Owning Session — writeInput() destination. */
    Session& session;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Input)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
