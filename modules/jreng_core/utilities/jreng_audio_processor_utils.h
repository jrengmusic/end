namespace jreng
{
/*____________________________________________________________________________*/
#if JUCE_MODULE_AVAILABLE_juce_audio_processors
/**
 * @brief A utility structure for managing and retrieving audio processor parameters.
 */
struct Processor
{
    /**
     * @brief Retrieves a parameter from the given audio processor by its ID.
     *
     * @param processor The audio processor containing the parameters.
     * @param parameterID The ID of the desired parameter.
     * @return juce::RangedAudioParameter* Pointer to the parameter if found, or nullptr otherwise.
     */
    static juce::RangedAudioParameter*
        getParameter (const juce::AudioProcessor& processor,
                      const juce::String& parameterID)
    {
        for (auto& param : processor.getParameters())
            if (auto p { dynamic_cast<juce::RangedAudioParameter*> (param) })
                if (p->paramID.compare (parameterID) == 0)
                    return p;

        return nullptr;
    }

    /**
     * @brief Retrieves the normalizable range of a parameter from the given audio processor.
     *
     * @param processor The audio processor containing the parameters.
     * @param parameterID The ID of the desired parameter.
     * @return juce::NormalisableRange<float> The normalizable range of the parameter, or an empty range if not found.
     */
    static juce::NormalisableRange<float>
        getParameterRange (const juce::AudioProcessor& processor,
                           const juce::String& parameterID)
    {
        if (auto parameter { getParameter (processor, parameterID) })
            return parameter->getNormalisableRange();

        return {};
    }
};

#endif// JUCE_MODULE_AVAILABLE_juce_audio_processors
/**_____________________________END OF NAMESPACE______________________________*/
} // namespace jreng
