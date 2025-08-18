#include "EventHandlers.h"
#include "BookViewModel.h"
#include "DebugLogger.h"
#include "PdfParser.h"
#include "SystemUtils.h"
#include "UIComponents.h"
#include <chrono>
#include <fstream>
#include <memory>
#include <thread>
#include <filesystem>

namespace fs = std::filesystem;

EventHandlers::EventHandlers(AppState& state, 
                           ScreenInteractive& screen,
                           std::mutex& ui_mutex,
                           DatabaseManager& db_manager,
                           LibraryManager& library_manager,
                           SyncController& sync_controller,
                           GoogleAuthManager& auth_manager,
                           ConfigManager& config_manager)
    : app_state_(state), screen_(screen), ui_state_mutex_(ui_mutex),
      db_manager_(db_manager), library_manager_(library_manager),
      sync_controller_(sync_controller), auth_manager_(auth_manager),
      config_manager_(config_manager) {}

void EventHandlers::SetUIComponents(UIComponents* ui_components) {
    ui_components_ = ui_components;
}

bool EventHandlers::HandleEvent(Event event, Component modal_component, std::function<void()> refresh_books) {
    if (app_state_.show_modal) {
        return modal_component->OnEvent(event);
    }

    // Handle global events first
    if (HandleGlobalEvents(event, refresh_books)) {
        return true;
    }

    // Handle view-specific events
    switch (app_state_.current_view) {
        case View::Library:
            return HandleLibraryEvents(event, refresh_books);
        case View::Reader:
            return HandleReaderEvents(event, refresh_books);
        case View::TableOfContents:
            return HandleTableOfContentsEvents(event);
        case View::FilePicker:
            return HandleFilePickerEvents(event, refresh_books);
        case View::DeleteConfirm:
            return HandleDeleteConfirmEvents(event, refresh_books);
        case View::SystemInfo:
            return HandleSystemInfoEvents(event);
        default:
            return false;
    }
}

bool EventHandlers::HandleGlobalEvents(Event event, std::function<void()> refresh_books) {
    if (event == BOOK_LOAD_SUCCESS) {
        app_state_.current_view = View::Reader;
        screen_.Post(Event::Custom);
        return true;
    }
    
    if (event == BOOK_LOAD_FAILURE) {
        app_state_.message_to_show = "Failed to load book. The file may be corrupt or unsupported.";
        app_state_.current_view = View::ShowMessage;
        screen_.Post(Event::Custom);
        return true;
    }

    if (event == Event::Character('q')) {
        // Only handle 'q' globally if we're NOT in Reader view
        if (app_state_.current_view != View::Reader) {
            app_state_.current_view = View::Exiting;
            screen_.Exit();
            return true;
        }
        // Let Reader view handle 'q' key itself
        return false;
    }

    if (event == Event::Character('c')) {
        std::lock_guard<std::mutex> lock(ui_state_mutex_);
        if (app_state_.cloud_sync_enabled) {
            app_state_.cloud_sync_enabled = false;
            // This state is now managed in memory, no need to call sync_controller
            screen_.Post(refresh_books);
        } else {
            // Check if Google credentials are configured
            if (!config_manager_.hasGoogleCredentials()) {
                app_state_.current_view = View::BlockingAuth;
                screen_.Exit(); // Exit the UI loop to enter console interaction for credential setup
                return true;
            }
            
            bool needs_interaction = false;
            std::string token = auth_manager_.get_access_token(needs_interaction);
            if (!token.empty()) {
                app_state_.cloud_sync_enabled = true;
                // This state is now managed in memory
                screen_.Post(refresh_books);
            } else {
                // Need to re-authenticate with existing credentials
                app_state_.current_view = View::BlockingAuth;
                screen_.Exit(); // Exit the UI loop to enter console interaction
            }
        }
        return true;
    }

    return false;
}

