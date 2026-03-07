/**
 * @file TerminalGLRenderer.h
 * @brief Compatibility shim — `Render::OpenGL` is defined in Screen.h.
 *
 * `Render::OpenGL` was originally declared in this file but has since been
 * consolidated into `Screen.h` as `Terminal::Render::OpenGL`.  This header
 * exists solely so that `TerminalGLRenderer.cpp` can include a single
 * well-known path without needing to know the final home of the class.
 *
 * @note Do **not** add new declarations here.  All renderer types live in
 *       `Screen.h` inside the `Terminal::Render` namespace-struct.
 *
 * @see Screen.h
 * @see Terminal::Render::OpenGL
 */
#pragma once
// OpenGL class is now defined in Screen.h as Terminal::Render::OpenGL
// This file kept for TerminalGLRenderer.cpp include compatibility
#include "Screen.h"
