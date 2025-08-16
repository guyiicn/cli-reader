#include "UIComponents.h"
#include <algorithm>
#include <ctime>
#include <iomanip>

UIComponents::UIComponents(AppState& state, ScreenInteractive& screen) 
    : app_state_(state), screen_(screen) {}

void UIComponents::CreateComponents() {
    // Create menu components
    library_menu_ = Menu(&app_state_.library_visible_books, &app_state_.selected_book_index);
    picker_menu_ = Menu(&app_state_.picker_entries, &app_state_.selected_picker_entry);
    toc_menu_ = Menu(&app_state_.toc_visible_entries, &app_state_.selected_toc_entry);
    
    // Create button components
    ok_button_ = Button(" OK ", [&]{ 
        app_state_.current_view = View::Library; 
    });

    confirm_ocr_container_ = Container::Horizontal({
        Button(" Yes ", [&] { 
            app_state_.message_to_show = "OCR功能尚未实现。建议使用文本版PDF或其他格式。";
            app_state_.current_view = View::ShowMessage;
        }), 
        Button(" No ", [&] { 
            app_state_.current_view = View::Library; 
        })
    });

    delete_menu_ = Menu(&app_state_.delete_options, &app_state_.selected_delete_option);
    delete_confirm_renderer_ = Renderer(delete_menu_, [&] {
        return vbox({
            text("Delete Book: " + app_state_.title_to_delete) | bold,
            separator(),
            text("Please choose an option:"),
            delete_menu_->Render() | vscroll_indicator | frame | flex,
            separator(),
            text("[Enter] Confirm | [Esc] Cancel")
        }) | border | center;
    });

    // Create main tab container
    main_container_ = Container::Tab({
        library_menu_,
        Renderer([]{ return text("Reader placeholder"); }),
        toc_menu_,
        picker_menu_,
        ok_button_,
        Renderer([]{ return text("Loading placeholder"); }),
        confirm_ocr_container_,
        delete_confirm_renderer_
    }, (int*)&app_state_.current_view);

    // Create modal components
    auto modal_ok_button = Button(&app_state_.modal_ok_label, [&] { 
        if (app_state_.modal_ok_action) app_state_.modal_ok_action();
        app_state_.show_modal = false; 
    });

    auto modal_cancel_button = Button(&app_state_.modal_cancel_label, [&] {
        if (app_state_.modal_cancel_action) app_state_.modal_cancel_action();
        app_state_.show_modal = false;
    });

    auto modal_layout = Container::Horizontal({modal_ok_button, modal_cancel_button});

    modal_component_ = Renderer(modal_layout, [&, modal_ok_button, modal_cancel_button] {
        Element buttons;
        if (app_state_.show_modal_cancel_button) {
            buttons = hbox(modal_ok_button->Render(), separator(), modal_cancel_button->Render());
        } else {
            buttons = modal_ok_button->Render();
        }
        return vbox({ 
            text(app_state_.modal_title) | bold, 
            separator(), 
            text(app_state_.modal_content), 
            separator(), 
            buttons | center 
        }) | border | size(WIDTH, EQUAL, 60) | center;
    });

    root_container_ = Modal(main_container_, modal_component_, &app_state_.show_modal);
}

Component UIComponents::GetMainContainer() {
    return root_container_;
}

Component UIComponents::GetDeleteMenu() {
    return delete_menu_;
}

Component UIComponents::GetPickerMenu() {
    return picker_menu_;
}

