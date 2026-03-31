/**
 * @file jreng_glass_component.cpp
 * @brief Implementation of GlassComponent — auto-blurring JUCE Component.
 *
 * @par Blur Deferral
 * The native peer is not available at construction time.  The first call to
 * @c visibilityChanged() sets @c blurApplied and posts an async update.
 * @c handleAsyncUpdate() then runs on the message thread where the peer is
 * guaranteed to exist.
 *
 * @see jreng_glass_component.h
 * @see BackgroundBlur
 */

namespace jreng
{
/*____________________________________________________________________________*/

/**
 * @brief Default constructor — no-op; blur is deferred until first show.
 */
GlassComponent::GlassComponent() {}

/**
 * @brief Schedules blur application on the first visibility change.
 *
 * Sets @c blurApplied to @c true before posting the async update to prevent
 * a second trigger if the component is hidden and shown again before the
 * message thread processes the update.
 */
void GlassComponent::visibilityChanged()
{
    if (! blurApplied)
    {
        blurApplied = true;
        triggerAsyncUpdate();
    }
}

/**
 * @brief Applies a 20 pt background blur via BackgroundBlur::enable().
 *
 * Runs on the JUCE message thread after @c triggerAsyncUpdate().  The fixed
 * radius of 20 pt matches the default glassmorphism aesthetic.  For a
 * configurable radius, use GlassWindow instead.
 *
 * @see BackgroundBlur::enable()
 * @see GlassWindow
 */
void GlassComponent::handleAsyncUpdate()
{
    BackgroundBlur::enable (this, 20.0f, juce::Colours::transparentBlack);
}

/**_____________________________END_OF_NAMESPACE______________________________*/
} /** namespace jreng */
