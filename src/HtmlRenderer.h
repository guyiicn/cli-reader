#ifndef HTML_RENDERER_H
#define HTML_RENDERER_H

#include <gumbo.h>
#include <string>
#include <vector>
#include <utility> // For std::pair

namespace HtmlRenderer {
    std::vector<std::string> ToParagraphs(const std::string& html);
    std::pair<std::string, std::vector<std::string>> ExtractTitleAndParagraphs(const std::string& html);
}

#endif // HTML_RENDERER_H
