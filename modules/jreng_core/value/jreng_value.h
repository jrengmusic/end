namespace jreng
{
/*____________________________________________________________________________*/
struct Value
{
    /**
     * @brief Base interface for any Component that exposes a juce::Value.
     *
     * Classes inheriting this must implement getValueObject() to return
     * a reference to their bound juce::Value. This allows generic traversal
     * code to attach component state to a ValueTree.
     *
     * Optionally, onAttachment can be set to a callback that will be invoked
     * when the component's Value is attached to the ValueTree state.
     */
    struct Object
    {
        virtual ~Object() = default;

        /**
         * @brief Return the component's bound juce::Value.
         *
         * @return Reference to the juce::Value owned by the component.
         */
        virtual juce::Value& getValueObject() noexcept = 0;

        /**
         * @brief Optional callback invoked when this component's Value
         *        is attached to a ValueTree state.
         */
        std::function<void()> onAttachment;
    };

    /**
     * @brief CRTP mix-in that enforces ComponentID assignment.
     *
     * Inherit from ObjectID<Derived> alongside juce::Component to ensure
     * that every component has a non-empty ComponentID. The constructor
     * requires an ID string and sets it on the underlying Component.
     *
     * This makes the contract explicit:
     *   - The derived type must be a juce::Component.
     *   - Every instance must declare its ComponentID up front.
     *
     * @tparam Derived The concrete component type, which must inherit juce::Component.
     *
     * @code
     * // Example usage:
     * class MyComponent : public juce::Component,
     *                     public ObjectID<MyComponent>
     * {
     * public:
     *     MyComponent() : ObjectID("MyComponentID") {}
     *
     *     juce::Value& getValueObject() noexcept override { return value; }
     *
     * private:
     *     juce::Value value;
     * };
     * @endcode
     */
    template<typename Derived>
    struct ObjectID : public Object
    {
        /**
         * @brief Construct a ValueObject with a required ComponentID.
         *
         * @param newID The ComponentID string to assign.
         */
        explicit ObjectID (juce::StringRef newID)
        {
            static_assert (std::is_base_of<juce::Component, Derived>::value,
                           "ObjectID can only be mixed into juce::Component subclasses");

            auto* comp = static_cast<Derived*> (this);
            comp->setComponentID (newID);
        }
    };

#if JUCE_MODULE_AVAILABLE_juce_audio_processors
    /**
     * @brief A class to attach a juce::Parameter to a component's Value.
     *
     * This class allows the value of a parameter in an AudioProcessorValueTreeState
     * to be linked to a component's Value. The ParameterAttachment ensures that changes
     * in the parameter are reflected in the component's Value, and vice versa.
     */
    class ParameterAttachment
        : public juce::ParameterAttachment
        , private juce::Value::Listener
    {
    public:
        /**
         * @brief Construct a ParameterAttachment with an AudioProcessorValueTreeState,
         *        parameter ID, associated component value, and optional UndoManager.
         *
         * @param stateToUse Reference to the AudioProcessorValueTreeState.
         * @param parameterID The unique identifier for the parameter to attach.
         * @param componentWithValue Reference to a component that implements Object
         *                         and provides its Value via getValueObject().
         * @param undoManager Optional pointer to an UndoManager for handling value changes.
         */
        ParameterAttachment (juce::AudioProcessorValueTreeState& stateToUse,
                             const juce::String& parameterID,
                             Object& componentWithValue,
                             juce::UndoManager* undoManager = nullptr)
            : juce::ParameterAttachment (*stateToUse.getParameter (parameterID), [this] (float newValue)
                                         {
                                             value.setValue (newValue);
                                         },
                                         undoManager)
            , value (componentWithValue.getValueObject())
        {
            // Assert that the parameter was found. If this fails, the parameterID is incorrect.
            jassert (stateToUse.getParameter (parameterID) != nullptr);

            value.addListener (this);
            sendInitialUpdate();
        }

        /**
         * @brief Destructor for ParameterAttachment.
         */
        ~ParameterAttachment() override
        {
            value.removeListener (this);
        }

    private:
        /**
         * @brief Callback when the associated Value changes.
         *
         * @param value The Value that has changed.
         */
        void valueChanged (juce::Value& value) override
        {
            if (value.refersToSameSourceAs (this->value))
                setValueAsCompleteGesture (value.getValue());
        }

        /**
         * @brief Reference to the component's associated Value.
         */
        juce::Value& value;

        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterAttachment)
    };
#endif// JUCE_MODULE_AVAILABLE_juce_audio_processors

    //==============================================================
    // Helpers
    //==============================================================

    /**
     * @brief Check if a juce::Value is non-void.
     *
     * @param v The juce::Value to test.
     * @return true if the Value is not void, false otherwise.
     */
    static bool isNonVoid (const juce::Value& v) noexcept
    {
        return ! v.getValue().isVoid();
    }

    /**
     * @brief Check if a Component exposes a non-void juce::Value.
     *
     * Uses getFrom() internally to retrieve the Value.
     *
     * @param c Pointer to the Component.
     * @return true if the Component has a non-void Value, false otherwise.
     */
    static bool isNonVoid (juce::Component* c) noexcept
    {
        return ! getFrom (c).getValue().isVoid();
    }

    /**
     * @brief Retrieve the juce::Value associated with a Component.
     *
     * Supports JUCE built-in widgets (Slider, Button, Label, TextEditor, ComboBox),
     * legacy jreng::Component, and any custom component inheriting Object/ObjectID.
     *
     * @param c Pointer to the Component.
     * @return Reference to the associated juce::Value, or a dummy Value if unsupported.
     */
    static juce::Value& getFrom (juce::Component* c)
    {
        static juce::Value dummy;
        if (! c)
            return dummy;

        // JUCE built-ins
        if (auto* s = dynamic_cast<juce::Slider*> (c))
            return s->getValueObject();
        if (auto* b = dynamic_cast<juce::Button*> (c))
            return b->getToggleStateValue();
        if (auto* l = dynamic_cast<juce::Label*> (c))
            return l->getTextValue();
        if (auto* e = dynamic_cast<juce::TextEditor*> (c))
            return e->getTextValue();
        if (auto* cb = dynamic_cast<juce::ComboBox*> (c))
            return cb->getSelectedIdAsValue();

        // New Object-based components
        if (auto* vo = dynamic_cast<Object*> (c))
            return vo->getValueObject();

        return dummy;
    }

    //==============================================================================

    /**
     * @brief Enum representing units for sliders.
     */
    enum class Slider
    {
        noUnit,///< Slider has no unit.
        percent,///< Slider values are in percent.
        pixel,///< Slider values are in pixels.
        degree///< Slider values are in degrees.
    };

    /**
     * @brief Configures a slider to display values as percentages.
     *
     * @tparam SliderPointer A pointer type for sliders.
     * @param slider A pointer to the slider to configure.
     * @param isPositiveOnly Whether the range is restricted to positive values (default: true).
     * @param min The minimum value for the slider range (default: 0.0).
     * @param max The maximum value for the slider range (default: 100.0).
     */
    template<typename SliderPointer>
    static void makePercent (SliderPointer* slider,
                             bool isPositiveOnly = true,
                             double min = 0.0,
                             double max = 100.0,
                             double increment = 1.0)
    {
        slider->setRange (isPositiveOnly ? min : -max, max, increment);
        slider->textFromValueFunction = [] (auto value)
        {
            return juce::String (value) + " %%";
        };
        slider->setDoubleClickReturnValue (true, 0.0);
    }

    /**
     * @brief Configures a slider to display values in pixels.
     *
     * @tparam SliderPointer A pointer type for sliders.
     * @param slider A pointer to the slider to configure.
     */
    template<typename SliderPointer>
    static void makePixel (SliderPointer* slider)
    {
        slider->textFromValueFunction = [] (auto value)
        {
            return juce::String (value) + " px";
        };
    }

    //==============================================================================
    /**
     * @brief Normalize a floating-point value within a specified range.
     *
     * @tparam FloatType The type of the floating-point values.
     * @param value The value to normalize.
     * @param startValue The start of the range.
     * @param endValue The end of the range.
     * @return The normalized value between 0 and 1.
     */
    template<typename FloatType>
    static FloatType normalise (FloatType value,
                                FloatType startValue,
                                FloatType endValue)
    {
        return (value - startValue) / (endValue - startValue);
    }

    /**
     * @brief Normalize an integer value within a specified range.
     *
     * @tparam FloatType The type of the floating-point values for calculation.
     * @param value The integer value to normalize.
     * @param startValue The start of the range.
     * @param endValue The end of the range.
     * @return The normalized value between 0 and 1.
     */
    template<typename FloatType>
    static FloatType normalise (int value, int startValue, int endValue)
    {
        return normalise (static_cast<FloatType> (value),
                          static_cast<FloatType> (startValue),
                          static_cast<FloatType> (endValue));
    }

    /**
     * @brief Normalize a floating-point value within an integer range.
     *
     * @tparam FloatType The type of the floating-point values.
     * @param value The floating-point value to normalize.
     * @param startValue The start of the range as an integer.
     * @param endValue The end of the range as an integer.
     * @return The normalized value between 0 and 1.
     */
    template<typename FloatType>
    static FloatType normalise (FloatType value, int startValue, int endValue)
    {
        return normalise (value,
                          static_cast<FloatType> (startValue),
                          static_cast<FloatType> (endValue));
    }

    /**
     * @brief Clip a value to a minimum threshold.
     *
     * @tparam Type The type of the values.
     * @param value The value to clip.
     * @param minValue The minimum threshold.
     * @return The clipped value, not less than minValue.
     */
    template<typename Type>
    static Type clipMin (Type value, Type minValue)
    {
        if (value < minValue)
            value = minValue;

        return value;
    }

    /**
     * @brief Clip a value to a maximum threshold.
     *
     * @tparam Type The type of the values.
     * @param value The value to clip.
     * @param maxValue The maximum threshold.
     * @return The clipped value, not greater than maxValue.
     */
    template<typename Type>
    static Type clipMax (Type value, Type maxValue)
    {
        if (value > maxValue)
            value = maxValue;

        return value;
    }

    /**
     * @brief Clip a value within a specified range.
     *
     * @tparam Type The type of the values.
     * @param value The value to clip.
     * @param minValue The minimum threshold.
     * @param maxValue The maximum threshold.
     * @return The clipped value, between minValue and maxValue.
     */
    template<typename Type>
    static Type clip (Type value, Type minValue, Type maxValue)
    {
        if (maxValue < minValue)
        {
            value = clipMax (clipMin (value, maxValue), minValue);
        }
        else
        {
            value = clipMax (clipMin (value, minValue), maxValue);
        }

        return value;
    }

    //==============================================================================
    /**
     * @brief Enum representing the type of skew adjustment.
     */
    enum class Skew
    {
        centre, /**< Centered skew adjustment. */
        shape /**< Default linear mapping adjustment. */
    };

    /**
     * @brief Map a normalized value to a specified range with optional skewing.
     *
     * @tparam FloatType The type of the floating-point values.
     * @param value The value to map.
     * @param startValue The start of the source range.
     * @param endValue The end of the source range.
     * @param targetStart The start of the target range.
     * @param targetEnd The end of the target range.
     * @param factor A scaling factor for the mapping.
     * @param curve The type of skew adjustment to apply.
     * @param clamp Whether to clip the result to the target range.
     * @return The mapped value within the target range.
     */
    template<typename FloatType>
    static FloatType map (FloatType value,
                          FloatType startValue,
                          FloatType endValue,
                          FloatType targetStart,
                          FloatType targetEnd,
                          FloatType factor = static_cast<FloatType> (1),
                          Skew curve = Skew::shape,
                          bool clamp = true)
    {
        const FloatType valueRange { startValue - endValue };
        const FloatType mapRange { targetEnd - targetStart };

        if (std::abs (valueRange) < Math::flt_epsilon<FloatType>)
        {
            return targetStart;
        }
        else
        {
            FloatType normal = normalise (value, startValue, endValue);
            FloatType shape;

            switch (curve)
            {
                case Skew::centre:
                {
                    FloatType centreMap = factor;

                    jassert ((centreMap > targetStart && centreMap < targetEnd) || (centreMap < targetStart && centreMap > targetEnd));

                    shape = log ((centreMap - targetStart) / mapRange) / log (static_cast<FloatType> (0.5));
                }
                break;

                default:
                    shape = factor;
                    break;
            }

            FloatType target = targetStart + pow (normal, shape) * mapRange;

            if (clamp)
                target = clip (target, targetStart, targetEnd);

            return target;
        }
    }

    /**
     * @brief Map a normalized value to a specified integer range with optional skewing.
     *
     * @tparam FloatType The type of the floating-point values for calculation.
     * @param value The value to map.
     * @param startValue The start of the source range.
     * @param endValue The end of the source range.
     * @param targetStart The start of the target integer range.
     * @param targetEnd The end of the target integer range.
     * @param factor A scaling factor for the mapping.
     * @param curve The type of skew adjustment to apply.
     * @param clamp Whether to clip the result to the target range.
     * @return The mapped value as an integer within the target range.
     */
    template<typename FloatType>
    static int map (FloatType value,
                    FloatType startValue,
                    FloatType endValue,
                    int targetStart,
                    int targetEnd,
                    FloatType factor = static_cast<FloatType> (1),
                    Skew curve = Skew::shape,
                    bool clamp = true)
    {
        return toInt (map (value,
                           startValue,
                           endValue,
                           static_cast<FloatType> (targetStart),
                           static_cast<FloatType> (targetEnd),
                           factor,
                           curve,
                           clamp));
    }

    //==============================================================================
    /**
     * @brief Structure representing a target range and shape.
     *
     * @tparam Type The type of the values in the target range.
     */
    template<typename Type>
    struct Target
    {
        /**
             * @brief Constructor to initialize the Target structure.
             *
             * @param minValue Minimum value of the target.
             * @param maxValue Maximum value of the target.
             * @param shape Shape parameter for the target.
             */
        Target (const Type minValue,
                const Type maxValue,
                const Type shape)
            : min (minValue)
            , max (maxValue)
            , shape (shape) {}

        /**
             * @brief Destructor for the Target structure.
             */
        ~Target() {}

        const Type min; /**< Minimum value of the target. */
        const Type max; /**< Maximum value of the target. */
        const Type shape; /**< Shape parameter of the target. */

        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Target)
    };

    //==============================================================================
    /**
     * @brief Map a normalized value to a specified range with optional skewing.
     *
     * This version assumes the source range is between 0 and 1.
     *
     * @tparam FloatType The type of the floating-point values.
     * @param normalisedValue The normalized value (between 0 and 1) to map.
     * @param targetStart The start of the target range.
     * @param targetEnd The end of the target range.
     * @param factor A scaling factor for the mapping.
     * @param curve The type of skew adjustment to apply.
     * @param clamp Whether to clip the result to the target range.
     * @return The mapped value within the target range.
     */
    template<typename FloatType>
    static FloatType map (FloatType normalisedValue,
                          FloatType targetStart,
                          FloatType targetEnd,
                          FloatType factor = static_cast<FloatType> (1),
                          Skew curve = Skew::shape,
                          bool clamp = true)
    {
        return map (normalisedValue,
                    static_cast<FloatType> (0),
                    static_cast<FloatType> (1),
                    targetStart,
                    targetEnd,
                    factor,
                    curve,
                    clamp);
    }

    /**
     * @brief Map a normalized value to a specified integer range with optional skewing.
     *
     * This version assumes the source range is between 0 and 1.
     *
     * @tparam FloatType The type of the floating-point values for calculation.
     * @param normalisedValue The normalized value (between 0 and 1) to map.
     * @param targetStart The start of the target integer range.
     * @param targetEnd The end of the target integer range.
     * @param factor A scaling factor for the mapping.
     * @param curve The type of skew adjustment to apply.
     * @param clamp Whether to clip the result to the target range.
     * @return The mapped value as an integer within the target range.
     */
    template<typename FloatType>
    static int map (FloatType normalisedValue,
                    int targetStart,
                    int targetEnd,
                    FloatType factor = static_cast<FloatType> (1),
                    Skew curve = Skew::shape,
                    bool clamp = true)
    {
        return map (normalisedValue,
                    static_cast<FloatType> (0),
                    static_cast<FloatType> (1),
                    targetStart,
                    targetEnd,
                    factor,
                    curve,
                    clamp);
    }

    /**
     * @brief Invert and normalize a value with optional skewing.
     *
     * This function maps the input value from the range [0, 1] to [1, 0].
     *
     * @tparam FloatType The type of the floating-point values.
     * @param normalisableValue The value to invert and normalize.
     * @param factor A scaling factor for the mapping.
     * @param curve The type of skew adjustment to apply.
     * @return The inverted and normalized value between 0 and 1.
     */
    template<typename FloatType>
    static FloatType invertNormalise (FloatType normalisableValue,
                                      FloatType factor = static_cast<FloatType> (1),
                                      Skew curve = Skew::shape)
    {
        return map (normalisableValue,
                    static_cast<FloatType> (1),
                    static_cast<FloatType> (0),
                    factor,
                    curve);
    }

    /**
     * @brief Apply a shaping function to a normalized value.
     *
     * This function maps the input value from the range [0, 1] with a custom shape.
     *
     * @tparam FloatType The type of the floating-point values.
     * @param normalisableValue The value to map.
     * @param factor A scaling factor for the mapping.
     * @param curve The type of skew adjustment to apply.
     * @return The shaped value between 0 and 1.
     */
    template<typename FloatType>
    static FloatType curve (FloatType normalisableValue,
                            FloatType factor,
                            Skew curve = Skew::shape)
    {
        return map (normalisableValue,
                    static_cast<FloatType> (0),
                    static_cast<FloatType> (1),
                    factor,
                    curve);
    }

    /**
     * @brief Convert a percentage value to a mapped value.
     *
     * This function maps the input percentage value from [0, 100] to a specified range.
     *
     * @tparam FloatType The type of the floating-point values for calculation and output.
     * @param percentageValue The percentage value (between 0 and 100) to map.
     * @param targetStart The start of the target range.
     * @param targetEnd The end of the target range.
     * @return The mapped value within the specified range.
     */
    template<typename FloatType>
    static FloatType fromPercent (FloatType percentageValue,
                                  FloatType targetStart,
                                  FloatType targetEnd)
    {
        return map (percentageValue,
                    static_cast<FloatType> (0),
                    static_cast<FloatType> (100),
                    targetStart,
                    targetEnd);
    }

    /**
     * @brief Convert a percentage value to a mapped value with default start.
     *
     * This function maps the input percentage value from [0, 100] to a specified range starting at 0.
     *
     * @tparam FloatType The type of the floating-point values for calculation and output.
     * @param percentageValue The percentage value (between 0 and 100) to map.
     * @param targetEnd The end of the target range.
     * @return The mapped value within the specified range.
     */
    template<typename FloatType>
    static FloatType fromPercent (FloatType percentageValue, FloatType targetEnd)
    {
        return map (percentageValue,
                    static_cast<FloatType> (0),
                    static_cast<FloatType> (100),
                    static_cast<FloatType> (0),
                    targetEnd);
    }
};

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng
