#include "plugins/PluginState.h"
#if JUCE_MAC
#include <CoreFoundation/CoreFoundation.h>
#endif

#if JUCE_MAC
static constexpr const char* audioUnitStateKey { "jucePluginState" };
#endif
static constexpr const char* componentStateName { "IComponent" };

static juce::MemoryBlock getStateInformation (juce::AudioPluginInstance& instance)
{
    juce::MemoryBlock wrapper;
    instance.getStateInformation (wrapper);

    return wrapper;
}

static juce::MemoryBlock getChunkState (const juce::String&, juce::AudioPluginInstance& instance)
{
    juce::MemoryBlock block;
    instance.getVSTClient()->getChunkData (block, false);

    return block;
}

static void setChunkState (const juce::String&, juce::AudioPluginInstance& instance, const juce::MemoryBlock& block)
{
    instance.getVSTClient()->setChunkData (juce::Span<const std::byte> { static_cast<const std::byte*> (block.getData()), block.getSize() }, false);
}

static juce::MemoryBlock getComponentState (const juce::String&, juce::AudioPluginInstance& instance)
{
    const auto wrapper { getStateInformation (instance) };
    const auto xml { juce::AudioProcessor::getXmlFromBinary (wrapper.getData(), static_cast<int> (wrapper.getSize())) };
    const auto* child { xml->getChildByName (componentStateName) };

    juce::MemoryBlock block;
    block.fromBase64Encoding (child->getAllSubText());

    return block;
}

static void setComponentState (const juce::String&, juce::AudioPluginInstance& instance, const juce::MemoryBlock& block)
{
    const auto wrapper { getStateInformation (instance) };
    const auto xml { juce::AudioProcessor::getXmlFromBinary (wrapper.getData(), static_cast<int> (wrapper.getSize())) };
    auto* child { xml->getChildByName (componentStateName) };

    child->deleteAllTextElements();
    child->addTextElement (block.toBase64Encoding());

    juce::MemoryBlock state;
    juce::AudioProcessor::copyXmlToBinary (*xml, state);
    instance.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
}

#if JUCE_MAC
static juce::MemoryBlock getAudioUnitState (const juce::String&, juce::AudioPluginInstance& instance)
{
    const auto wrapper { getStateInformation (instance) };
    const std::unique_ptr<std::remove_pointer_t<CFStringRef>, decltype (&CFRelease)> key {
        CFStringCreateWithCString (kCFAllocatorDefault, audioUnitStateKey, kCFStringEncodingUTF8), CFRelease };
    const std::unique_ptr<std::remove_pointer_t<CFDataRef>, decltype (&CFRelease)> data {
        CFDataCreateWithBytesNoCopy (kCFAllocatorDefault, static_cast<const UInt8*> (wrapper.getData()), static_cast<CFIndex> (wrapper.getSize()), kCFAllocatorNull), CFRelease };
    const std::unique_ptr<std::remove_pointer_t<CFPropertyListRef>, decltype (&CFRelease)> list {
        CFPropertyListCreateWithData (kCFAllocatorDefault, data.get(), kCFPropertyListImmutable, nullptr, nullptr), CFRelease };
    const auto state { static_cast<CFDataRef> (CFDictionaryGetValue (static_cast<CFDictionaryRef> (list.get()), key.get())) };

    return juce::MemoryBlock { CFDataGetBytePtr (state), static_cast<size_t> (CFDataGetLength (state)) };
}

