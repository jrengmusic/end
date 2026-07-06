#include "View.h"

namespace end
{
/*____________________________________________________________________________*/

void View::registerActions()
{
    registry.actions.add (ID::newTab,
                          [this]
                          {
                              tabs.addNewTab();
                          });

    registry.actions.add (ID::closeTab,
                          [this]
                          {
                              tabs.removeCurrentTab();
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

    registry.actions.add (ID::splitHorizontal,
                          [this]
                          {
                              if (auto* panes { tabs.getActivePanes() })
                              {
                                  const auto uuid { panes->getFirstPaneUUID() };

                                  if (uuid.value != 0)
                                      panes->split (uuid, jam::ID::horizontal);
                              }
                          });

    registry.actions.add (ID::splitVertical,
                          [this]
                          {
                              if (auto* panes { tabs.getActivePanes() })
                              {
                                  const auto uuid { panes->getFirstPaneUUID() };

                                  if (uuid.value != 0)
                                      panes->split (uuid, jam::ID::vertical);
                              }
                          });

    registry.actions.add (ID::closePane,
                          [this]
                          {
                              if (auto* panes { tabs.getActivePanes() })
                              {
                                  if (panes->getPaneCount() == 1)
                                  {
                                      tabs.removeCurrentTab();
                                  }
                                  else
                                  {
                                      auto id { state.getProperty (ID::focusedPane) };
                                      panes->removePane (jam::UUID (static_cast<int64_t> (id)));
                                  }
                              }
                          });

    registry.actions.add (ID::zoomIn,
                          [this]
                          {
                              const jam::UUID id { static_cast<int64_t> (
                                  state.getProperty (ID::focusedPane)) };

                              if (id.value != 0)
                              {
                                  const float step { config.getValue (IDtype::display, ID::zoomStep) };
                                  end::Nexus::getInstance()->get (id).getModel().zoomBy (step);
                              }
                          });

    registry.actions.add (ID::zoomOut,
                          [this]
                          {
                              const jam::UUID id { static_cast<int64_t> (
                                  state.getProperty (ID::focusedPane)) };

                              if (id.value != 0)
                              {
                                  const float step { config.getValue (IDtype::display, ID::zoomStep) };
                                  end::Nexus::getInstance()->get (id).getModel().zoomBy (-step);
                              }
                          });

    registry.actions.add (ID::zoomReset,
                          [this]
                          {
                              const jam::UUID id { static_cast<int64_t> (
                                  state.getProperty (ID::focusedPane)) };

                              if (id.value != 0)
                                  end::Nexus::getInstance()->get (id).getModel().setZoom (
                                      terminal::Model::defaultZoom);
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
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
