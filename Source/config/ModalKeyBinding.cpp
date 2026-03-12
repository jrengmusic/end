#include "ModalKeyBinding.h"

static const juce::String actionKeys[]
{
    Config::Key::keysPaneLeft,
    Config::Key::keysPaneDown,
    Config::Key::keysPaneUp,
    Config::Key::keysPaneRight,
    Config::Key::keysSplitHorizontal,
    Config::Key::keysSplitVertical
};

ModalKeyBinding::ModalKeyBinding()
{
    loadFromConfig();
}

ModalKeyBinding::~ModalKeyBinding() = default;

bool ModalKeyBinding::handleKeyPress (const juce::KeyPress& key)
{
    if (state == State::idle)
    {
        if (key == prefixKey)
        {
            state = State::waiting;
            startTimer (timeoutMs);
            return true;
        }

        return false;
    }

    if (state == State::waiting)
    {
        stopTimer();
        state = State::idle;

        for (const auto& binding : actions)
        {
            if (key == binding.key)
            {
                if (binding.callback != nullptr)
                    binding.callback();

                return true;
            }
        }

        return false;
    }

    return false;
}

void ModalKeyBinding::reload()
{
    stopTimer();
    state = State::idle;
    actions.clear();
    loadFromConfig();
}

void ModalKeyBinding::setAction (Action action, std::function<void()> callback)
{
    const int idx { static_cast<int> (action) };
    jassert (idx >= 0 and idx < 6);

    const juce::KeyPress kp { KeyBinding::parse (Config::getContext()->getString (actionKeys[idx])) };

    for (auto& binding : actions)
    {
        if (binding.key == kp)
        {
            binding.callback = std::move (callback);
            return;
        }
    }

    actions.push_back ({ kp, std::move (callback) });
}

void ModalKeyBinding::loadFromConfig()
{
    auto* cfg { Config::getContext() };
    jassert (cfg != nullptr);

    prefixKey = KeyBinding::parse (cfg->getString (Config::Key::keysPrefix));
    timeoutMs = cfg->getInt (Config::Key::keysPrefixTimeout);

    for (const auto& configKey : actionKeys)
    {
        const juce::KeyPress kp { KeyBinding::parse (cfg->getString (configKey)) };

        if (kp.isValid())
            actions.push_back ({ kp, nullptr });
    }
}

void ModalKeyBinding::timerCallback()
{
    stopTimer();
    state = State::idle;
}