Element UIComponents::RenderLibraryView() {
    // Update pagination
    if (screen_.dimx() != app_state_.last_library_width || screen_.dimy() != app_state_.last_library_height) {
        app_state_.last_library_width = screen_.dimx();
        app_state_.last_library_height = screen_.dimy();
        app_state_.library_entries_per_page = app_state_.last_library_height > 8 ? app_state_.last_library_height - 8 : 1;
        app_state_.library_total_pages = app_state_.books.empty() ? 1 : (app_state_.books.size() + app_state_.library_entries_per_page - 1) / app_state_.library_entries_per_page;
        if (app_state_.library_current_page >= app_state_.library_total_pages) {
            app_state_.library_current_page = std::max(0, app_state_.library_total_pages - 1);
        }
    }
    
    int start_index = app_state_.library_current_page * app_state_.library_entries_per_page;
    int end_index = std::min(start_index + app_state_.library_entries_per_page, (int)app_state_.books.size());
    
    app_state_.library_visible_books.clear();
    if (start_index < end_index) {
        app_state_.library_visible_books.assign(app_state_.book_display_list.begin() + start_index, app_state_.book_display_list.begin() + end_index);
    }

    // Footer Logic
    std::string footer_text = "[a] Add | [s] System Info | [q] Quit";
    if (app_state_.cloud_sync_enabled) {
        footer_text += " | [c] Cloud Off | [r] Refresh";
    } else {
        footer_text += " | [c] Cloud On";
    }

    // Context-sensitive part of the footer
    if (app_state_.cloud_sync_enabled && !app_state_.books.empty()) {
        int global_index = app_state_.library_current_page * app_state_.library_entries_per_page + app_state_.selected_book_index;
        if (global_index < app_state_.books.size()) {
            const auto& book = app_state_.books[global_index];
            if (book.sync_status == "local") footer_text += " | [u] Upload";
            if (book.sync_status == "cloud") footer_text += " | [Enter] Download";
            if (book.sync_status == "local" || book.sync_status == "synced") footer_text += " | [d] Delete";
        }
    } else if (!app_state_.cloud_sync_enabled && !app_state_.books.empty()) {
        footer_text += " | [d] Delete";
    }

    // Sync status message
    Element sync_status_element;
    SyncStatus current_sync_status = app_state_.sync_status;
    if (current_sync_status == SyncStatus::IN_PROGRESS) {
        sync_status_element = text(" ☁️ " + app_state_.sync_message) | blink;
    } else if (current_sync_status == SyncStatus::SUCCESS) {
        sync_status_element = text(" ✓ " + app_state_.sync_message);
    } else if (current_sync_status == SyncStatus::ERROR) {
        sync_status_element = text(" ✗ " + app_state_.sync_message);
    } else {
        sync_status_element = text("");
    }

    // Clock
    time_t now = time(0);
    struct tm ltm;
    localtime_r(&now, &ltm);
    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &ltm);
    Element clock_element = text(std::string(time_buf)) | dim;

    auto footer = hbox({
        text(footer_text),
        filler(),
        sync_status_element,
        separator(),
        clock_element
    });

    std::string cloud_icon = app_state_.cloud_sync_enabled ? " ☁️" : " 💻";
    auto title = hbox({ text("Ebook Library") | bold, text(cloud_icon) }) | hcenter;

    return vbox({
        title,
        separator(),
        library_menu_->Render() | vscroll_indicator | frame | flex,
        separator(),
        footer
    }) | border;
}

