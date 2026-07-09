#include "ENDView.h"
#include "Nexus.h"

void ENDView::createDockPane (int positionKey)
{
    // resolve center anchor -> extent px from getPaneSidebarSize x window
    // axis -> uuid = paneManager.split (anchor, edge, extent) -> dock
    // component = jam::PaneComponent (model, paneManager.getPane (uuid))
    // into components -> ID::position + jam::ID::visible on the row ->
    // resized.
}

void ENDView::registerActions()
{
    registry.actions.add (ID::newTab,
                          [this]
                          {
                              jam::UUID uuid;
                              tabs.add (uuid);

                              // No-split/fresh-tab form — an invalid
                              // (default-constructed) Identifier, the same
                              // convention ID::newPane's own body below
                              // reads via edge.isValid(). Explicit
                              // template argument: the temporary argument
                              // would otherwise deduce Args without the
                              // reference add<>() registered below, and
                              // Function::Map::get()'s static_cast requires
                              // an EXACT Args match.
                              registry.actions.get<const juce::Identifier&> (
                                  ID::newPane, juce::Identifier {});
                          });

    // Mints the new pane and attaches it onto the currently focused
    // (just-created or pre-existing) tab — Panes::add(uuid) when no
    // edge is given, Panes::add(uuid, anchor, edge) split against
    // the SESSIONS-canonical focused pane otherwise — then ALWAYS finishes
    // by invoking ID::newTerminal through the registry with the pane uuid —
    // never called directly, only composed from ID::newTab/ID::splitHorizontal/
    // ID::splitVertical's own bodies. No keybinding of its own.
    registry.actions.add<const juce::Identifier&> (
        ID::newPane,
        [this] (const juce::Identifier& edge)
        {
            auto* panes { tabs.getActivePanes() };

            jam::UUID uuid;

            if (edge.isValid())
            {
                // ID::focusedPane is canonical on the SESSIONS node, never a
                // per-Session tree copy (ARCHITECT ruling).
                const jam::UUID anchor { static_cast<int64_t> (
                    model.getValue (IDtype::sessions, ID::focusedPane)) };

                uuid = panes->add (anchor, edge);
            }
            else
            {
                uuid = panes->add();
            }

            // std::move — Function::Map::get()'s forwarding-reference
            // deduction only matches add<jam::UUID>()'s by-value Args when
            // the argument is an rvalue (a named lvalue would deduce
            // jam::UUID&, mismatching the registered signature). uuid is
            // not read again after this call.
            registry.actions.get (ID::newTerminal, std::move (uuid));
        });

    // Atomic pane-terminal pairing tail — attaches the TERMINAL state under
    // the already-placed PANE leaf (Session::newTerminal()'s own contract).
    // Invoked only through the registry, by ID::newPane's own body above.
    // No keybinding: never bound in keys.lua, registry composition only.
    registry.actions.add<jam::UUID> (
        ID::newTerminal,
        [] (jam::UUID uuid)
        {
            Nexus::getInstance()->getActiveSession().newTerminal (uuid);
        });

    registry.actions.add (ID::closeTab,
                          [this]
                          {
                              if (tabs.getNumTabs() > 1)
                              {
                                  const jam::UUID focusedTab { static_cast<int64_t> (
                                      tabs.getValueTree().getProperty (ID::focusedTab)) };

                                  tabs.remove (focusedTab);
                              }
                              else
                              {
                                  juce::JUCEApplication::getInstance()->systemRequestedQuit();
                              }
                          });

    registry.actions.add (ID::nextTab,
                          [this]
                          {
                              auto next { tabs.getCurrentTabIndex() + 1 };

                              if (next < tabs.getNumTabs())
                                  tabs.setCurrentTabIndex (next);
                          });

    registry.actions.add (ID::prevTab,
                          [this]
                          {
                              auto prev { tabs.getCurrentTabIndex() - 1 };

                              if (prev >= 0)
                                  tabs.setCurrentTabIndex (prev);
                          });

    registry.actions.add (ID::splitHorizontal, [this] {});

    registry.actions.add (ID::splitVertical, [this] {});

    registry.actions.add (ID::closePane,
                          [this]
                          {
                              if (auto* panes { tabs.getActivePanes() })
                              {
                                  if (panes->getPaneCount() > 1)
                                  {
                                      // ID::focusedPane is canonical on the SESSIONS node,
                                      // never a per-Session tree copy (ARCHITECT ruling).
                                      const jam::UUID focusedUuid { static_cast<int64_t> (
                                          model.getValue (IDtype::sessions, ID::focusedPane)) };

                                      panes->remove (focusedUuid);
                                  }
                                  else
                                  {
                                      // Last pane in this tab — same semantics as closeTab
                                      // (dispatched through the SAME action, never duplicated).
                                      registry.run (ID::closeTab);
                                  }
                              }
                          });

    registry.actions.add (
        ID::zoomIn,
        [this]
        {
            // ID::focusedPane is canonical on the SESSIONS node, never a
            // per-Session tree copy (ARCHITECT ruling).
            auto& session { Nexus::getInstance()->getActiveSession() };
            const jam::UUID id { static_cast<int64_t> (
                model.getValue (IDtype::sessions, ID::focusedPane)) };

            if (id.value != 0)
            {
                const float step { config.getValue (IDtype::display, ID::zoomStep) };
                session.get (id).model.zoomBy (step);
            }
        });

    registry.actions.add (
        ID::zoomOut,
        [this]
        {
            // ID::focusedPane is canonical on the SESSIONS node, never a
            // per-Session tree copy (ARCHITECT ruling).
            auto& session { Nexus::getInstance()->getActiveSession() };
            const jam::UUID id { static_cast<int64_t> (
                model.getValue (IDtype::sessions, ID::focusedPane)) };

            if (id.value != 0)
            {
                const float step { config.getValue (IDtype::display, ID::zoomStep) };
                session.get (id).model.zoomBy (-step);
            }
        });

    registry.actions.add (ID::zoomReset,
                          [this]
                          {
                              // ID::focusedPane is canonical on the SESSIONS node, never a
                              // per-Session tree copy (ARCHITECT ruling).
                              auto& session { Nexus::getInstance()->getActiveSession() };
                              const jam::UUID id { static_cast<int64_t> (
                                  model.getValue (IDtype::sessions, ID::focusedPane)) };

                              if (id.value != 0)
                                  session.get (id).model.setZoom (TerminalModel::defaultZoom);
                          });

    registry.actions.add (ID::paneLeft,
                          [this]
                          {
                              if (auto* p { tabs.getActivePanes() })
                                  p->focusPane (ID::paneLeft);
                          });

    registry.actions.add (ID::paneRight,
                          [this]
                          {
                              if (auto* p { tabs.getActivePanes() })
                                  p->focusPane (ID::paneRight);
                          });

    registry.actions.add (ID::paneUp,
                          [this]
                          {
                              if (auto* p { tabs.getActivePanes() })
                                  p->focusPane (ID::paneUp);
                          });

    registry.actions.add (ID::paneDown,
                          [this]
                          {
                              if (auto* p { tabs.getActivePanes() })
                                  p->focusPane (ID::paneDown);
                          });

    // Four WINDOW leaf visibility toggles — loop-registered over the
    // Position bimap entries (Position is the SSOT for edge vocabulary,
    // Bimap.h's own doc comment). center is never toggled (always the main
    // content leaf), so it is excluded from this loop.
    for (const auto& [key, id] : Position::get())
    {
        // C++17 forbids capturing a structured-binding name directly in a
        // lambda — key is a binding into Position::get()'s own pair, not a
        // variable of its own; positionKey is the named copy the capture
        // list actually closes over.
        const int positionKey { key };

        registry.actions.add (
            Position::getPropertyId (key),
            [this, positionKey]
            {
                if (positionKey != Position::center)
                {
                    // WINDOW tree is flat (jam::PaneManager's own class
                    // doc) — the dock leaf, if it already exists, is a
                    // direct child of state carrying ID::position ==
                    // this edge's own string.
                    auto leaf { state.getChildWithProperty (
                        ID::position, Position::get (positionKey)) };

                    if (leaf.isValid())
                    {
                        const bool visible { jam::toBool (leaf.getProperty (jam::ID::visible)) };

                        // Plain property write — jam::ID::visible was
                        // registered as a jam::Model parameter at this
                        // leaf's creation (createDockPane()), so the
                        // write rides that already-live adapter's own
                        // VT->atomic reverse sync, same as any other
                        // registered parameter (EventRegistration.cpp's
                        // ID::focus handler doc comment).
                        leaf.setProperty (
                            jam::ID::visible, static_cast<int> (not visible), nullptr);
                    }
                    else
                    {
                        createDockPane (positionKey);
                    }
                }
            });
    }
}
