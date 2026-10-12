// kurigram `pyrogram/parser/parser.py` parity: parse-mode dispatch.
//
//   Parser p;
//   TextEntities te = p.parse("**bold**", enums::ParseMode::MARKDOWN);
//   te.entities -> raw messageEntity ctor trees in .entities
//
// Mirrors the reference: DEFAULT -> markdown, MARKDOWN -> markdown strict,
// HTML -> html, DISABLED -> pass-through.
#pragma once

#include "cppgram/enums.hpp"
#include "cppgram/html.hpp"
#include "cppgram/markdown.hpp"

#include <cctype>
#include <string>
#include <vector>

namespace cppgram {

enum class ParseModeKind { Default, Markdown, Html, Disabled };

struct Parser {
    // mode is one of the ParseMode enum values (see enums.hpp); anything else is
    // treated as DISABLED.
    TextEntities parse(const std::string& text, int parse_mode) const;
    static std::string unparse(const std::string& text, std::vector<Entity> entities, bool is_html);
};

}  // namespace cppgram
