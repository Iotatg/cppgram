// Markdown subset -> plain text + messageEntity list (pyrogram parser
// semantics for the common subset). Supported:
//   **bold**  __italic__  *italic*  _italic_  --underline--  ~~strike~~
//   ||spoiler||  `code`  ```pre```  [text](url)
// Entities carry UTF-16 code-unit offsets/lengths, which is what Telegram
// expects; for BMP-only text this equals the character count.
#include "cppgram/client.hpp"
#include "cppgram/args.hpp"

#include <cctype>
#include <cstring>

namespace cppgram {

namespace {

struct Ent {
    std::string ctor;
    size_t start;
    size_t len;
    std::string extra;  // url or language
};

size_t utf16_len(const std::string& s, size_t from, size_t to) {
    size_t units = 0;
    for (size_t i = from; i < to;) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c >= 0xF0) i += 4;
        else if (c >= 0xE0) i += 3;
        else if (c >= 0xC0) i += 2;
        else i += 1;
        units += (c >= 0xF0) ? 2 : 1;
    }
    return units;
}

// Telegram entity offsets are UTF-16 code units, not byte offsets.
size_t utf16_offset(const std::string& s, size_t byte_pos) {
    return utf16_len(s, 0, byte_pos);
}

}  // namespace

std::vector<Val> parse_markdown(const std::string& md, std::string* plain_out) {
    std::string out;
    std::vector<Ent> ents;
    size_t i = 0;
    while (i < md.size()) {
        char c = md[i];
        // ```pre``` blocks
        if (c == '`' && md.compare(i, 3, "```") == 0) {
            size_t end = md.find("```", i + 3);
            if (end != std::string::npos) {
                size_t cs = out.size();
                out += md.substr(i + 3, end - i - 3);
                ents.push_back({"messageEntityPre", utf16_offset(out, cs), utf16_len(out, cs, out.size()), ""});
                i = end + 3;
                continue;
            }
        }
        // `code`
        if (c == '`') {
            size_t end = md.find('`', i + 1);
            if (end != std::string::npos) {
                size_t cs = out.size();
                out += md.substr(i + 1, end - i - 1);
                ents.push_back({"messageEntityCode", utf16_offset(out, cs), utf16_len(out, cs, out.size()), ""});
                i = end + 1;
                continue;
            }
        }
        // [text](url)
        if (c == '[') {
            size_t cb = md.find(']', i + 1);
            if (cb != std::string::npos && cb + 1 < md.size() && md[cb + 1] == '(') {
                size_t cp = md.find(')', cb + 2);
                if (cp != std::string::npos) {
                    size_t cs = out.size();
                    out += md.substr(i + 1, cb - i - 1);
                    ents.push_back({"messageEntityTextUrl", utf16_offset(out, cs), utf16_len(out, cs, out.size()),
                                    md.substr(cb + 2, cp - cb - 2)});
                    i = cp + 1;
                    continue;
                }
            }
        }
        // paired inline delimiters (pyrogram markdown.py set + * / _ aliases)
        struct Pat {
            const char* tok;
            const char* ctor;
        };
        static const Pat pats[] = {
            {"**", "messageEntityBold"},
            {"__", "messageEntityItalic"},       // pyrogram: __italic__
            {"~~", "messageEntityStrike"},
            {"--", "messageEntityUnderline"},    // pyrogram: --underline--
            {"||", "messageEntitySpoiler"},
            {"*", "messageEntityItalic"},
            {"_", "messageEntityItalic"},
        };
        bool matched = false;
        for (const Pat& p : pats) {
            size_t tl = std::strlen(p.tok);
            if (md.compare(i, tl, p.tok) != 0) continue;
            // avoid ** vs * ambiguity: "**" already matched above
            size_t end = md.find(p.tok, i + tl);
            if (end == std::string::npos || end == i + tl) continue;
            // single "_" must sit on a word boundary so snake_case is left alone
            if (tl == 1 && p.tok[0] == '_') {
                auto wordish = [](char ch) {
                    return std::isalnum(static_cast<unsigned char>(ch)) != 0;
                };
                char before = i > 0 ? md[i - 1] : ' ';
                char after = end + tl < md.size() ? md[end + tl] : ' ';
                if (wordish(before) || wordish(after)) continue;
            }
            size_t cs = out.size();
            out += md.substr(i + tl, end - i - tl);
            ents.push_back({p.ctor, utf16_offset(out, cs), utf16_len(out, cs, out.size()), ""});
            i = end + tl;
            matched = true;
            break;
        }
        if (matched) continue;
        out += c;
        i++;
    }
    if (plain_out) *plain_out = out;

    std::vector<Val> entities;
    entities.reserve(ents.size());
    for (const Ent& e : ents) {
        std::map<std::string, Val> f = {
            {"offset", Val::i64(static_cast<long long>(e.start))},
            {"length", Val::i64(static_cast<long long>(e.len))},
        };
        if (e.ctor == "messageEntityTextUrl") f["url"] = Val::str(e.extra);
        if (e.ctor == "messageEntityPre") f["language"] = Val::str("");
        entities.push_back(Val::obj(e.ctor, std::move(f)));
    }
    return entities;
}

}  // namespace cppgram
