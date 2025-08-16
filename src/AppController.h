#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include "AppState.h"
#include "DatabaseManager.h"
#include "ConfigManager.h"
#include "EventHandlers.h"
#include "GoogleAuthManager.h"
#include "GoogleDriveManager.h"
#include "LibraryManager.h"
#include "SyncController.h"
#include "UIComponents.h"
#include "ftxui/component/screen_interactive.hpp"
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

using namespace ftxui;

class AppController {
public:
    AppController();
    ~AppController();
    
    // Main application entry point
    int Run(int argc, char* argv[]);

private:
    // Initialization
    bool RunFirstTimeWizard();
    void InitializeManagers();
    void InitializeManagersFromConfig(const fs::path& data_path);
    void InitializeUI();
    void LoadInitialData();
    
    // Core logic functions
    void RefreshBooks();
    void HandleConsoleInteraction();
    void SetupModalFunctions();
    
    // Application state and managers
    AppState app_state_;
    std::mutex ui_state_mutex_;
    
    // Backend managers
    std::unique_ptr<ConfigManager> config_manager_;
    std::unique_ptr<LibraryManager> library_manager_;
    std::unique_ptr<DatabaseManager> db_manager_;
    std::unique_ptr<GoogleAuthManager> auth_manager_;
    std::unique_ptr<GoogleDriveManager> drive_manager_;
    std::unique_ptr<SyncController> sync_controller_;
    
    // UI components
    ScreenInteractive screen_;
    std::unique_ptr<UIComponents> ui_components_;
    std::unique_ptr<EventHandlers> event_handlers_;
    
    // Background threads
    std::atomic<bool> stop_refresh_thread_ = false;
    std::thread refresh_thread_;
    
    // Modal functions
    std::function<void(std::string, std::string, std::function<void()>)> open_modal_;
    std::function<void(std::string, std::string, std::function<void()>, std::function<void()>)> open_confirmation_modal_;
};

#endif // APPCONTROLLER_H