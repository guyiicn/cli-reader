#include "AppController.h"
#include "ConfigManager.h"
#include "DebugLogger.h"
#include "PdfParser.h"
#include "SystemUtils.h"
#include "UIUtils.h"
#include "nlohmann/json.hpp"
#include <chrono>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using json = nlohmann::json;

AppController::AppController() : screen_(ScreenInteractive::Fullscreen()) {}

AppController::~AppController() {
    if (app_state_.load_thread.joinable()) {
        app_state_.load_thread.join();
    }
    
    stop_refresh_thread_ = true;
    if (refresh_thread_.joinable()) {
        refresh_thread_.join();
    }
}

int AppController::Run(int argc, char* argv[]) {
    try {
        InitializeManagers();
        InitializeUI();
        LoadInitialData();
        
        // Handle command line arguments
        if (argc > 1) {
            std::string path_from_args = argv[1];
            if (fs::exists(path_from_args)) {
                app_state_.message_to_show = library_manager_->AddBook(path_from_args, *db_manager_, screen_.dimx(), screen_.dimy());
                app_state_.current_view = View::ShowMessage;
            } else {
                app_state_.message_to_show = "Error: File provided via command-line does not exist.";
                app_state_.current_view = View::ShowMessage;
            }
        }
        
        RefreshBooks();
        
        // Start background sync on launch
        if (app_state_.cloud_sync_enabled) {
            std::thread([this] {
                sync_controller_->full_sync([this](bool success, std::string msg) {
                    if (success) {
                        DebugLogger::log("Startup sync completed successfully.");
                        screen_.Post([this] { RefreshBooks(); });
                    } else {
                        DebugLogger::log("Startup sync failed: " + msg);
                    }
                });
            }).detach();
        }
        
        // Create main renderer with UI components
        auto main_renderer = Renderer(ui_components_->GetMainContainer(), [&] {
            Element document;
            switch (app_state_.current_view) {
                case View::Library:
                    document = ui_components_->RenderLibraryView();
                    break;
                case View::Reader:
                    document = ui_components_->RenderReaderView();
                    break;
                case View::FilePicker:
                    document = ui_components_->RenderFilePickerView();
                    break;
                case View::ShowMessage:
                    document = ui_components_->RenderShowMessageView();
                    break;
                case View::Loading:
                    document = ui_components_->RenderLoadingView();
                    break;
                case View::TableOfContents:
                    document = ui_components_->RenderTableOfContentsView();
                    break;
                case View::ConfirmOcr:
                    document = ui_components_->RenderConfirmOcrView();
                    break;
                case View::DeleteConfirm:
                    document = ui_components_->RenderDeleteConfirmView();
                    break;
                case View::SystemInfo:
                    document = ui_components_->RenderSystemInfoView();
                    break;
                default:
                    document = text("Unknown view state") | center;
            }
            return document;
        });
        
        // Create event handler
        auto refresh_books_func = [this]() { RefreshBooks(); };
        auto event_handler = CatchEvent(main_renderer, [&](Event event) -> bool {
            return event_handlers_->HandleEvent(event, ui_components_->GetMainContainer(), refresh_books_func);
        });
        
        // Start refresh thread
        refresh_thread_ = std::thread([&] {
            while (!stop_refresh_thread_) {
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(1s);
                screen_.Post(Event::Custom);
            }
        });
        
        // Main loop
        while(app_state_.current_view != View::Exiting) {
            screen_.Loop(event_handler);
            
            if (app_state_.current_view == View::FirstTimeSetup || 
                app_state_.current_view == View::BlockingAuth) {
                HandleConsoleInteraction();
            }
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Application error: " << e.what() << std::endl;
        return 1;
    }
}

bool AppController::RunFirstTimeWizard() {
    std::cout << "\033[2J\033[1;1H"; // Clear screen
    std::cout << "--- Welcome to new_lastreader (First-Time Setup) ---" << std::endl << std::endl;
    std::cout << "Please specify a directory to store your library and configuration." << std::endl;
    std::cout << "Press ENTER to use the default path (~/.all_reader)." << std::endl;
    std::cout << "Enter path: ";
    
    std::string input_path;
    std::getline(std::cin, input_path);

    if (input_path.empty()) {
        input_path = "~/.all_reader";
    }

    fs::path data_path;
    try {
        data_path = SystemUtils::NormalizePath(input_path);
        fs::create_directories(data_path / "books");
        fs::create_directories(data_path / "config");
    } catch (const fs::filesystem_error& e) {
        std::cerr << std::endl << "Error: Failed to create directory at '" << data_path.string() << "'." << std::endl;
        std::cerr << "Reason: " << e.what() << std::endl;
        std::cerr << "Please check the path and permissions, then restart the application." << std::endl;
        return false;
    }

    fs::path anchor_file_path = fs::path(SystemUtils::GetHomePath()) / ".cli_reader.json";
    json anchor_json;
    anchor_json["data_path"] = data_path.string();
    
    std::ofstream anchor_file(anchor_file_path);
    if (!anchor_file.is_open()) {
        std::cerr << "FATAL: Could not write to anchor file: " << anchor_file_path << std::endl;
        return false;
    }
    anchor_file << anchor_json.dump(4);
    anchor_file.close();

    std::cout << "Initialization complete. Data will be stored in: " << data_path.string() << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));

    InitializeManagersFromConfig(data_path);
    
    // Ask if user wants to configure cloud sync now
    std::cout << "\n--- Cloud Sync Setup (Optional) ---" << std::endl;
    std::cout << "Would you like to configure Google Drive cloud synchronization now?" << std::endl;
    std::cout << "You can always set this up later by pressing 'c' in the main interface." << std::endl;
    std::cout << "Configure now? (y/N): ";
    
    std::string setup_choice;
    std::getline(std::cin, setup_choice);
    
    // Default to "no" if empty or anything other than "y" or "Y"
    if (setup_choice == "y" || setup_choice == "Y") {
        promptForGoogleCredentials(*config_manager_);
    } else {
        std::cout << "Cloud sync setup skipped. You can configure it later." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return true;
}

