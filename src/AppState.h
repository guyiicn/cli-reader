#ifndef APPSTATE_H
#define APPSTATE_H

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Book.h"
#include "BookViewModel.h"
#include "CommonTypes.h"
#include "ftxui/component/event.hpp"

namespace fs = std::filesystem;
using namespace ftxui;

// --- App State Management ---
enum class View {
    Library,
    Reader,
    TableOfContents,
    FilePicker,
    ShowMessage,
    Loading,
    ConfirmOcr,
    DeleteConfirm,
    SystemInfo,
    // These are not real views, but states to trigger console interaction
    FirstTimeSetup, 
    BlockingAuth,
    Exiting
};

// Custom events
extern const Event BOOK_LOAD_SUCCESS;
extern const Event BOOK_LOAD_FAILURE;

struct AppState {
    // View
    View current_view = View::Library;

    // Library Data
    std::vector<Book> books;
    std::vector<std::string> book_display_list;
    std::vector<std::string> library_visible_books;
    int selected_book_index = 0;
    int library_current_page = 0;
    int library_total_pages = 1;
    int library_entries_per_page = 1;
    int last_library_width = 0;
    int last_library_height = 0;

    // Reader Data
    std::unique_ptr<BookViewModel> book_view_model = nullptr;
    std::mutex model_mutex;
    std::thread load_thread;
    bool paginated = false;
    int current_page = 0;
    bool dual_page_mode_enabled = false;
    int last_page_width = 0;
    int last_page_height = 0;

    // TOC State
    std::vector<std::string> toc_entries;
    std::vector<std::string> toc_visible_entries;
    int selected_toc_entry = 0;
    int toc_current_page = 0;
    int toc_total_pages = 1;
    int toc_entries_per_page = 1;

    // File Picker State
    fs::path current_picker_path = fs::current_path();
    std::vector<std::string> picker_entries;
    int selected_picker_entry = 0;

    // Modal/Message State
    std::string book_to_action_uuid;
    std::string message_to_show;
    std::string loading_message;

    // Generic Modal State
    bool show_modal = false;
    std::string modal_title;
    std::string modal_content;
    std::function<void()> modal_ok_action = []{};
    std::function<void()> modal_cancel_action = []{};
    std::string modal_ok_label = "OK";
    std::string modal_cancel_label = "Cancel";
    bool show_modal_cancel_button = false;

    // Delete Confirmation State
    std::string uuid_to_delete;
    std::string title_to_delete;
    std::vector<std::string> delete_options;
    int selected_delete_option = 0;

    // Cloud Sync State
    bool cloud_sync_enabled = false;
    std::atomic<SyncStatus> sync_status = SyncStatus::IDLE;
    std::string sync_message;

    // System Info View Data
    std::vector<std::pair<std::string, std::string>> system_info_data;
};

#endif // APPSTATE_H