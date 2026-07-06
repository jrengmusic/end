/**
 * @file terminal/Mouse.h
 * @brief Mouse reporting encode path for one terminal session.
 */
#pragma once
#include <JuceHeader.h>
#include "terminal/Model.h"
#include "terminal/Session.h"

namespace terminal
{
/*____________________________________________________________________________*/

/**
 * @class Mouse
 * @brief juce::MouseListener owning ALL mouse reporting for one terminal::View.
 *
 * Registered on @c codeView ADDITIVELY by terminal::View's own
 * @c codeView->addMouseListener(&mouse, false) — jam::CodeView's own
 * ContentView still handles local text selection natively through JUCE's
 * normal dispatch; this registration only lets Mouse also see the same
 * events for xterm mouse-reporting purposes. terminal::View itself carries
 * no mouse overrides.
 *
 * Every override converts the event's pixel position to a cell via
 * jam::Cell::Point::fromPixel() (HARD RULE — no manual arithmetic), reads
 * mouseTracking/mouseSgr off @ref model at encode time (message-thread
 * lock-free reads), encodes via jam::terminal::Mouse::map(), and forwards a
 * non-empty result to session.writeInput(). A Shift-held event skips
 * reporting entirely — local selection (jam::CodeView's own ContentView) is
 * the only thing that happens.
 *
 * @par Thread context
 * MESSAGE THREAD only.
 */
class Mouse : public juce::MouseListener
{
public:
    /** @brief Constructs a Mouse bound to one session's Model/Session pair.
     *  @param modelRef    Session's owned terminal::Model — mouseTracking/
     *                     mouseSgr read source.
     *  @param sessionRef  Owning Session — writeInput() destination.
     */
    Mouse (Model& modelRef, Session& sessionRef) noexcept;

    /** @brief Updates the pixel cell size jam::Cell::Point::fromPixel() maps
     *  against — called by terminal::View's lookAndFeelChanged() every time
     *  cell metrics recompute.
     *  @note MESSAGE THREAD. */
    void setCellSize (int width, int height) noexcept;

    /** @internal Records @ref lastPressedButton from @p event's modifier
     *  state, then reports Type::press. */
    void mouseDown (const juce::MouseEvent& event) override;

    /** @internal Reports Type::release with @ref lastPressedButton — the
     *  ORIGINAL pressed button, since a released button is no longer
     *  present in @p event's own modifier state by the time this fires. */
    void mouseUp (const juce::MouseEvent& event) override;

    /** @internal Reports Type::drag with @ref lastPressedButton. */
    void mouseDrag (const juce::MouseEvent& event) override;

    /** @internal Reports Type::move — button is ignored for this Type
     *  (jam::terminal::Mouse::map()'s own contract). */
    void mouseMove (const juce::MouseEvent& event) override;

    /** @internal Wheel direction from @p wheel.deltaY sign — positive is
     *  wheel-up, matching the codebase's established trackpad/wheel
     *  convention. Zero-delta events (trackpad momentum settle) are not
     *  reported. */
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

private:
    /** @brief Shared cell-convert/read-mode/encode/forward body for every
     *  override above — see this class's own doc comment for the full
     *  Shift-gate sequence.
     *  @note MESSAGE THREAD. */
    void send (jam::terminal::Mouse::Type type, jam::terminal::Mouse::Button button, const juce::MouseEvent& event);

    /** @brief Session's owned terminal::Model — mouseTracking/mouseSgr reads. */
    Model& model;

    /** @brief Owning Session — writeInput() destination. */
    Session& session;

    /** @brief Pixel cell width — told via setCellSize(), never font-derived
     *  here (LookAndFeel/terminal::View own that computation). */
    int cellWidth { 0 };

    /** @brief Pixel cell height — told via setCellSize(). */
    int cellHeight { 0 };

    /** @brief The button held for the in-flight press/drag gesture —
     *  recorded in mouseDown() since a released button is no longer present
     *  in the MouseEvent's own modifier state by the time mouseUp() fires;
     *  send() needs the ORIGINAL button for the SGR release encoding
     *  (jam::terminal::Mouse::map()'s own contract). */
    jam::terminal::Mouse::Button lastPressedButton { jam::terminal::Mouse::Button::left };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Mouse)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