Element UIComponents::RenderReaderView() {
    if (!app_state_.book_view_model) {
        return text("No book loaded.") | center;
    }
    
    bool is_dual = app_state_.dual_page_mode_enabled && (screen_.dimx() > 100);
    int page_width = is_dual ? (screen_.dimx() / 2) - 4 : screen_.dimx() - 4;
    int page_height = screen_.dimy() - 6;

    if (!app_state_.paginated || page_width != app_state_.last_page_width || page_height != app_state_.last_page_height) {
        app_state_.book_view_model->Paginate(page_width, page_height);
        app_state_.paginated = true;
        app_state_.last_page_width = page_width;
        app_state_.last_page_height = page_height;
    }
    
    std::string progress_str = "Page: " + std::to_string(app_state_.current_page + 1) + " / " + std::to_string(app_state_.book_view_model->GetTotalPages());
    
    std::string book_title;
    int global_index = (app_state_.library_current_page * app_state_.library_entries_per_page) + app_state_.selected_book_index;
    if (global_index < app_state_.books.size()) {
        book_title = app_state_.books[global_index].title;
    }

    std::string chapter_title = app_state_.book_view_model->GetPageTitleForPage(app_state_.current_page);
    std::string full_title = book_title + " - " + chapter_title;
    
    // Page Content
    Element page_content;
    if (is_dual) {
        auto left_page = vbox(app_state_.book_view_model->GetPageContent(app_state_.current_page, page_width)) | vscroll_indicator | frame | flex;
        Element right_page;
        if (app_state_.current_page + 1 < app_state_.book_view_model->GetTotalPages()) {
            right_page = vbox(app_state_.book_view_model->GetPageContent(app_state_.current_page + 1, page_width)) | vscroll_indicator | frame | flex;
        } else {
            right_page = text("") | flex; // Empty placeholder for the last page
        }
        page_content = hbox({left_page, separator(), right_page});
    } else {
        page_content = vbox(app_state_.book_view_model->GetPageContent(app_state_.current_page, page_width)) | vscroll_indicator | frame | flex;
    }

    auto status_bar = hbox({text(progress_str), filler(), text("← Prev|→ Next|[d]Mode|[q]Back|[m]TOC")});
    
    return vbox({
        text(full_title) | bold | hcenter,
        separator(),
        page_content,
        separator(),
        status_bar
    }) | border;
}

Element UIComponents::RenderFilePickerView() {
    return vbox({
        text("Select a Book") | bold | hcenter,
        text("Current Path: " + app_state_.current_picker_path.string()) | color(Color::Yellow),
        separator(),
        picker_menu_->Render() | vscroll_indicator | frame | flex,
        separator(),
        text("[Enter] Select | [Esc] Cancel") | hcenter
    }) | border;
}

Element UIComponents::RenderShowMessageView() {
    return vbox({
        text("Information") | bold | hcenter,
        separator(),
        text(app_state_.message_to_show) | hcenter,
        separator(),
        ok_button_->Render() | hcenter
    }) | border | center;
}

Element UIComponents::RenderLoadingView() {
    return vbox({
        text(app_state_.loading_message) | hcenter,
        text("Please wait...") | hcenter,
    }) | border | center;
}

Element UIComponents::RenderTableOfContentsView() {
    std::string page_str = "Page " + std::to_string(app_state_.toc_current_page + 1) + "/" + std::to_string(app_state_.toc_total_pages);
    return vbox({
        text("Table of Contents") | bold | hcenter,
        separator(),
        toc_menu_->Render() | vscroll_indicator | frame | flex,
        separator(),
        hbox({
            text("[Enter] Go | [Esc] Back"),
            filler(),
            text(page_str),
            filler(),
            text("← Prev | Next →")
        })
    }) | border;
}

Element UIComponents::RenderConfirmOcrView() {
    std::string book_title_to_ocr;
    auto it = std::find_if(app_state_.books.begin(), app_state_.books.end(), 
                          [&](const Book& b){ return b.uuid == app_state_.book_to_action_uuid; });
    if (it != app_state_.books.end()) {
        book_title_to_ocr = it->title;
    }
    
    return vbox({
        text("OCR Required") | bold | hcenter,
        separator(),
        text("This PDF appears to be image-based."),
        text("OCR text extraction is not yet implemented."),
        text("Consider using text-based PDFs or other formats."),
        text(book_title_to_ocr) | bold | hcenter,
        separator(),
        confirm_ocr_container_->Render() | hcenter
    }) | border | center;
}

Element UIComponents::RenderDeleteConfirmView() {
    return delete_confirm_renderer_->Render();
}

Element UIComponents::RenderSystemInfoView() {
    Elements items;
    for (const auto& pair : app_state_.system_info_data) {
        items.push_back(
            hbox({
                text(pair.first) | size(WIDTH, EQUAL, 30),
                text(" = "),
                text(pair.second) | flex,
            })
        );
    }

    auto content = vbox({
        text("System Information") | bold | hcenter,
        separator(),
        vbox(items),
        filler(),
        separator(),
        text("Press 'Esc' to return to the library") | hcenter,
    });

    return window(text(" System Info "), content) | center;
}
