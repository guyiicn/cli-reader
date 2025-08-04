
#include "EpubParser.h"
#include "DebugLogger.h"
#include "HtmlRenderer.h"
#include <iostream>
#include <vector>
#include <zip.h>
#include <tinyxml2.h>
#include <map>
#include <filesystem>

namespace fs = std::filesystem;
using namespace tinyxml2;

// Helper function to read a file from the zip archive into a string
std::string read_zip_file(zip_t* archive, const std::string& filename) {
    zip_stat_t stat;
    zip_stat_init(&stat);
    if (zip_stat(archive, filename.c_str(), 0, &stat) != 0) {
        DebugLogger::log("Error: File not found in zip: " + filename);
        return "";
    }

    zip_file_t* file = zip_fopen(archive, filename.c_str(), 0);
    if (!file) {
        DebugLogger::log("Error: Failed to open file in zip: " + filename);
        return "";
    }

    std::string content(stat.size, '\0');
    zip_fread(file, &content[0], stat.size);
    zip_fclose(file);

    return content;
}

// PIMPL idiom: private implementation details
struct EpubParser::Impl {
    zip_t* archive = nullptr;
    std::string file_path;
    std::string title = "Unknown Title";
    std::string author = "Unknown Author";
    fs::path opf_dir;

    std::vector<BookChapter> chapters;
    std::map<std::string, std::string> manifest;

    ~Impl() {
        if (archive) {
            zip_close(archive);
        }
    }

    void parse() {
        std::string container_xml = read_zip_file(archive, "META-INF/container.xml");
        if (container_xml.empty()) return;

        XMLDocument doc;
        doc.Parse(container_xml.c_str());

        XMLElement* rootfile = doc.FirstChildElement("container")->FirstChildElement("rootfiles")->FirstChildElement("rootfile");
        if (!rootfile) return;
        
        const char* opf_full_path = rootfile->Attribute("full-path");
        if (!opf_full_path) return;

        fs::path opf_path(opf_full_path);
        opf_dir = opf_path.parent_path();
        DebugLogger::log("Found OPF file at: " + std::string(opf_full_path));
        
        parseOpf(opf_full_path);
    }

    // Recursive function to parse navPoints and build a chapter tree
    std::vector<BookChapter> recursiveParseNavPoints(XMLElement* nav_point_element, const fs::path& ncx_dir) {
        std::vector<BookChapter> current_level_chapters;
        if (!nav_point_element) return current_level_chapters;

        for (auto* current_point = nav_point_element; current_point; current_point = current_point->NextSiblingElement("navPoint")) {
            auto* nav_label = current_point->FirstChildElement("navLabel");
            auto* content = current_point->FirstChildElement("content");

            if (nav_label && content) {
                auto* text_el = nav_label->FirstChildElement("text");
                const char* src_attr = content->Attribute("src");
                if (text_el && src_attr) {
                    BookChapter chapter;
                    chapter.title = text_el->GetText();

                    fs::path content_path = ncx_dir / src_attr;
                    std::string clean_content_path = content_path.lexically_normal().string();
                    
                    size_t fragment_pos = clean_content_path.find('#');
                    if (fragment_pos != std::string::npos) {
                        clean_content_path = clean_content_path.substr(0, fragment_pos);
                    }

                    std::string html_content = read_zip_file(archive, clean_content_path);
                    chapter.paragraphs = HtmlRenderer::ToParagraphs(html_content);
                    
                    // Recursively parse nested navPoints to get children
                    chapter.children = recursiveParseNavPoints(current_point->FirstChildElement("navPoint"), ncx_dir);

                    current_level_chapters.push_back(chapter);
                }
            }
        }
        return current_level_chapters;
    }

