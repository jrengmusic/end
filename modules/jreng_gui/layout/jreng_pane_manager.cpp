#include "jreng_pane_manager.h"

namespace jreng
{ /*____________________________________________________________________________*/

//==============================================================================
void PaneManager::clearAllItems()
{
    items.clear();
    totalSize = 0;
}

void PaneManager::setItemLayout (const int itemIndex,
                                 const double minimumSize,
                                 const double maximumSize,
                                 const double preferredSize)
{
    auto* layout = getInfoFor (itemIndex);

    if (layout == nullptr)
    {
        layout = new ItemLayoutProperties();
        layout->itemIndex = itemIndex;

        int i { 0 };
        for (i = 0; i < static_cast<int> (items.size()); ++i)
        {
            if (items.at (i)->itemIndex > itemIndex)
                break;
        }

        items.insert (items.begin() + i, std::unique_ptr<ItemLayoutProperties> (layout));
    }

    layout->minSize = minimumSize;
    layout->maxSize = maximumSize;
    layout->preferredSize = preferredSize;
    layout->currentSize = 0;
}

bool PaneManager::getItemLayout (const int itemIndex,
                                 double& minimumSize,
                                 double& maximumSize,
                                 double& preferredSize) const
{
    auto* layout = getInfoFor (itemIndex);

    if (layout != nullptr)
    {
        minimumSize = layout->minSize;
        maximumSize = layout->maxSize;
        preferredSize = layout->preferredSize;
        return true;
    }

    return false;
}

//==============================================================================
void PaneManager::setTotalSize (const int newTotalSize)
{
    totalSize = newTotalSize;

    fitComponentsIntoSpace (0, static_cast<int> (items.size()), totalSize, 0);
}

int PaneManager::getItemCurrentPosition (const int itemIndex) const
{
    int pos { -1 };

    for (int i = 0; i < itemIndex; ++i)
    {
        auto* layout = getInfoFor (i);
        if (layout != nullptr)
            pos += layout->currentSize;
    }

    return pos;
}

int PaneManager::getItemCurrentAbsoluteSize (const int itemIndex) const
{
    auto* layout = getInfoFor (itemIndex);
    if (layout != nullptr)
        return layout->currentSize;

    return 0;
}

double PaneManager::getItemCurrentRelativeSize (const int itemIndex) const
{
    auto* layout = getInfoFor (itemIndex);
    if (layout != nullptr)
        return -layout->currentSize / static_cast<double> (totalSize);

    return 0;
}

void PaneManager::setItemPosition (const int itemIndex, int newPosition)
{
    for (int i = static_cast<int> (items.size()) - 1; i >= 0; --i)
    {
        auto* layout = items.at (i).get();

        if (layout->itemIndex == itemIndex)
        {
            auto realTotalSize = juce::jmax (totalSize, getMinimumSizeOfItems (0, static_cast<int> (items.size())));
            auto minSizeAfterThisComp = getMinimumSizeOfItems (i, static_cast<int> (items.size()));
            auto maxSizeAfterThisComp = getMaximumSizeOfItems (i + 1, static_cast<int> (items.size()));

            newPosition = juce::jmax (newPosition, totalSize - maxSizeAfterThisComp - layout->currentSize);
            newPosition = juce::jmin (newPosition, realTotalSize - minSizeAfterThisComp);

            auto endPos = fitComponentsIntoSpace (0, i, newPosition, 0);

            endPos += layout->currentSize;

            fitComponentsIntoSpace (i + 1, static_cast<int> (items.size()), totalSize - endPos, endPos);
            updatePrefSizesToMatchCurrentPositions();
            break;
        }
    }
}

//==============================================================================
void PaneManager::layOutComponents (ComponentAccessor accessor,
                                    int numComponents,
                                    int x,
                                    int y,
                                    int w,
                                    int h,
                                    const bool vertically,
                                    const bool resizeOtherDimension)
{
    setTotalSize (vertically ? h : w);
    int pos = vertically ? y : x;

    for (int i = 0; i < numComponents; ++i)
    {
        auto* layout = getInfoFor (i);

        if (layout != nullptr)
        {
            auto* c = accessor (i);

            if (c != nullptr)
            {
                if (i == numComponents - 1)
                {
                    if (resizeOtherDimension)
                    {
                        if (vertically)
                            c->setBounds (x, pos, w, juce::jmax (layout->currentSize, h - pos));
                        else
                            c->setBounds (pos, y, juce::jmax (layout->currentSize, w - pos), h);
                    }
                    else
                    {
                        if (vertically)
                            c->setBounds (c->getX(), pos, c->getWidth(), juce::jmax (layout->currentSize, h - pos));
                        else
                            c->setBounds (pos, c->getY(), juce::jmax (layout->currentSize, w - pos), c->getHeight());
                    }
                }
                else
                {
                    if (resizeOtherDimension)
                    {
                        if (vertically)
                            c->setBounds (x, pos, w, layout->currentSize);
                        else
                            c->setBounds (pos, y, layout->currentSize, h);
                    }
                    else
                    {
                        if (vertically)
                            c->setBounds (c->getX(), pos, c->getWidth(), layout->currentSize);
                        else
                            c->setBounds (pos, c->getY(), layout->currentSize, c->getHeight());
                    }
                }
            }

            pos += layout->currentSize;
        }
    }
}

//==============================================================================
PaneManager::ItemLayoutProperties* PaneManager::getInfoFor (const int itemIndex) const
{
    for (auto& i : items)
    {
        if (i->itemIndex == itemIndex)
            return i.get();
    }

    return nullptr;
}

int PaneManager::fitComponentsIntoSpace (const int startIndex,
                                         const int endIndex,
                                         const int availableSpace,
                                         int startPos)
{
    double totalIdealSize { 0.0 };
    int totalMinimums { 0 };

    for (int i = startIndex; i < endIndex; ++i)
    {
        auto* layout = items.at (i).get();

        layout->currentSize = sizeToRealSize (layout->minSize, totalSize);

        totalMinimums += layout->currentSize;
        totalIdealSize += sizeToRealSize (layout->preferredSize, totalSize);
    }

    if (totalIdealSize <= 0)
        totalIdealSize = 1.0;

    int extraSpace = availableSpace - totalMinimums;

    while (extraSpace > 0)
    {
        int numWantingMoreSpace { 0 };
        int numHavingTakenExtraSpace { 0 };

        for (int i = startIndex; i < endIndex; ++i)
        {
            auto* layout = items.at (i).get();

            auto sizeWanted = sizeToRealSize (layout->preferredSize, totalSize);

            auto bestSize = juce::jlimit (layout->currentSize,
                                          juce::jmax (layout->currentSize, sizeToRealSize (layout->maxSize, totalSize)),
                                          juce::roundToInt (sizeWanted * availableSpace / totalIdealSize));

            if (bestSize > layout->currentSize)
                ++numWantingMoreSpace;
        }

        for (int i = startIndex; i < endIndex; ++i)
        {
            auto* layout = items.at (i).get();

            auto sizeWanted = sizeToRealSize (layout->preferredSize, totalSize);

            auto bestSize = juce::jlimit (layout->currentSize,
                                          juce::jmax (layout->currentSize, sizeToRealSize (layout->maxSize, totalSize)),
                                          juce::roundToInt (sizeWanted * availableSpace / totalIdealSize));

            auto extraWanted = bestSize - layout->currentSize;

            if (extraWanted > 0)
            {
                auto extraAllowed = juce::jmin (extraWanted, extraSpace / juce::jmax (1, numWantingMoreSpace));

                if (extraAllowed > 0)
                {
                    ++numHavingTakenExtraSpace;
                    --numWantingMoreSpace;

                    layout->currentSize += extraAllowed;
                    extraSpace -= extraAllowed;
                }
            }
        }

        if (numHavingTakenExtraSpace <= 0)
            break;
    }

    for (int i = startIndex; i < endIndex; ++i)
    {
        auto* layout = items.at (i).get();
        startPos += layout->currentSize;
    }

    return startPos;
}

int PaneManager::getMinimumSizeOfItems (const int startIndex, const int endIndex) const
{
    int totalMinimums { 0 };

    for (int i = startIndex; i < endIndex; ++i)
        totalMinimums += sizeToRealSize (items.at (i)->minSize, totalSize);

    return totalMinimums;
}

int PaneManager::getMaximumSizeOfItems (const int startIndex, const int endIndex) const
{
    int totalMaximums { 0 };

    for (int i = startIndex; i < endIndex; ++i)
        totalMaximums += sizeToRealSize (items.at (i)->maxSize, totalSize);

    return totalMaximums;
}

void PaneManager::updatePrefSizesToMatchCurrentPositions()
{
    for (int i = 0; i < static_cast<int> (items.size()); ++i)
    {
        auto* layout = items.at (i).get();

        layout->preferredSize =
            (layout->preferredSize < 0) ? getItemCurrentRelativeSize (i) : getItemCurrentAbsoluteSize (i);
    }
}

int PaneManager::sizeToRealSize (double size, int totalSpace)
{
    if (size < 0)
        size *= -totalSpace;

    return juce::roundToInt (juce::jmax (1.0, size));
}
/**______________________________END OF NAMESPACE______________________________*/
}// namespace jreng