bool EventHandlers::HandleLibraryEvents(Event event, std::function<void()> refresh_books) {
    if (event == Event::Character('s')) {
        app_state_.system_info_data.clear();
        
        app_state_.system_info_data.push_back({"Library Path", config_manager_.GetLibraryPath().string()});
        app_state_.system_info_data.push_back({"Config Path", config_manager_.GetConfigPath().string()});

        app_state_.system_info_data.push_back({"Google Client ID", config_manager_.GetClientId()});
        
        // Access Token
        app_state_.system_info_data.push_back({"", ""}); // Separator
        app_state_.system_info_data.push_back({"Access Token", ""});
        bool needs_interaction = false;
        std::string access_token = auth_manager_.get_access_token(needs_interaction);
        if (needs_interaction) {
            app_state_.system_info_data.push_back({"  Status", "Needs user login"});
        } else if (access_token.empty()) {
            app_state_.system_info_data.push_back({"  Status", "Unavailable (refresh failed)"});
        } else {
            app_state_.system_info_data.push_back({"  Status", "Available (in memory)"});
        }

        // Refresh Token
        app_state_.system_info_data.push_back({"", ""}); // Separator
        app_state_.system_info_data.push_back({"Refresh Token", ""});
        app_state_.system_info_data.push_back({"  Status", config_manager_.GetRefreshToken().empty() ? "Not Set" : "Stored in Database"});

        // Other info
        app_state_.system_info_data.push_back({"", ""}); // Separator
        app_state_.system_info_data.push_back({"Last Picker Path", config_manager_.GetLastPickerPath().string()});
        app_state_.system_info_data.push_back({"Cloud Sync Enabled", app_state_.cloud_sync_enabled ? "Yes" : "No"});

        app_state_.system_info_data.push_back({"", ""}); // Separator
        app_state_.system_info_data.push_back({"Cli Ebook Reader", "Version 1.0"});
        app_state_.system_info_data.push_back({"License", "MIT"});
        app_state_.system_info_data.push_back({"Author", "guyiicn@gmail.com"});

        app_state_.current_view = View::SystemInfo;
        return true;
    }


    if (event == Event::Character('r') && app_state_.cloud_sync_enabled) {
        app_state_.sync_status = SyncStatus::IN_PROGRESS;
        app_state_.sync_message = "Syncing with cloud...";
        screen_.Post(Event::Custom);

        std::thread([this, refresh_books] {
            sync_controller_.full_sync([this, refresh_books](bool success, std::string msg) {
                app_state_.sync_status = success ? SyncStatus::SUCCESS : SyncStatus::ERROR;
                app_state_.sync_message = msg;
                screen_.Post([refresh_books] {
                    refresh_books();
                });
            });
        }).detach();
        return true;
    }

    if (event == Event::Return) {
        if (app_state_.books.empty()) return true;
        int global_index = (app_state_.library_current_page * app_state_.library_entries_per_page) + app_state_.selected_book_index;
        if (global_index >= app_state_.books.size()) return true;

        const auto& selected_book = app_state_.books[global_index];
        
        auto final_load_action = [&](Book book_to_load) {
            db_manager_.UpdateLastReadTime(book_to_load.uuid);
            
            auto start_loading = [&](const Book& book_to_load_inner) {
                if (app_state_.load_thread.joinable()) app_state_.load_thread.join();
                app_state_.loading_message = "Loading: " + book_to_load_inner.title;
                app_state_.current_view = View::Loading;
                screen_.Post(Event::Custom);
                
                app_state_.load_thread = std::thread([&, book_path = book_to_load_inner.path, book_current_page = book_to_load_inner.current_page] {
                    auto parser = CreateParser(book_path);
                    if (!parser) {
                        screen_.PostEvent(BOOK_LOAD_FAILURE);
                        return;
                    }

                    if (auto* pdf_parser = dynamic_cast<PdfParser*>(parser.get())) {
                        if (!pdf_parser->Load()) {
                            screen_.PostEvent(BOOK_LOAD_FAILURE);
                            return;
                        }
                        if (pdf_parser->IsImageBased()) {
                            app_state_.message_to_show = "This PDF appears to be image-based. OCR functionality is under development.";
                            app_state_.current_view = View::ShowMessage;
                            screen_.Post(Event::Custom);
                            return;
                        }
                    }
                    
                    std::unique_ptr<BookViewModel> temp_model = std::make_unique<BookViewModel>(std::move(parser));
                    temp_model->Paginate(screen_.dimx() - 4, screen_.dimy() - 6);

                    {
                        std::lock_guard<std::mutex> lock(app_state_.model_mutex);
                        app_state_.book_view_model = std::move(temp_model);
                        app_state_.current_page = book_current_page;
                        app_state_.paginated = true;
                    }
                    
                    screen_.PostEvent(BOOK_LOAD_SUCCESS);
                });
            };

            if (book_to_load.format == "PDF" && book_to_load.pdf_content_type == "image_based") {
                app_state_.book_to_action_uuid = book_to_load.uuid;
                app_state_.current_view = View::ConfirmOcr;
                screen_.Post(Event::Custom);
            } else {
                start_loading(book_to_load);
            }
        };

        if (app_state_.cloud_sync_enabled && selected_book.sync_status == "cloud") {
            app_state_.current_view = View::Loading;
            app_state_.loading_message = "Verifying and downloading " + selected_book.title + "...";
            screen_.Post(Event::Custom);

            // Get config directory for downloads from the single source of truth
            fs::path download_dir = config_manager_.GetLibraryPath();
            
            sync_controller_.verify_and_download_book_async(selected_book, download_dir.string(), [this, refresh_books](bool success, std::string msg){
                if (success) {
                    screen_.Post([this, refresh_books]{
                        refresh_books();
                        app_state_.current_view = View::Library;
                        screen_.Post(Event::Custom);
                    });
                } else {
                    app_state_.message_to_show = msg;
                    app_state_.current_view = View::ShowMessage;
                    screen_.Post(Event::Custom);
                }
            });
        } else if (app_state_.cloud_sync_enabled && selected_book.sync_status == "synced") {
            app_state_.current_view = View::Loading;
            app_state_.loading_message = "Checking for latest progress...";
            screen_.Post(Event::Custom);

            std::thread([this, book_uuid = selected_book.uuid, final_load_action]{
                sync_controller_.get_latest_progress_async(book_uuid, [this, final_load_action](Book updated_book, bool success){
                    if (success) {
                        screen_.Post([final_load_action, updated_book]{
                            final_load_action(updated_book);
                        });
                    } else {
                        screen_.Post([final_load_action, updated_book]{
                            final_load_action(updated_book); // Use local version on failure
                        });
                    }
                });
            }).detach();
        } else {
            final_load_action(selected_book);
        }
        return true;
    }

    if (event == Event::Character('a')) {
        UpdatePickerEntries(app_state_.current_picker_path, app_state_.picker_entries, app_state_.selected_picker_entry);
        app_state_.current_view = View::FilePicker;
        screen_.Post(Event::Custom);
        return true;
    }

    if (event == Event::Character('d')) {
        if (app_state_.books.empty()) return true;
        int global_index = (app_state_.library_current_page * app_state_.library_entries_per_page) + app_state_.selected_book_index;
        if (global_index >= app_state_.books.size()) return true;
        
        const auto& book = app_state_.books[global_index];
        
        app_state_.uuid_to_delete = book.uuid;
        app_state_.title_to_delete = book.title;
        
        app_state_.delete_options.clear();
        if (book.sync_status == "synced") {
            app_state_.delete_options.push_back("Delete from this device only");
            app_state_.delete_options.push_back("Delete from cloud only");
            app_state_.delete_options.push_back("Delete from both device and cloud");
        } else if (book.sync_status == "local") {
            app_state_.delete_options.push_back("Delete from this device");
            if (app_state_.cloud_sync_enabled) {
                app_state_.delete_options.push_back("Upload and Sync");
            }
        } else if (book.sync_status == "cloud") {
            app_state_.delete_options.push_back("Delete from cloud only");
            app_state_.delete_options.push_back("Download to this device");
        }
        app_state_.delete_options.push_back("Cancel");
        app_state_.selected_delete_option = 0;
        
        app_state_.current_view = View::DeleteConfirm;
        screen_.Post(Event::Custom);
        return true;
    }

    if (event == Event::ArrowLeft) {
        if (app_state_.library_current_page > 0) {
            app_state_.library_current_page--;
            app_state_.selected_book_index = 0;
        }
        screen_.Post(Event::Custom);
        return true;
    }
    
    if (event == Event::ArrowRight) {
        if (app_state_.library_current_page < app_state_.library_total_pages - 1) {
            app_state_.library_current_page++;
            app_state_.selected_book_index = 0;
        }
        screen_.Post(Event::Custom);
        return true;
    }

    if (event == Event::Character('u')) {
        if (app_state_.cloud_sync_enabled && !app_state_.books.empty()) {
            int global_index = app_state_.library_current_page * app_state_.library_entries_per_page + app_state_.selected_book_index;
            if (global_index < app_state_.books.size()) {
                const auto& book = app_state_.books[global_index];
                if (book.sync_status == "local") {
                    app_state_.sync_status = SyncStatus::IN_PROGRESS;
                    app_state_.sync_message = "Uploading " + book.title + "...";
                    screen_.Post(Event::Custom);
                    std::thread([this, book_uuid = book.uuid, refresh_books] {
                        sync_controller_.upload_book(book_uuid, [this, refresh_books](bool success, std::string msg){
                            app_state_.sync_status = success ? SyncStatus::SUCCESS : SyncStatus::ERROR;
                            app_state_.sync_message = msg;
                            if (success) {
                                screen_.Post([refresh_books]{
                                    refresh_books();
                                });
                            }
                            screen_.Post(Event::Custom);
                        });
                    }).detach();
                }
            }
        }
        return true;
    }

    return false;
}

