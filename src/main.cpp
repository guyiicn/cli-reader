#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>

#include "Book.h"
#include "BookViewModel.h"
#include "DatabaseManager.h"
#include "DebugLogger.h"
#include "EpubParser.h"
#include "IBookParser.h"
#include "LibraryManager.h"
#include "MobiParser.h"
#include "TxtParser.h"
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

namespace fs = std::filesystem;
using namespace ftxui;

// --- Views ---
enum class View {
    Library,
    Reader,
    TableOfContents,
    FilePicker,
    ConfirmDelete,
    ShowMessage,
};

// --- Parser Factory ---
std::unique_ptr<IBookParser> CreateParser(const std::string& path) {
    std::string extension = fs::path(path).extension().string();
    if (extension == ".epub") return std::make_unique<EpubParser>(path);
    if (extension == ".txt") return std::make_unique<TxtParser>(path);
    if (extension == ".mobi" || extension == ".azw3") return std::make_unique<MobiParser>(path);
    return nullptr;
}

// --- Helper Functions ---

// Recursive helper to flatten the chapter tree for the menu
void FlattenChapters(const std::vector<BookChapter>& chapters, std::vector<std::string>& entries, int depth = 0) {
    for (const auto& chapter : chapters) {
        std::string prefix = std::string(depth * 2, ' '); // Indent by 2 spaces per depth level
        entries.push_back(prefix + chapter.title);
        FlattenChapters(chapter.children, entries, depth + 1);
    }
}

void SortEntries(std::vector<std::string>& entries, const fs::path& p) {
    std::sort(entries.begin(), entries.end(),
              [&](const std::string& a, const std::string& b) {
                  bool is_a_dir = fs::is_directory(p / a);
                  bool is_b_dir = fs::is_directory(p / b);
                  if (is_a_dir != is_b_dir) {
                      return is_a_dir;
                  }
                  return a < b;
              });
}

void UpdatePickerEntries(const fs::path& p, std::vector<std::string>& entries, int& selected_entry) {
    entries.clear();
    selected_entry = 0;
    try {
        if (fs::exists(p) && fs::is_directory(p)) {
            entries.push_back("../");
            std::vector<std::string> child_entries;
            for (const auto& entry : fs::directory_iterator(p)) {
                try {
                    std::string filename = entry.path().filename().string();
                    if (fs::is_directory(entry.path())) {
                        child_entries.push_back(filename + "/");
                    } else {
                        std::string ext = entry.path().extension().string();
                        if (ext == ".epub" || ext == ".txt" || ext == ".mobi" || ext == ".azw3") {
                            child_entries.push_back(filename);
                        }
                    }
                } catch (const fs::filesystem_error&) {}
            }
            SortEntries(child_entries, p);
            entries.insert(entries.end(), child_entries.begin(), child_entries.end());
        }
    } catch (const fs::filesystem_error&) {
        entries.push_back("Error accessing directory.");
    }
}

