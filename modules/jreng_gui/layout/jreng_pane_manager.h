#pragma once

namespace jreng
{ /*____________________________________________________________________________*/
//==============================================================================
/**
    For laying out a set of components, where the components have preferred sizes
    and size limits, but where they are allowed to stretch to fill the available
    space.

    For example, if you have a component containing several other components, and
    each one should be given a share of the total size, you could use one of these
    to resize the child components when the parent component is resized. Then
    you could add a PaneResizerBar to easily let the user rescale them.

    A PaneManager operates only in one dimension, so if you have a set
    of components stacked vertically on top of each other, you'd use one to manage their
    heights. To build up complex arrangements of components, e.g. for applications
    with multiple nested panels, you would use more than one PaneManager.
    E.g. by using two (one vertical, one horizontal), you could create a resizable
    spreadsheet-style table.

    @see PaneResizerBar

    @tags{GUI}
*/
class PaneManager
{
public:
    //==============================================================================
    /** Creates an empty layout.

        You'll need to add some item properties to the layout before it can be used
        to resize things - see setItemLayout().
    */
    PaneManager() = default;

    /** Destructor. */
    ~PaneManager() = default;

    //==============================================================================
    /** For a numbered item, this sets its size limits and preferred size.

        @param itemIndex        the index of the item to change.
        @param minimumSize      the minimum size that this item is allowed to be - a positive number
                                indicates an absolute size in pixels. A negative number indicates a
                                proportion of the available space (e.g -0.5 is 50%)
        @param maximumSize      the maximum size that this item is allowed to be - a positive number
                                indicates an absolute size in pixels. A negative number indicates a
                                proportion of the available space
        @param preferredSize    the size that this item would like to be, if there's enough room. A
                                positive number indicates an absolute size in pixels. A negative number
                                indicates a proportion of the available space
        @see getItemLayout
    */
    void setItemLayout (int itemIndex, double minimumSize, double maximumSize, double preferredSize);

    /** For a numbered item, this returns its size limits and preferred size.

        @param itemIndex        the index of the item.
        @param minimumSize      the minimum size that this item is allowed to be - a positive number
                                indicates an absolute size in pixels. A negative number indicates a
                                proportion of the available space (e.g -0.5 is 50%)
        @param maximumSize      the maximum size that this item is allowed to be - a positive number
                                indicates an absolute size in pixels. A negative number indicates a
                                proportion of the available space
        @param preferredSize    the size that this item would like to be, if there's enough room. A
                                positive number indicates an absolute size in pixels. A negative number
                                indicates a proportion of the available space
        @returns false if the item's properties hadn't been set
        @see setItemLayout
    */
    bool getItemLayout (int itemIndex, double& minimumSize, double& maximumSize, double& preferredSize) const;

    /** Clears all the properties that have been set with setItemLayout() and resets
        this object to its initial state.
    */
    void clearAllItems();

    //==============================================================================
    /** Takes a set of components that correspond to the layout's items, and positions
        them to fill a space.

        This will try to give each item its preferred size, whether that's a relative size
        or an absolute one.

        @param components       an Owner of components that correspond to each of the
                                numbered items that the PaneManager object
                                has been told about with setItemLayout()
        @param x                the left of the rectangle in which the components should
                                be laid out
        @param y                the top of the rectangle in which the components should
                                be laid out
        @param width            the width of the rectangle in which the components should
                                be laid out
        @param height           the height of the rectangle in which the components should
                                be laid out
        @param vertically       if true, the components will be positioned in a vertical stack,
                                so that they fill the height of the rectangle. If false, they
                                will be placed side-by-side in a horizontal line, filling the
                                available width
        @param resizeOtherDimension     if true, this means that the components will have their
                                other dimension resized to fit the space - i.e. if the 'vertically'
                                parameter is true, their x-positions and widths are adjusted to fit
                                the x and width parameters; if 'vertically' is false, their y-positions
                                and heights are adjusted to fit the y and height parameters.
    */
    using ComponentAccessor = std::function<juce::Component* (int)>;

    void layOutComponents (ComponentAccessor accessor,
                           int numComponents,
                           int x,
                           int y,
                           int width,
                           int height,
                           bool vertically,
                           bool resizeOtherDimension);

    //==============================================================================
    /** Returns the current position of one of the items.

        This is only a valid call after layOutComponents() has been called, as it
        returns the last position that this item was placed at. If the layout was
        vertical, the value returned will be the y position of the top of the item,
        relative to the top of the rectangle in which the items were placed (so for
        example, item 0 will always have position of 0, even in the rectangle passed
        in to layOutComponents() wasn't at y = 0). If the layout was done horizontally,
        the position returned is the item's left-hand position, again relative to the
        x position of the rectangle used.

        @see getItemCurrentSize, setItemPosition
    */
    int getItemCurrentPosition (int itemIndex) const;

    /** Returns the current size of one of the items.

        This is only meaningful after layOutComponents() has been called, as it
        returns the last size that this item was given. If the layout was done
        vertically, it'll return the item's height in pixels; if it was horizontal,
        it'll return its width.

        @see getItemCurrentRelativeSize
    */
    int getItemCurrentAbsoluteSize (int itemIndex) const;

    /** Returns the current size of one of the items.

        This is only meaningful after layOutComponents() has been called, as it
        returns the last size that this item was given. If the layout was done
        vertically, it'll return a negative value representing the item's height relative
        to the last size used for laying the components out; if the layout was done
        horizontally it'll be the proportion of its width.

        @see getItemCurrentAbsoluteSize
    */
    double getItemCurrentRelativeSize (int itemIndex) const;

    //==============================================================================
    /** Moves one of the items, shifting along any other items as necessary in
        order to get it to the desired position.

        Calling this method will also update the preferred sizes of the items it
        shuffles along, so that they reflect their new positions.

        (This is the method that a PaneResizerBar uses to shift the items
        about when it's dragged).

        @param itemIndex        the item to move
        @param newPosition      the absolute position that you'd like this item to move
                                to. The item might not be able to always reach exactly this position,
                                because other items may have minimum sizes that constrain how
                                far it can go
    */
    void setItemPosition (int itemIndex, int newPosition);

private:
    //==============================================================================
    struct ItemLayoutProperties
    {
        int itemIndex;
        int currentSize;
        double minSize, maxSize, preferredSize;
    };

    std::vector<std::unique_ptr<ItemLayoutProperties>> items;
    int totalSize { 0 };

    //==============================================================================
    static int sizeToRealSize (double size, int totalSpace);
    ItemLayoutProperties* getInfoFor (int itemIndex) const;
    void setTotalSize (int newTotalSize);
    int fitComponentsIntoSpace (int startIndex, int endIndex, int availableSpace, int startPos);
    int getMinimumSizeOfItems (int startIndex, int endIndex) const;
    int getMaximumSizeOfItems (int startIndex, int endIndex) const;
    void updatePrefSizesToMatchCurrentPositions();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PaneManager)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace jreng