bool EventHandlers::HandleReaderEvents(Event event, std::function<void()> refresh_books) {
    if (event == Event::Character('d')) {
        app_state_.dual_page_mode_enabled = !app_state_.dual_page_mode_enabled;
        app_state_.paginated = false; // Force re-pagination on next render
        screen_.Post(Event::Custom);
        return true;
    }

    if (event == Event::Character('q')) {
        int global_index = (app_state_.library_current_page * app_state_.library_entries_per_page) + app_state_.selected_book_index;
        if (global_index < app_state_.books.size()) {
            Book book_to_update = app_state_.books[global_index];
            book_to_update.current_page = app_state_.current_page;
            book_to_update.last_read_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            
            db_manager_.UpdateProgressAndTimestamp(book_to_update.uuid, book_to_update.current_page, book_to_update.last_read_time);

            if (app_state_.cloud_sync_enabled && (book_to_update.sync_status == "synced" || book_to_update.sync_status == "cloud")) {
                sync_controller_.upload_progress_async(book_to_update, [](bool success){
                    if (!success) {
                        DebugLogger::log("Background progress upload failed.");
                    }
                });
            }
        }
        
        refresh_books();
        app_state_.current_view = View::Library;
        screen_.Post(Event::Custom);
        return true;
    }

    if (event == Event::ArrowRight || event == Event::Character('j')) {
        if (app_state_.book_view_model) {
            bool is_dual = app_state_.dual_page_mode_enabled && (screen_.dimx() > 100);
            int increment = is_dual ? 2 : 1;
            if (app_state_.current_page + increment < app_state_.book_view_model->GetTotalPages()) {
                app_state_.current_page += increment;
            } else if (app_state_.current_page < app_state_.book_view_model->GetTotalPages() - 1) {
                app_state_.current_page = app_state_.book_view_model->GetTotalPages() - 1;
            }
        }
        screen_.Post(Event::Custom);
        return true;
    }
    
    if (event == Event::ArrowLeft || event == Event::Character('k')) {
        bool is_dual = app_state_.dual_page_mode_enabled && (screen_.dimx() > 100);
        int decrement = is_dual ? 2 : 1;
        app_state_.current_page -= decrement;
        if (app_state_.current_page < 0) app_state_.current_page = 0;
        screen_.Post(Event::Custom);
        return true;
    }

    if (event == Event::Character('m')) {
        app_state_.toc_entries.clear();
        FlattenChapters(app_state_.book_view_model->GetChapters(), app_state_.toc_entries);
        
        app_state_.toc_entries_per_page = screen_.dimy() - 8;
        if (app_state_.toc_entries_per_page <= 0) app_state_.toc_entries_per_page = 1;
        
        app_state_.toc_total_pages = app_state_.toc_entries.empty() ? 1 : (app_state_.toc_entries.size() + app_state_.toc_entries_per_page - 1) / app_state_.toc_entries_per_page;
        app_state_.toc_current_page = 0;
        
        int start_index = 0;
        int end_index = std::min((int)app_state_.toc_entries.size(), app_state_.toc_entries_per_page);
        app_state_.toc_visible_entries.assign(app_state_.toc_entries.begin() + start_index, app_state_.toc_entries.begin() + end_index);
        
        app_state_.selected_toc_entry = 0;
        app_state_.current_view = View::TableOfContents;
        screen_.Post(Event::Custom);
        return true;
    }

    return false;
}