static void setAudioUnitState (const juce::String&, juce::AudioPluginInstance& instance, const juce::MemoryBlock& block)
{
    const auto wrapper { getStateInformation (instance) };
    const std::unique_ptr<std::remove_pointer_t<CFStringRef>, decltype (&CFRelease)> key {
        CFStringCreateWithCString (kCFAllocatorDefault, audioUnitStateKey, kCFStringEncodingUTF8), CFRelease };
    const std::unique_ptr<std::remove_pointer_t<CFDataRef>, decltype (&CFRelease)> data {
        CFDataCreateWithBytesNoCopy (kCFAllocatorDefault, static_cast<const UInt8*> (wrapper.getData()), static_cast<CFIndex> (wrapper.getSize()), kCFAllocatorNull), CFRelease };
    const std::unique_ptr<std::remove_pointer_t<CFMutableDictionaryRef>, decltype (&CFRelease)> list {
        static_cast<CFMutableDictionaryRef> (const_cast<void*> (CFPropertyListCreateWithData (kCFAllocatorDefault, data.get(), kCFPropertyListMutableContainers, nullptr, nullptr))), CFRelease };
    const std::unique_ptr<std::remove_pointer_t<CFDataRef>, decltype (&CFRelease)> state {
        CFDataCreate (kCFAllocatorDefault, static_cast<const UInt8*> (block.getData()), static_cast<CFIndex> (block.getSize())), CFRelease };

    CFDictionarySetValue (list.get(), key.get(), state.get());

    const std::unique_ptr<std::remove_pointer_t<CFDataRef>, decltype (&CFRelease)> serialised {
        CFPropertyListCreateData (kCFAllocatorDefault, list.get(), kCFPropertyListBinaryFormat_v1_0, 0, nullptr), CFRelease };

    instance.setStateInformation (CFDataGetBytePtr (serialised.get()), static_cast<int> (CFDataGetLength (serialised.get())));
}
#endif

static juce::MemoryBlock getRawState (const juce::String&, juce::AudioPluginInstance& instance)
{
    return getStateInformation (instance);
}

static void setRawState (const juce::String&, juce::AudioPluginInstance& instance, const juce::MemoryBlock& block)
{
    instance.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
}

static const auto& getStateReads()
{
    static const auto reads (
        []
        {
            jam::Function::Map<juce::String, jam::Function::pointerCapacity, juce::MemoryBlock (juce::AudioPluginInstance&)> reads;

            reads.add (juce::VSTPluginFormat::getFormatName(), getChunkState);
            reads.add (juce::VST3PluginFormat::getFormatName(), getComponentState);
#if JUCE_MAC
            reads.add (juce::AudioUnitPluginFormat::getFormatName(), getAudioUnitState);
#endif
            reads.add (jam::ClapPluginFormat::getFormatName(), getRawState);

            return reads;
        }()
    );

    return reads;
}

static const auto& getStateWrites()
{
    static const auto writes (
        []
        {
            jam::Function::Map<juce::String, jam::Function::pointerCapacity, void (juce::AudioPluginInstance&, const juce::MemoryBlock&)> writes;

            writes.add (juce::VSTPluginFormat::getFormatName(), setChunkState);
            writes.add (juce::VST3PluginFormat::getFormatName(), setComponentState);
#if JUCE_MAC
            writes.add (juce::AudioUnitPluginFormat::getFormatName(), setAudioUnitState);
#endif
            writes.add (jam::ClapPluginFormat::getFormatName(), setRawState);

            return writes;
        }()
    );

    return writes;
}

PluginState::PluginState (juce::AudioPluginInstance& newInstance, juce::ValueTree paneRow)
    : instance (newInstance)
    , pluginState (paneRow.getOrCreateChildWithName (jam::Format::toType (Id::pluginState), nullptr))
{
    instance.addListener (this);
}

PluginState::~PluginState()
{
    instance.removeListener (this);
}

void PluginState::setRows (const juce::ValueTree& rows)
{
    juce::MemoryBlock block;
    juce::MemoryOutputStream stream { block, false };
    rows.writeToStream (stream);
    stream.flush();

    getStateWrites().get (instance.getPluginDescription().pluginFormatName, instance, block);
}

void PluginState::audioProcessorParameterChanged (juce::AudioProcessor*, int, float) {}

void PluginState::audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&)
{
    const auto block { getStateReads().get (instance.getPluginDescription().pluginFormatName, instance) };
    const auto state { juce::ValueTree::readFromData (block.getData(), block.getSize()) };

    pluginState.copyPropertiesFrom (state.getChildWithName (jam::Format::toType (Id::pluginState)), nullptr);
}
