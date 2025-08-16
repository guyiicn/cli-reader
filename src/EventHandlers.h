#ifndef EVENTHANDLERS_H
#define EVENTHANDLERS_H

#include "AppState.h"
#include "DatabaseManager.h"
#include "ConfigManager.h"
#include "GoogleAuthManager.h"
#include "LibraryManager.h"
#include "SyncController.h"
#include "UIUtils.h"
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include <functional>
#include <mutex>

using namespace ftxui;

// Forward declaration
class UIComponents;

class EventHandlers {
public:
    EventHandlers(AppState& state, 
                  ScreenInteractive& screen,
                  std::mutex& ui_mutex,
                  DatabaseManager& db_manager,
                  LibraryManager& library_manager,
                  SyncController& sync_controller,
                  GoogleAuthManager& auth_manager,
                  ConfigManager& config_manager);

    // Set UI components reference (called after UIComponents is created)
    void SetUIComponents(UIComponents* ui_components);

    // Main event handler
    bool HandleEvent(Event event, Component modal_component, std::function<void()> refresh_books);
    
private:
    // Event handlers for different views
    bool HandleGlobalEvents(Event event, std::function<void()> refresh_books);
    bool HandleLibraryEvents(Event event, std::function<void()> refresh_books);
    bool HandleReaderEvents(Event event, std::function<void()> refresh_books);
    bool HandleTableOfContentsEvents(Event event);
    bool HandleFilePickerEvents(Event event, std::function<void()> refresh_books);
    bool HandleDeleteConfirmEvents(Event event, std::function<void()> refresh_books);
    bool HandleSystemInfoEvents(Event event);
    
    AppState& app_state_;
    ScreenInteractive& screen_;
    std::mutex& ui_state_mutex_;
    DatabaseManager& db_manager_;
    LibraryManager& library_manager_;
    SyncController& sync_controller_;
    GoogleAuthManager& auth_manager_;
    ConfigManager& config_manager_;
    UIComponents* ui_components_ = nullptr;
};

#endif // EVENTHANDLERS_H