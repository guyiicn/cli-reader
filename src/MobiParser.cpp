#include "MobiParser.h"
#include "DebugLogger.h"
#include "HtmlRenderer.h"
#include <mobi.h>
#include <filesystem>
#include <functional>
#include <map>
#include <vector>

namespace fs = std::filesystem;

// PIMPL idiom for private implementation
struct MobiParser::Impl {
    std::string file_path;
    std::string title = "Unknown Title";
    std::string author = "Unknown Author";
    std::vector<BookChapter> chapters;

    ~Impl() {}

    void parse() {
        // 1. Initialize MOBI library and load file
        MOBIData* mobi_data = mobi_init();
        if (mobi_data == nullptr) {
            DebugLogger::log("Error: mobi_init failed");
            return;
        }

        FILE* file = fopen(file_path.c_str(), "rb");
        if (file == nullptr) {
            DebugLogger::log("Error: Failed to open mobi file: " + file_path);
            mobi_free(mobi_data);
            return;
        }

        MOBI_RET mobi_ret = mobi_load_file(mobi_data, file);
        fclose(file);

        if (mobi_ret != MOBI_SUCCESS) {
            DebugLogger::log("Error: mobi_load_file failed with code " + std::to_string(mobi_ret));
            mobi_free(mobi_data);
            return;
        }

        // 2. Extract metadata
        char* mobi_title = mobi_meta_get_title(mobi_data);
        if (mobi_title) {
            title = mobi_title;
            free(mobi_title);
        }
        char* mobi_author = mobi_meta_get_author(mobi_data);
        if (mobi_author) {
            author = mobi_author;
            free(mobi_author);
        }

        // 3. Initialize and parse raw markup with TOC enabled
        MOBIRawml* rawml = mobi_init_rawml(mobi_data);
        if (rawml == nullptr) {
            DebugLogger::log("Error: mobi_init_rawml failed");
            mobi_free(mobi_data);
            return;
        }
        mobi_ret = mobi_parse_rawml_opt(rawml, mobi_data, true, false, true); // Reconstruct parts
        if (mobi_ret != MOBI_SUCCESS) {
            DebugLogger::log("Error: mobi_parse_rawml_opt failed");
            mobi_free_rawml(rawml);
            mobi_free(mobi_data);
            return;
        }

        // 4. Pre-load ALL available HTML content parts into a map for easy access
        std::map<size_t, MOBIPart*> content_map;
        MOBIPart* part_iterator = rawml->markup;
        while(part_iterator) {
            if (part_iterator->type == T_HTML) {
                content_map[part_iterator->uid] = part_iterator;
            }
            part_iterator = part_iterator->next;
        }
        part_iterator = rawml->flow;
         while(part_iterator) {
            if (part_iterator->type == T_HTML) {
                content_map[part_iterator->uid] = part_iterator;
            }
            part_iterator = part_iterator->next;
        }

        // 5. Build chapters from the NCX index if it exists
        if (rawml->ncx && rawml->ncx->entries_count > 0) {
            DebugLogger::log("Found NCX with " + std::to_string(rawml->ncx->entries_count) + " entries. Building chapters from TOC.");
            
            // Create a map of parent index to list of child indices
            std::map<size_t, std::vector<size_t>> parent_child_map;
            std::vector<size_t> root_entries;

            for (size_t i = 0; i < rawml->ncx->entries_count; ++i) {
                MOBIIndexEntry* entry = &rawml->ncx->entries[i];
                bool has_parent = false;
                // Find the parent tag (tagid 21 is typically the parent tag)
                for (size_t j = 0; j < entry->tags_count; ++j) {
                    if (entry->tags[j].tagid == 21) {
                        parent_child_map[entry->tags[j].tagvalues[0]].push_back(i);
                        has_parent = true;
                        break;
                    }
                }
                if (!has_parent) {
                    root_entries.push_back(i);
                }
            }
            
            // Recursive function to build the chapter tree
            std::function<BookChapter(size_t)> build_chapter_tree = 
                [&](size_t entry_index) -> BookChapter {
                MOBIIndexEntry* entry = &rawml->ncx->entries[entry_index];
                BookChapter chapter;

                // Find the content for this chapter
                size_t content_uid = 0;
                for (size_t i = 0; i < entry->tags_count; ++i) {
                    if (entry->tags[i].tagid == 6) { // tagid 6 is start position
                        content_uid = entry->tags[i].tagvalues[0];
                        break;
                    }
                }

                if (content_map.count(content_uid)) {
                    MOBIPart* content_part = content_map[content_uid];
                    std::string html_content(reinterpret_cast<char*>(content_part->data), content_part->size);
                    
                    auto result = HtmlRenderer::ExtractTitleAndParagraphs(html_content);
                    std::string real_title = result.first;
                    
                    if (!real_title.empty()) {
                        chapter.title = real_title;
                    } else if (entry->label) {
                        chapter.title = entry->label; // Fallback to NCX label
                    } else {
                        chapter.title = "Untitled Chapter";
                    }
                    chapter.paragraphs = result.second;

                } else {
                    // Fallback for content not found
                    if (entry->label) {
                        chapter.title = entry->label;
                    } else {
                        chapter.title = "Untitled Chapter (Content Not Found)";
                    }
                }

                // Recurse for children
                if (parent_child_map.count(entry_index)) {
                    for (size_t child_index : parent_child_map[entry_index]) {
                        chapter.children.push_back(build_chapter_tree(child_index));
                    }
                }
                return chapter;
            };

            // Build the tree starting from the root entries
            for (size_t root_index : root_entries) {
                chapters.push_back(build_chapter_tree(root_index));
            }
        } 
        
        // 6. Ultimate fallback: if still no chapters, process the entire markup as a single chapter
        if (chapters.empty()) {
            DebugLogger::log("No chapters built from NCX. Using fallback on flow/markup.");
            MOBIPart* part = rawml->flow ? rawml->flow : rawml->markup;
            while (part != nullptr) {
                if (part->type == T_HTML) {
                    BookChapter chapter;
                    chapter.title = "Chapter " + std::to_string(chapters.size() + 1);
                    std::string html_content(reinterpret_cast<char*>(part->data), part->size);
                    chapter.paragraphs = HtmlRenderer::ToParagraphs(html_content);
                    if (!chapter.paragraphs.empty()) {
                        chapters.push_back(chapter);
                    }
                }
                part = part->next;
            }
        }

        // 7. Clean up
        mobi_free_rawml(rawml);
        mobi_free(mobi_data);
        DebugLogger::log("MOBI parse complete. Total chapters found: " + std::to_string(chapters.size()));
    }
};

MobiParser::MobiParser(const std::string& file_path) : pimpl_(std::make_unique<Impl>()) {
    pimpl_->file_path = file_path;
    DebugLogger::init("debug.log");
    DebugLogger::log("--- Starting MOBI Parse for: " + file_path + " ---");
    pimpl_->parse();
}

MobiParser::~MobiParser() = default;

std::string MobiParser::GetTitle() const { return pimpl_->title; }
std::string MobiParser::GetAuthor() const { return pimpl_->author; }
std::string MobiParser::GetType() const { return "MOBI"; }
std::string MobiParser::GetFilePath() const { return pimpl_->file_path; }
const std::vector<BookChapter>& MobiParser::GetChapters() const { return pimpl_->chapters; }




