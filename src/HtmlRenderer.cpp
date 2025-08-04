#include "HtmlRenderer.h"
#include "gumbo.h"
#include <string>
#include <vector>
#include <algorithm>
#include <utility>

// --- Helper Functions (Anonymous Namespace) ---

namespace {

// Helper to trim leading/trailing whitespace
std::string trim(const std::string& s) {
    auto first = s.find_first_not_of(" \t\n\r\f\v");
    if (std::string::npos == first) {
        return "";
    }
    auto last = s.find_last_not_of(" \t\n\r\f\v");
    return s.substr(first, (last - first + 1));
}

// Original recursive function for ToParagraphs
void extract_text_from_node(GumboNode* node, std::string& text_buffer, std::vector<std::string>& paragraphs) {
    if (node->type == GUMBO_NODE_TEXT) {
        text_buffer += node->v.text.text;
    } else if (node->type == GUMBO_NODE_ELEMENT &&
               node->v.element.tag != GUMBO_TAG_SCRIPT &&
               node->v.element.tag != GUMBO_TAG_STYLE) {
        
        GumboTag tag = node->v.element.tag;
        bool is_block = (tag == GUMBO_TAG_P || tag == GUMBO_TAG_DIV ||
                         tag == GUMBO_TAG_H1 || tag == GUMBO_TAG_H2 ||
                         tag == GUMBO_TAG_H3 || tag == GUMBO_TAG_H4 ||
                         tag == GUMBO_TAG_H5 || tag == GUMBO_TAG_H6 ||
                         tag == GUMBO_TAG_LI || tag == GUMBO_TAG_BR);

        GumboVector* children = &node->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
            extract_text_from_node(static_cast<GumboNode*>(children->data[i]), text_buffer, paragraphs);
        }

        if (is_block) {
            std::string trimmed_text = trim(text_buffer);
            if (!trimmed_text.empty()) {
                paragraphs.push_back(trimmed_text);
            }
            paragraphs.push_back(""); // Represents a line break
            text_buffer.clear();
        }
    }
}

// New recursive helper for ExtractTitleAndParagraphs
void extract_title_and_text_from_node(GumboNode* node, std::string& title, bool& title_found, std::string& text_buffer, std::vector<std::string>& paragraphs) {
    if (node->type == GUMBO_NODE_TEXT) {
        text_buffer += node->v.text.text;
        return;
    }
    
    if (node->type != GUMBO_NODE_ELEMENT ||
        node->v.element.tag == GUMBO_TAG_SCRIPT ||
        node->v.element.tag == GUMBO_TAG_STYLE) {
        return;
    }

    GumboTag tag = node->v.element.tag;
    bool is_header = (tag >= GUMBO_TAG_H1 && tag <= GUMBO_TAG_H6);

    // If this is the first header we've found, extract it as the title and stop processing it.
    if (is_header && !title_found) {
        title_found = true;
        std::string temp_buffer;
        // Special extraction just for the title text
        GumboVector* children = &node->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
             if (static_cast<GumboNode*>(children->data[i])->type == GUMBO_NODE_TEXT) {
                temp_buffer += static_cast<GumboNode*>(children->data[i])->v.text.text;
            }
        }
        title = trim(temp_buffer);
        return; // Do not process children or add to paragraphs
    }

    // For all other nodes, process them normally.
    bool is_block = (tag == GUMBO_TAG_P || tag == GUMBO_TAG_DIV || is_header ||
                     tag == GUMBO_TAG_LI || tag == GUMBO_TAG_BR);

    GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
        extract_title_and_text_from_node(static_cast<GumboNode*>(children->data[i]), title, title_found, text_buffer, paragraphs);
    }

    if (is_block) {
        std::string trimmed_text = trim(text_buffer);
        if (!trimmed_text.empty()) {
            paragraphs.push_back(trimmed_text);
        }
        paragraphs.push_back(""); // Represents a line break
        text_buffer.clear();
    }
}

} // Anonymous namespace

// --- Public Interface ---

namespace HtmlRenderer {

std::vector<std::string> ToParagraphs(const std::string& html_content) {
    std::vector<std::string> paragraphs;
    if (html_content.empty()) {
        return paragraphs;
    }

    GumboOutput* output = gumbo_parse_with_options(&kGumboDefaultOptions, html_content.c_str(), html_content.length());
    if (!output) {
        return {"[HTML Parse Error]"};
    }

    std::string text_buffer;
    extract_text_from_node(output->root, text_buffer, paragraphs);

    std::string final_text = trim(text_buffer);
    if (!final_text.empty()) {
        paragraphs.push_back(final_text);
    }
    
    gumbo_destroy_output(&kGumboDefaultOptions, output);
    return paragraphs;
}

std::pair<std::string, std::vector<std::string>> ExtractTitleAndParagraphs(const std::string& html_content) {
    std::string title;
    std::vector<std::string> paragraphs;
    bool title_found = false;

    if (html_content.empty()) {
        return {title, paragraphs};
    }

    GumboOutput* output = gumbo_parse_with_options(&kGumboDefaultOptions, html_content.c_str(), html_content.length());
    if (!output) {
        paragraphs.push_back("[HTML Parse Error]");
        return {title, paragraphs};
    }

    std::string text_buffer;
    extract_title_and_text_from_node(output->root, title, title_found, text_buffer, paragraphs);

    std::string final_text = trim(text_buffer);
    if (!final_text.empty()) {
        paragraphs.push_back(final_text);
    }

    gumbo_destroy_output(&kGumboDefaultOptions, output);
    return {title, paragraphs};
}

} // namespace HtmlRenderer