void AppController::InitializeManagers() {
    fs::path anchor_file_path = fs::path(SystemUtils::GetHomePath()) / ".cli_reader.json";
    
    if (!fs::exists(anchor_file_path)) {
        if (!RunFirstTimeWizard()) {
            throw std::runtime_error("First time setup wizard failed.");
        }
    } else {
        try {
            std::ifstream anchor_file(anchor_file_path);
            json anchor_json;
            anchor_file >> anchor_json;
            fs::path data_path = anchor_json["data_path"].get<std::string>();
            InitializeManagersFromConfig(data_path);
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to read or parse anchor file " + anchor_file_path.string() + ": " + e.what());
        }
    }
}

void AppController::InitializeManagersFromConfig(const fs::path& data_path) {
    // Initialize paths and logging
    fs::path config_dir = data_path / "config";
    fs::create_directories(config_dir); // Ensure it exists
    DebugLogger::init((config_dir / "debug.log").string());
    
    // Initialize Database and Config Managers
    fs::path db_path = config_dir / "library.db";
    db_manager_ = std::make_unique<DatabaseManager>(db_path.string());
    db_manager_->InitDatabase();
    db_manager_->InitializeSystemSettings(data_path.string());

    config_manager_ = std::make_unique<ConfigManager>(*db_manager_);
    config_manager_->LoadSettings();
    DebugLogger::log("Config loaded. Library path from config: " + config_manager_->Get("library_path"));
    
    // Initialize all other managers using the ConfigManager
    library_manager_ = std::make_unique<LibraryManager>(*config_manager_);
    auth_manager_ = std::make_unique<GoogleAuthManager>(*config_manager_);
    drive_manager_ = std::make_unique<GoogleDriveManager>(*auth_manager_);
    sync_controller_ = std::make_unique<SyncController>(*db_manager_, *drive_manager_, *config_manager_);
    
    // The initial sync state is determined by the presence of a refresh token.
    // The user can then disable it at runtime.
    app_state_.cloud_sync_enabled = !config_manager_->GetRefreshToken().empty();
}

