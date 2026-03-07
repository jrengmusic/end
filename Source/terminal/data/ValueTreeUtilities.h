/**
 * @file ValueTreeUtilities.h
 * @brief ValueTree traversal and lookup helpers for terminal state management.
 *
 * This file provides utility functions for searching and accessing properties
 * within a JUCE ValueTree hierarchy. These helpers are used to locate parameter
 * nodes by their ID property and retrieve live juce::Value references for
 * data binding.
 *
 * The terminal state is stored as a nested ValueTree structure where each
 * parameter node carries an ID::id property and an ID::value property.
 * These utilities abstract the recursive traversal needed to find nodes
 * anywhere in the tree.
 *
 * @note Thread safety: All functions in this namespace operate on the
 *       MESSAGE THREAD. ValueTree is not thread-safe; do not call from
 *       the reader or audio thread.
 *
 * @see State.h for the primary ValueTree structure
 * @see Identifier.h for the ID::id and ID::value property identifiers
 */

#pragma once

#include <JuceHeader.h>
#include "Identifier.h"

namespace Terminal
{ /*____________________________________________________________________________*/
namespace ValueTreeUtilities
{

/**
 * @brief Recursively applies a function to each node in a ValueTree.
 *
 * Performs a depth-first traversal of the ValueTree rooted at @p root,
 * calling @p function on each node. Traversal stops early if @p function
 * returns true (indicating the target was found).
 *
 * @param root The root of the ValueTree to traverse
 * @param function A callable that takes a const juce::ValueTree& and returns
 *                 true to stop traversal, false to continue
 * @return true if @p function returned true for any node, false otherwise
 *
 * @note Thread safety: MESSAGE THREAD only. ValueTree is not thread-safe.
 *
 * @par Example
 * @code
 * applyFunctionRecursively (root, [] (const juce::ValueTree& node) -> bool
 * {
 *     if (node.getType() == someType)
 *         return true; // stop
 *     return false;    // continue
 * });
 * @endcode
 */
inline bool applyFunctionRecursively (const juce::ValueTree& root,
                                      const std::function<bool (const juce::ValueTree&)>& function)
{
    bool found { function (root) };

    for (auto&& child : root)
    {
        if (not found)
        {
            found = applyFunctionRecursively (child, function);
        }
    }

    return found;
}

/**
 * @brief Finds the first child node whose ID::id property matches @p parameterID.
 *
 * Performs a depth-first search through the ValueTree hierarchy starting at
 * @p root, returning the first node whose ID::id property equals @p parameterID.
 *
 * @param root The root of the ValueTree to search
 * @param parameterID The value to match against the ID::id property
 * @return The matching ValueTree node, or an invalid (default) ValueTree if not found
 *
 * @note Thread safety: MESSAGE THREAD only.
 * @note Returns an invalid ValueTree if no match is found. Check with isValid().
 *
 * @see Identifier.h for ID::id definition
 */
inline juce::ValueTree getChildWithID (const juce::ValueTree& root,
                                       const juce::var& parameterID)
{
    juce::ValueTree result;

    applyFunctionRecursively (root, [&result, &parameterID] (const juce::ValueTree& node) -> bool
    {
        if (node.hasProperty (ID::id) and node.getProperty (ID::id) == parameterID)
        {
            result = node;
            return true;
        }
        return false;
    });

    return result;
}

/**
 * @brief Retrieves a live juce::Value for the ID::value property of a node found by ID.
 *
 * Searches the ValueTree hierarchy for a node whose ID::id matches @p parameterID,
 * then returns a live juce::Value reference to its ID::value property. This value
 * can be used for data binding (e.g., attaching to a slider or label).
 *
 * @param root The root of the ValueTree to search
 * @param parameterID The identifier to search for (matched against ID::id)
 * @param undoManager Optional UndoManager for undo/redo support (may be nullptr)
 * @return A juce::Value referencing the node's ID::value property,
 *         or an empty juce::Value if the node is not found
 *
 * @note Thread safety: MESSAGE THREAD only.
 * @note If the node is not found, the returned Value will be empty (void var).
 *       Callers should verify the node exists before relying on the returned Value.
 *
 * @see State.h for ValueTree structure
 * @see Identifier.h for ID::id and ID::value definitions
 */
inline juce::Value getValueFromChildWithID (const juce::ValueTree& root,
                                            const juce::Identifier& parameterID,
                                            juce::UndoManager* undoManager = nullptr)
{
    return getChildWithID (root, parameterID.toString())
        .getPropertyAsValue (ID::value, undoManager);
}

}
/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
