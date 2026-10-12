// Implementation of the kurigram parse-mode dispatch (parser/parser.py).
#include "cppgram/parser.hpp"

#include <stdexcept>

namespace cppgram {

TextEntities Parser::parse(const std::string& text, int parse_mode) const {
    // kurigram strips the text before parsing.
    size_t b = 0, e = text.size();
    while (b < e && std::isspace(static_cast<unsigned char>(text[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(text[e - 1]))) --e;
    const std::string stripped = text.substr(b, e - b);

    if (parse_mode == enums::ParseMode::DEFAULT) return Markdown{}.parse(stripped, false);
    if (parse_mode == enums::ParseMode::MARKDOWN) return Markdown{}.parse(stripped, true);
    if (parse_mode == enums::ParseMode::HTML) return Html{}.parse(stripped);
    if (parse_mode == enums::ParseMode::DISABLED)
        return TextEntities{stripped, std::vector<Entity>{}};
    throw std::invalid_argument("Invalid parse mode");
}

std::string Parser::unparse(const std::string& text, std::vector<Entity> entities, bool is_html) {
    return is_html ? Html::unparse(text, std::move(entities))
                   : Markdown::unparse(text, std::move(entities));
}

}  // namespace cppgram
