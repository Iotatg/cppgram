// kurigram `pyrogram/parser/markdown.py` parity: markdown parsing / unparsing.
//
//   Markdown m;
//   TextEntities te = m.parse("**bold** and [link](https://x.com)");
//   Markdown::unparse(te.message, te.entities) -> "**bold** and [link](https://x.com)"
//
// Supported markup: **bold** __italic__ --underline-- ~~strike~~ ||spoiler||
// `code` ```pre``` [text](url) ![text](tg://emoji?id=...) ![text](tg://time?unix=...)
// plus > quotes, **> expandable quotes ... || and backslash escaping.
// The pipeline is the reference's: escape_and_create_quotes() folds quotes into
// html, delimiter pairs are turned into html tags, then Html::parse() runs.
#pragma once

#include "cppgram/html.hpp"

#include <string>
#include <vector>

namespace cppgram {

struct TextEntities;

struct Markdown {
    TextEntities parse(const std::string& text, bool strict = false) const;
    static std::string unparse(const std::string& text, std::vector<Entity> entities);

  private:
    friend struct MarkdownImpl;
};

}  // namespace cppgram
