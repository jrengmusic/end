#pragma once

namespace jreng
{
/*____________________________________________________________________________*/
// ============================================================================
// MACROS are evil. Yet, in the pursuit of one source of truth for convenience
// and consistency in the land of C++, this necessary horror spares us from
// repeating ourselves. May God forgive our sins.
//
// JRENG!
// ============================================================================

#define IDENTIFIER_UI_COMPONENTS(X) \
    X(component,        "component") \
    X(desc,             "desc") \
    X(comboBox,         "comboBox") \
    X(background,       "background") \
    X(primary,          "primary") \
    X(toolbar,          "toolbar") \
    X(label,            "label") \
    X(button,           "button") \
    X(toggle,           "toggle") \
    X(toggleMod,        "toggleMod") \
    X(toggleOverlay,    "toggleOverlay") \
    X(knob,             "knob") \
    X(knobShadow,       "knobShadow") \
    X(knobMarker,       "knobMarker") \
    X(slider,           "slider") \
    X(fader,            "fader") \
    X(faderOverlay,     "faderOverlay") \
    X(frequencyTextBox, "frequencyTextBox") \
    X(overlay,          "overlay") \
    X(images,           "images") \
    X(image,            "image") \
    X(imageOverlay,     "imageOverlay") \
    X(animation,        "animation") \
    X(variDisplay,      "variDisplay") \
    X(simpleLabel,      "simpleLabel") \
    X(canvas,           "canvas") \
    X(enabled,          "enabled") \
    X(isIncrement,      "isIncrement") \
    X(plus,             "+") \
    X(minus,            "-") \
    X(textBox,          "textBox") \
    X(popupTextBox,     "popupTextBox") \
    X(faderShadow,      "faderShadow") \
    X(toggleShadow,     "toggleShadow") \
    X(imageShadow,      "imageShadow") \
    X(displayMode,      "displayMode") \
    X(event,            "event") \

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng
