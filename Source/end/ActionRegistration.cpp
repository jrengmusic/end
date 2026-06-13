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
                              auto id { state.getProperty (ID::focusedPane) };
                              if (auto* panes { tabs.getActivePanes() })
                                  panes->split (
                                      jam::UUID (static_cast<int64_t> (id)), jam::ID::horizontal);
                          });

    registry.actions.add (ID::splitVertical,
                          [this]
                          {
                              auto id { state.getProperty (ID::focusedPane) };
                              if (auto* panes { tabs.getActivePanes() })
                                  panes->split (
                                      jam::UUID (static_cast<int64_t> (id)), jam::ID::vertical);
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