// --- Main Application ---
int main(int argc, char* argv[]) {
    auto screen = ScreenInteractive::Fullscreen();

    // --- Managers and State ---
    LibraryManager library_manager;
    DatabaseManager db_manager(library_manager.GetLibraryPath() + "/library.db");
    db_manager.InitDatabase();

    View current_view = View::Library;
    std::vector<Book> books;
    std::vector<std::string> book_display_list;
    std::vector<std::string> library_visible_books;
    int selected_book_index = 0;
    int library_current_page = 0;
    int library_total_pages = 1;
    int library_entries_per_page = 1;
    int last_library_width = 0;
    int last_library_height = 0;

    auto refresh_books = [&]() {
        books = db_manager.GetAllBooks();
        book_display_list.clear();
        for (const auto& book : books) {
            int progress = 0;
            if (book.total_pages > 0) {
                progress = static_cast<int>((static_cast<float>(book.current_page) / book.total_pages) * 100);
            }
            book_display_list.push_back(
                book.title + " - " + book.author + " [" + std::to_string(progress) + "%]");
        }
        // Force a pagination recalculation on the next render pass
        last_library_width = 0;
        last_library_height = 0;
    };
    refresh_books();

    std::unique_ptr<BookViewModel> book_view_model = nullptr;
    bool paginated = false;
    int current_page = 0;
    int book_to_delete_id = -1;
    std::string message_to_show;

    // TOC State
    std::vector<std::string> toc_entries;
    std::vector<std::string> toc_visible_entries;
    int selected_toc_entry = 0;
    int toc_current_page = 0;
    int toc_total_pages = 1;
    int toc_entries_per_page = 1;

    // File Picker State
    fs::path current_picker_path = fs::current_path(); // Default
    std::string picker_config_path = library_manager.GetLibraryPath() + "/last_picker_path.txt";
    std::ifstream picker_config_in(picker_config_path);
    if (picker_config_in) {
        std::string last_path;
        if (std::getline(picker_config_in, last_path)) {
            if (fs::exists(last_path) && fs::is_directory(last_path)) {
                current_picker_path = last_path;
            }
        }
    }
    std::vector<std::string> picker_entries;
    int selected_picker_entry = 0;
    UpdatePickerEntries(current_picker_path, picker_entries, selected_picker_entry);

    // --- Handle Command-Line Arguments ---
    if (argc > 1) {
        std::string path_from_args = argv[1];
        if (fs::exists(path_from_args)) {
            message_to_show = library_manager.AddBook(path_from_args, db_manager, screen.dimx(), screen.dimy());
            refresh_books();
            current_view = View::ShowMessage;
        } else {
            message_to_show = "Error: File provided via command-line does not exist.";
            current_view = View::ShowMessage;
        }
    }

    // --- UI Components ---

    // Library View
    auto library_menu = Menu(&library_visible_books, &selected_book_index);
    auto library_component = Renderer(library_menu, [&] {
        // --- New Pagination Logic ---
        // Recalculate pagination only when screen size changes.
        if (screen.dimx() != last_library_width || screen.dimy() != last_library_height) {
            last_library_width = screen.dimx();
            last_library_height = screen.dimy();

            library_entries_per_page = last_library_height - 8;
            if (library_entries_per_page <= 0) library_entries_per_page = 1;
            
            if (book_display_list.empty()) {
                library_total_pages = 1;
            } else {
                library_total_pages = (book_display_list.size() + library_entries_per_page - 1) / library_entries_per_page;
            }
            
            if (library_current_page >= library_total_pages) {
                library_current_page = std::max(0, library_total_pages - 1);
            }
        }

        // Always update visible entries for the current page on every render.
        int start_index = library_current_page * library_entries_per_page;
        int end_index = std::min(start_index + library_entries_per_page, (int)book_display_list.size());
        
        if (start_index < end_index) {
            library_visible_books.assign(book_display_list.begin() + start_index, book_display_list.begin() + end_index);
        } else {
            library_visible_books.clear();
        }
        // --- End of New Logic ---

        std::string page_str = "Page " + std::to_string(library_current_page + 1) + "/" + std::to_string(library_total_pages);
        return vbox({
                   text("Ebook Reader Library") | bold | hcenter,
                   separator(),
                   library_menu->Render() | vscroll_indicator | frame | flex,
                   separator(),
                   hbox({
                       text("[Enter] Open | [a] Add | [d] Delete"),
                       filler(),
                       text(page_str),
                       filler(),
                       text("← Prev | Next → | [q] Quit")
                   })
               }) | border;
    });

    // Reader View
    auto reader_component = Renderer([&] {
        if (!book_view_model) return text("No book loaded.");
        if (!paginated) {
            book_view_model->Paginate(screen.dimx() - 2, screen.dimy() - 6);
            paginated = true;
        }
        std::string progress_str = "Page: " + std::to_string(current_page + 1) + "/" + std::to_string(book_view_model->GetTotalPages());
        std::time_t t = std::time(nullptr);
        std::tm tm = *std::localtime(&t);
        std::stringstream time_ss;
        time_ss << std::put_time(&tm, "%H:%M:%S");
        auto status_bar = hbox({text(progress_str), filler(), text(time_ss.str()), filler(), text("← Prev | → Next | l Lib | m TOC | q Quit")});
        return vbox({text(book_view_model->GetPageTitleForPage(current_page)) | bold | hcenter,
                     separator(),
                     vbox(book_view_model->GetPageContent(current_page)) | vscroll_indicator | frame | flex,
                     separator(),
                     status_bar}) | border;
    });

    // TOC View
    auto toc_menu = Menu(&toc_visible_entries, &selected_toc_entry);
    auto toc_component = Renderer(toc_menu, [&]{
        std::string page_str = "Page " + std::to_string(toc_current_page + 1) + "/" + std::to_string(toc_total_pages);

        return vbox({
            text("Table of Contents") | bold | hcenter,
            separator(),
            toc_menu->Render() | vscroll_indicator | frame | flex,
            separator(),
            hbox({
                text("[Enter] Go | [Esc] Back"),
                filler(),
                text(page_str),
                filler(),
                text("← Prev | Next →")
            })
        }) | border;
    });

    // File Picker View
    auto picker_menu = Menu(&picker_entries, &selected_picker_entry);
    auto file_picker_component = Renderer(picker_menu, [&] {
        return vbox({text("Select a Book") | bold | hcenter,
                     text("Current Path: " + current_picker_path.string()) | color(Color::Yellow),
                     separator(),
                     picker_menu->Render() | vscroll_indicator | frame | flex,
                     separator(),
                     text("Shortcuts: [Enter] Select | [Esc] Cancel") | hcenter}) | border;
    });

    // Confirm Delete View
    auto delete_action = [&](bool do_delete) {
        if (do_delete) {
            library_manager.DeleteBook(book_to_delete_id, db_manager);
            refresh_books();
            if (selected_book_index >= books.size()) {
                selected_book_index = books.empty() ? 0 : books.size() - 1;
            }
        }
        current_view = View::Library;
    };
    auto confirm_delete_container = Container::Horizontal({Button(" Yes ", [&] { delete_action(true); }), Button(" No ", [&] { delete_action(false); })});
    auto confirm_delete_component = Renderer(confirm_delete_container, [&] {
        std::string book_title_to_delete;
        if (!books.empty() && selected_book_index < books.size()) {
            book_title_to_delete = books[selected_book_index].title;
        }
        return vbox({text("Confirm Deletion") | bold | hcenter,
                     separator(),
                     text("Are you sure you want to delete this book?"),
                     text(book_title_to_delete) | bold | hcenter,
                     separator(),
                     confirm_delete_container->Render() | hcenter}) | border;
    });
    
    // Message View
    auto ok_button = Button(" OK ", [&]{ current_view = View::Library; });
    auto message_container = Container::Vertical({ ok_button });
    auto message_component = Renderer(message_container, [&]{
        return vbox({
            text("Information") | bold | hcenter,
            separator(),
            text(message_to_show) | hcenter,
            separator(),
            ok_button->Render() | hcenter
        }) | border | center;
    });

    // Main container
    auto main_container = Container::Tab({library_component, reader_component, toc_component, file_picker_component, confirm_delete_component, message_component}, (int*)&current_view);

    // --- Event Handling ---
    auto event_handler = CatchEvent(main_container, [&](Event event) {
        if (event == Event::Character('q')) {
            if (current_view == View::Reader) {
                int global_index = (library_current_page * library_entries_per_page) + selected_book_index;
                if (global_index < books.size()) {
                    db_manager.UpdateProgress(books[global_index].id, current_page);
                }
            }
            screen.Exit();
            return true;
        }

        if (current_view == View::Library) {
            if (event == Event::Return) {
                if (books.empty()) return true;
                int global_index = (library_current_page * library_entries_per_page) + selected_book_index;
                if (global_index >= books.size()) return true;

                const auto& selected_book = books[global_index];
                db_manager.UpdateLastReadTime(selected_book.id);
                book_view_model = std::make_unique<BookViewModel>(CreateParser(selected_book.path));
                current_page = selected_book.current_page;
                paginated = false;
                current_view = View::Reader;
                return true;
            }
            if (event == Event::Character('a')) {
                UpdatePickerEntries(current_picker_path, picker_entries, selected_picker_entry);
                current_view = View::FilePicker;
                return true;
            }
            if (event == Event::Character('d')) {
                if (books.empty()) return true;
                int global_index = (library_current_page * library_entries_per_page) + selected_book_index;
                if (global_index >= books.size()) return true;

                book_to_delete_id = books[global_index].id;
                current_view = View::ConfirmDelete;
                return true;
            }
            if (event == Event::ArrowLeft) {
                if (library_current_page > 0) {
                    library_current_page--;
                    selected_book_index = 0;
                }
                return true;
            }
            if (event == Event::ArrowRight) {
                if (library_current_page < library_total_pages - 1) {
                    library_current_page++;
                    selected_book_index = 0;
                }
                return true;
            }
        } else if (current_view == View::Reader) {
            if (event == Event::Character('l')) {
                int global_index = (library_current_page * library_entries_per_page) + selected_book_index;
                 if (global_index < books.size()) {
                    db_manager.UpdateProgress(books[global_index].id, current_page);
                }
                refresh_books();
                current_view = View::Library;
                return true;
            }
            if (event == Event::Character('m')) {
                toc_entries.clear();
                FlattenChapters(book_view_model->GetChapters(), toc_entries);
                
                toc_entries_per_page = screen.dimy() - 8;
                if (toc_entries_per_page <= 0) toc_entries_per_page = 1;
                
                if (toc_entries.empty()) {
                    toc_total_pages = 1;
                } else {
                    toc_total_pages = (toc_entries.size() + toc_entries_per_page - 1) / toc_entries_per_page;
                }
                toc_current_page = 0;
                
                int start_index = 0;
                int end_index = std::min((int)toc_entries.size(), toc_entries_per_page);
                toc_visible_entries.assign(toc_entries.begin() + start_index, toc_entries.begin() + end_index);
                
                selected_toc_entry = 0;
                current_view = View::TableOfContents;
                return true;
            }
            if (event == Event::ArrowRight) {
                if (book_view_model && current_page < book_view_model->GetTotalPages() - 1) current_page++;
                return true;
            }
            if (event == Event::ArrowLeft) {
                if (current_page > 0) current_page--;
                return true;
            }
        } else if (current_view == View::TableOfContents) {
            if (event == Event::Return) {
                int global_index = (toc_current_page * toc_entries_per_page) + selected_toc_entry;
                if (book_view_model && global_index < toc_entries.size()) {
                    current_page = book_view_model->GetChapterStartPage(global_index);
                }
                current_view = View::Reader;
                return true;
            }
            
            auto update_visible_toc = [&]() {
                int start_index = toc_current_page * toc_entries_per_page;
                int end_index = std::min(start_index + toc_entries_per_page, (int)toc_entries.size());
                toc_visible_entries.assign(toc_entries.begin() + start_index, toc_entries.begin() + end_index);
                selected_toc_entry = 0;
            };

            if (event == Event::ArrowLeft) {
                if (toc_current_page > 0) {
                    toc_current_page--;
                    update_visible_toc();
                }
                return true;
            }
            if (event == Event::ArrowRight) {
                if (toc_current_page < toc_total_pages - 1) {
                    toc_current_page++;
                    update_visible_toc();
                }
                return true;
            }
            if (event == Event::Escape || event == Event::Character('m')) {
                current_view = View::Reader;
                return true;
            }
        } else if (current_view == View::FilePicker) {
            if (event == Event::Return) {
                if (picker_entries.empty()) return true;
                fs::path selected_path = current_picker_path / picker_entries[selected_picker_entry];
                if (fs::is_directory(selected_path)) {
                    current_picker_path = fs::canonical(selected_path);
                    UpdatePickerEntries(current_picker_path, picker_entries, selected_picker_entry);
                } else {
                    // Save the parent path for next time
                    std::ofstream picker_config_out(picker_config_path);
                    if (picker_config_out) {
                        picker_config_out << selected_path.parent_path().string();
                    }
                    message_to_show = library_manager.AddBook(selected_path.string(), db_manager, screen.dimx(), screen.dimy());
                    refresh_books();
                    current_view = View::ShowMessage;
                }
                return true;
            }
            if (event == Event::Escape) {
                current_view = View::Library;
                return true;
            }
        } else if (current_view == View::ConfirmDelete) {
            if (event == Event::Character('y') || event == Event::Character('Y')) {
                delete_action(true);
                return true;
            }
            if (event == Event::Character('n') || event == Event::Character('N') || event == Event::Escape) {
                delete_action(false);
                return true;
            }
        }

        return main_container->OnEvent(event);
    });

    screen.Loop(event_handler);
    return 0;
}