bool EventHandlers::HandleTableOfContentsEvents(Event event) {
    if (event == Event::Return) {
        int global_toc_index = (app_state_.toc_current_page * app_state_.toc_entries_per_page) + app_state_.selected_toc_entry;
        if (app_state_.book_view_model && global_toc_index < app_state_.toc_entries.size()) {
            app_state_.current_page = app_state_.book_view_model->GetChapterStartPage(global_toc_index);
        }
        app_state_.current_view = View::Reader;
        screen_.Post(Event::Custom);
        return true;
    }
    
    if (event == Event::Escape || event == Event::Character('m')) {
        app_state_.current_view = View::Reader;
        screen_.Post(Event::Custom);
        return true;
    }

    auto update_visible_toc = [&]() {
        int start_index = app_state_.toc_current_page * app_state_.toc_entries_per_page;
        int end_index = std::min(start_index + app_state_.toc_entries_per_page, (int)app_state_.toc_entries.size());
        app_state_.toc_visible_entries.assign(app_state_.toc_entries.begin() + start_index, app_state_.toc_entries.begin() + end_index);
        app_state_.selected_toc_entry = 0;
    };
    
    if (event == Event::ArrowLeft) {
        if (app_state_.toc_current_page > 0) {
            app_state_.toc_current_page--;
            update_visible_toc();
        }
        screen_.Post(Event::Custom);
        return true;
    }
    
    if (event == Event::ArrowRight) {
        if (app_state_.toc_current_page < app_state_.toc_total_pages - 1) {
            app_state_.toc_current_page++;
            update_visible_toc();
        }
        screen_.Post(Event::Custom);
        return true;
    }

    return false;
}

