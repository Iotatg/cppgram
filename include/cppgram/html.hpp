// kurigram `pyrogram/parser/html.py` parity: HTML parsing / unparsing of
// message text and entities.
//
//   Html h;
//   auto te = h.parse("<b>bold</b> plain");
//   te.message == "bold plain", te.entities[0] -> messageEntityBold {0,4}
//
//   Html::unparse(te.message, te.entities) -> "<b>bold</b> plain"
//
// Offsets and lengths are counted in UTF-16 code units, the same unit Telegram
// itself and kurigram (by way of add_surrogates) use. Unknown tags are skipped,
// unmatched closing tags are ignored, zero-length entities are dropped, exactly
// like the reference implementation.
#pragma once

#include "cppgram/args.hpp"
#include "cppgram/client.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cppgram {

// Telegram counts entity offsets/lengths in UTF-16 code units. These two helpers
// are the single place where that conversion happens (the html and markdown
// parsers work on utf-16 buffers and convert only at their edges).
namespace utf16 {

std::u16string encode(const std::string& utf8);
std::string decode(const std::u16string& units);

}  // namespace utf16

// One raw.types.MessageEntity* constructor, extras kept apart from offset/length.
struct Entity {
    std::string ctor;  // "messageEntityBold", "messageEntityTextUrl", ...
    int64_t offset{0};
    int64_t length{0};
    std::map<std::string, Val> extra;

    // The raw ctor tree, ready to be handed to Client::api()/frag().
    Val to_val() const;
    // Populate from a raw ctor tree (used by unparse() and by handlers).
    static std::optional<Entity> from_val(const Val& v);
};

// html -> text + entities, text + entities -> html.
struct TextEntities {
    std::string message;
    std::vector<Entity> entities;  // empty when the text carries no markup
};

// Minimal HTML5 entity decoding for the references html.unescape resolves.
std::string html_unescape(const std::string& s);
// Text data goes through the parser's charref pass plus handle_data, so it is
// unescaped twice; attribute values only once.
std::string html_unescape_data(const std::string& s);
std::string html_escape(const std::string& s);

struct Html {
    TextEntities parse(const std::string& text) const;
    static std::string unparse(const std::string& text, std::vector<Entity> entities);

  private:
    friend struct Markdown;
};

}  // namespace cppgram
