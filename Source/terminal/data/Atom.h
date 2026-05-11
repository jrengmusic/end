/**
 * @file Atom.h
 * @brief Brings jam::Atom and jam::AtomBase into Terminal namespace.
 *
 * All Atom infrastructure (AtomBase, Atom<int>, Atom<const char*>) is provided
 * by jam_data_structures (jam_atom.h). This header imports them into the
 * Terminal namespace for unqualified use by Terminal::State and related code.
 *
 * @see jam::AtomBase
 * @see jam::Atom<int>
 * @see jam::Atom<const char*>
 */

#pragma once

#include <JuceHeader.h>

namespace Terminal
{

using jam::AtomBase;
using jam::Atom;

} // namespace Terminal