bool EventHandlers::HandleFilePickerEvents(Event event, std::function<void()> refresh_books) {
    if (event == Event::Return) {
        if (app_state_.picker_entries.empty()) return true;
        
        fs::path selected_item = app_state_.picker_entries[app_state_.selected_picker_entry];
        fs::path new_path;
        
        if (selected_item == "../") {
            new_path = app_state_.current_picker_path.parent_path();
        } else {
            new_path = app_state_.current_picker_path / selected_item;
        }

        if (fs::is_directory(new_path)) {
            app_state_.current_picker_path = fs::canonical(new_path);
            UpdatePickerEntries(app_state_.current_picker_path, app_state_.picker_entries, app_state_.selected_picker_entry);
        } else {
            // Save the picker path to the database via ConfigManager
            config_manager_.SetLastPickerPath(new_path.parent_path());
            
            // Add the book to library
            app_state_.message_to_show = library_manager_.AddBook(new_path.string(), db_manager_, screen_.dimx(), screen_.dimy());
            refresh_books();
            app_state_.current_view = View::ShowMessage;
        }
        screen_.Post(Event::Custom);
        return true;
    }
    
    if (event == Event::Escape) {
        app_state_.current_view = View::Library;
        screen_.Post(Event::Custom);
        return true;
    }

    // Handle menu navigation events (arrow keys)
    if (ui_components_) {
        return ui_components_->GetPickerMenu()->OnEvent(event);
    }
    return false;
}