void AppController::InitializeUI() {
    ui_components_ = std::make_unique<UIComponents>(app_state_, screen_);
    ui_components_->CreateComponents();
    
    event_handlers_ = std::make_unique<EventHandlers>(
        app_state_, screen_, ui_state_mutex_, *db_manager_, 
        *library_manager_, *sync_controller_, *auth_manager_, *config_manager_
    );
    
    // Set UI components reference for event handlers
    event_handlers_->SetUIComponents(ui_components_.get());
    
    SetupModalFunctions();
}

void AppController::LoadInitialData() {
    app_state_.current_picker_path = config_manager_->GetLastPickerPath();
    UpdatePickerEntries(app_state_.current_picker_path, app_state_.picker_entries, app_state_.selected_picker_entry);
}

void AppController::RefreshBooks() {
    std::lock_guard<std::mutex> lock(ui_state_mutex_);
    app_state_.books = db_manager_->GetAllBooks();
    app_state_.book_display_list.clear();
    
    for (const auto& book : app_state_.books) {
        std::string display_item = book.title + " - " + book.author;

        if (!book.format.empty()) {
            display_item += " [" + book.format + "]";
        }

        int progress = 0;
        if (book.total_pages > 0) {
            progress = static_cast<int>((static_cast<float>(book.current_page) / book.total_pages) * 100);
        }
        display_item += " [" + std::to_string(progress) + "%]";

        if (app_state_.cloud_sync_enabled) {
            if (book.sync_status == "local") display_item += " [💻]";
            else if (book.sync_status == "cloud") display_item += " [☁️]";
            else if (book.sync_status == "synced") display_item += " [✓]";
        }
        app_state_.book_display_list.push_back(display_item);
    }
    
    app_state_.last_library_width = 0;
    app_state_.last_library_height = 0;
}

void AppController::HandleConsoleInteraction() {
    if (app_state_.current_view == View::BlockingAuth) {
        // First check if we need to set up credentials
        if (!config_manager_->hasGoogleCredentials()) {
            std::cout << "\033[2J\033[1;1H";
            bool credentials_set = promptForGoogleCredentials(*config_manager_);
            if (!credentials_set) {
                std::cout << "\nCloud sync setup cancelled." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(2));
                app_state_.current_view = View::Library;
                screen_.Post([this]() { RefreshBooks(); });
                return;
            }
        }
        
        std::cout << "\033[2J\033[1;1H";
        std::string auth_url = auth_manager_->get_authorization_url();
        std::cout << "--- Authorization Required ---\n\n1. URL: " << auth_url << "\n\n2. Paste code:\n> ";
        std::string auth_code;
        std::getline(std::cin, auth_code);

        if (!auth_code.empty() && auth_manager_->exchange_code_for_token(auth_code)) {
            app_state_.cloud_sync_enabled = true;
            std::cout << "\nAuthentication successful!" << std::endl;
            // Immediately refresh the book list to reflect the new cloud status
            RefreshBooks(); 
        } else {
            std::cout << "\nAuthentication failed or cancelled." << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
        app_state_.current_view = View::Library;
        // Post another refresh just in case, to ensure UI redraws correctly upon re-entering the loop.
        screen_.Post([this]() { RefreshBooks(); });
    }
}

void AppController::SetupModalFunctions() {
    open_modal_ = [&](std::string title, std::string content, std::function<void()> action) {
        app_state_.modal_title = title;
        app_state_.modal_content = content;
        app_state_.modal_ok_action = action;
        app_state_.modal_ok_label = "OK";
        app_state_.show_modal_cancel_button = false;
        app_state_.show_modal = true;
    };

    open_confirmation_modal_ = [&](std::string title, std::string content, std::function<void()> yes_action, std::function<void()> no_action) {
        app_state_.modal_title = title;
        app_state_.modal_content = content;
        app_state_.modal_ok_action = yes_action;
        app_state_.modal_cancel_action = no_action;
        app_state_.modal_ok_label = "Yes";
        app_state_.modal_cancel_label = "No";
        app_state_.show_modal_cancel_button = true;
        app_state_.show_modal = true;
    };
}