    void parseNcx(const std::string& ncx_path_str) {
        DebugLogger::log("Parsing NCX file: " + ncx_path_str);
        std::string ncx_xml = read_zip_file(archive, ncx_path_str);
        if (ncx_xml.empty()) return;

        XMLDocument doc;
        doc.Parse(ncx_xml.c_str());

        auto* nav_map = doc.FirstChildElement("ncx")->FirstChildElement("navMap");
        if (!nav_map) return;

        chapters = recursiveParseNavPoints(nav_map->FirstChildElement("navPoint"), fs::path(ncx_path_str).parent_path());
    }

    void parseOpf(const std::string& opf_path_str) {
        std::string opf_xml = read_zip_file(archive, opf_path_str);
        if (opf_xml.empty()) return;

        XMLDocument doc;
        doc.Parse(opf_xml.c_str());

        auto* package = doc.FirstChildElement("package");
        if (!package) return;

        // Parse metadata
        auto* metadata = package->FirstChildElement("metadata");
        if (metadata) {
            auto* title_el = metadata->FirstChildElement("dc:title");
            if (title_el) title = title_el->GetText();
            auto* creator_el = metadata->FirstChildElement("dc:creator");
            if (creator_el) author = creator_el->GetText();
        }

        // Parse manifest to find NCX
        std::string ncx_href;
        auto* manifest_el = package->FirstChildElement("manifest");
        if (manifest_el) {
            for (auto* item = manifest_el->FirstChildElement("item"); item; item = item->NextSiblingElement("item")) {
                const char* id = item->Attribute("id");
                const char* href = item->Attribute("href");
                const char* media_type = item->Attribute("media-type");
                if (id && href) {
                    manifest[id] = (opf_dir / href).lexically_normal().string();
                    if (media_type && std::string(media_type) == "application/x-dtbncx+xml") {
                        ncx_href = manifest[id];
                    }
                }
            }
        }
        
        auto* spine_el = package->FirstChildElement("spine");
        if (ncx_href.empty() && spine_el && spine_el->Attribute("toc")) {
            std::string toc_id = spine_el->Attribute("toc");
            if (manifest.count(toc_id)) {
                ncx_href = manifest[toc_id];
            }
        }

        if (!ncx_href.empty()) {
            parseNcx(ncx_href);
        } else {
            DebugLogger::log("Warning: NCX file not found. Building chapters from spine.");
            // Fallback: build chapters from spine if no NCX is found
            if (spine_el) {
                for (auto* itemref = spine_el->FirstChildElement("itemref"); itemref; itemref = itemref->NextSiblingElement("itemref")) {
                    const char* idref = itemref->Attribute("idref");
                    if (idref && manifest.count(idref)) {
                        BookChapter chapter;
                        std::string content_path = manifest[idref];
                        chapter.title = fs::path(content_path).stem().string();
                        std::string html_content = read_zip_file(archive, content_path);
                        chapter.paragraphs = HtmlRenderer::ToParagraphs(html_content);
                        chapters.push_back(chapter);
                    }
                }
            }
        }
    }
};

EpubParser::EpubParser(const std::string& file_path) : pimpl_(std::make_unique<Impl>()) {
    pimpl_->file_path = file_path;
    DebugLogger::init("debug.log");
    DebugLogger::log("--- Starting EPUB Parse for: " + file_path + " ---");
    int error = 0;
    pimpl_->archive = zip_open(file_path.c_str(), 0, &error);

    if (!pimpl_->archive) {
        zip_error_t zip_err;
        zip_error_init_with_code(&zip_err, error);
        DebugLogger::log("Fatal: Failed to open epub file: " + std::string(zip_error_strerror(&zip_err)));
        zip_error_fini(&zip_err);
        return;
    }

    pimpl_->parse();
}

EpubParser::~EpubParser() = default;

bool EpubParser::isOpen() const { return pimpl_->archive != nullptr; }
std::string EpubParser::GetTitle() const { return pimpl_->title; }
std::string EpubParser::GetAuthor() const { return pimpl_->author; }
std::string EpubParser::GetType() const { return "EPUB"; }
std::string EpubParser::GetFilePath() const { return pimpl_->file_path; }
const std::vector<BookChapter>& EpubParser::GetChapters() const { return pimpl_->chapters; }