bool EventHandlers::HandleDeleteConfirmEvents(Event event, std::function<void()> refresh_books) {
    if (event == Event::Escape) {
        app_state_.current_view = View::Library;
        screen_.Post(Event::Custom);
        return true;
    }
    
    if (event == Event::Return) {
        const std::string& selected_option = app_state_.delete_options[app_state_.selected_delete_option];
        const std::string uuid = app_state_.uuid_to_delete;

        if (selected_option == "Cancel") {
            app_state_.current_view = View::Library;
            screen_.Post(Event::Custom);
            return true;
        }

        app_state_.current_view = View::Loading;
        app_state_.loading_message = "Processing: " + selected_option;
        screen_.Post(Event::Custom);

        std::thread([this, selected_option, uuid, refresh_books]{
            bool success = false;
            // --- Option Handling ---
            if (selected_option == "Delete from this device only") {
                success = library_manager_.DeleteBook(uuid, db_manager_, DeleteScope::LocalOnly);
            } 
            else if (selected_option == "Delete from cloud only") {
                sync_controller_.delete_cloud_file_async(uuid, [this, refresh_books](bool async_success) {
                    if (async_success) {
                        screen_.Post([this, refresh_books]{
                            refresh_books();
                            app_state_.current_view = View::Library;
                            screen_.Post(Event::Custom);
                        });
                    } else {
                        app_state_.message_to_show = "Failed to delete from cloud.";
                        app_state_.current_view = View::ShowMessage;
                        screen_.Post(Event::Custom);
                    }
                });
                return; // Async, so we return here
            }
            else if (selected_option == "Delete from both device and cloud") {
                sync_controller_.delete_cloud_file_async(uuid, [this, uuid, refresh_books](bool async_success) {
                    if (async_success) {
                        library_manager_.DeleteBook(uuid, db_manager_, DeleteScope::CloudAndLocal);
                    }
                    // Refresh regardless of the final outcome
                    screen_.Post([this, refresh_books]{
                        refresh_books();
                        app_state_.current_view = View::Library;
                        screen_.Post(Event::Custom);
                    });
                });
                return; // Async, so we return here
            }
            else if (selected_option == "Delete from this device") { // For local-only books
                success = library_manager_.DeleteBook(uuid, db_manager_, DeleteScope::CloudAndLocal);
            }
            // Note: "Upload and Sync" and "Download" are not handled here as they are not deletion operations.
            // They are included in the dialog for user convenience but would need separate handling if selected.
            // For now, we treat them as a no-op if they reach here.

            // --- Post-Action Refresh for synchronous operations ---
            screen_.Post([this, refresh_books]{
                refresh_books();
                if (app_state_.selected_book_index >= app_state_.books.size()) {
                    app_state_.selected_book_index = app_state_.books.empty() ? 0 : app_state_.books.size() - 1;
                }
                app_state_.current_view = View::Library;
                screen_.Post(Event::Custom);
            });
        }).detach();
        return true;
    }

    if (ui_components_) {
        return ui_components_->GetDeleteMenu()->OnEvent(event);
    }
    return false;
}

bool EventHandlers::HandleSystemInfoEvents(Event event) {
    if (event == Event::Escape) {
        app_state_.current_view = View::Library;
        return true;
    }
    return false;
}
