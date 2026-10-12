// Implementation of the kurigram HTML parser/unparser (parser/html.py).
#include "cppgram/html.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <functional>
#include <stdexcept>

namespace cppgram {

namespace utf16 {

// ---------------- UTF-16 view ----------------
// Telegram counts entity offsets/lengths in UTF-16 code units, so the whole
// parser works on a utf-16 copy of the text and only converts at the edges.

std::u16string encode(const std::string& s) {
    std::u16string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        const auto c = static_cast<unsigned char>(s[i]);
        uint32_t cp = 0;
        int extra = 0;
        if (c < 0x80) {
            cp = c;
        } else if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1Fu;
            extra = 1;
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0Fu;
            extra = 2;
        } else {
            cp = c & 0x07u;
            extra = 3;
        }
        if (i + static_cast<size_t>(extra) >= s.size()) {  // malformed tail: keep the byte
            out.push_back(static_cast<char16_t>(c));
            ++i;
            continue;
        }
        bool ok = true;
        for (int k = 1; k <= extra; ++k) {
            const auto cc = static_cast<unsigned char>(s[i + static_cast<size_t>(k)]);
            if ((cc & 0xC0) != 0x80) {
                ok = false;
                break;
            }
            cp = (cp << 6) | (cc & 0x3Fu);
        }
        if (!ok) {
            out.push_back(static_cast<char16_t>(c));
            ++i;
            continue;
        }
        i += static_cast<size_t>(extra) + 1;
        if (cp <= 0xFFFF) {
            out.push_back(static_cast<char16_t>(cp));
        } else {
            cp -= 0x10000;
            out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
        }
    }
    return out;
}

std::string decode(const std::u16string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        uint32_t cp = s[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < s.size() && s[i + 1] >= 0xDC00 &&
            s[i + 1] <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (s[i + 1] - 0xDC00);
            ++i;
        }
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

}  // namespace utf16

namespace {

std::string u16_to_utf8(const std::u16string& s, size_t begin, size_t end) {
    if (end > s.size()) end = s.size();
    if (begin > end) begin = end;
    return utf16::decode(s.substr(begin, end - begin));
}

// ---------------- open tag bookkeeping ----------------

struct OpenEntity {
    std::string ctor;
    int64_t offset{0};
    int64_t length{0};
    std::map<std::string, Val> extra;
};

struct TagInfo {
    std::string ctor;
    std::map<std::string, Val> extra;
};

TagInfo classify_tag(const std::string& tag, const std::map<std::string, std::string>& attrs) {
    TagInfo t;
    const auto a = [&](const char* k) -> std::string {
        const auto it = attrs.find(k);
        return it == attrs.end() ? std::string() : it->second;
    };
    const auto int_or = [](const std::string& v, int64_t dflt) -> int64_t {
        try {
            return static_cast<int64_t>(std::stoll(v));
        } catch (const std::exception&) {
            return dflt;
        }
    };

    if (tag == "b" || tag == "strong") {
        t.ctor = "messageEntityBold";
    } else if (tag == "i" || tag == "em") {
        t.ctor = "messageEntityItalic";
    } else if (tag == "u" || tag == "ins") {
        t.ctor = "messageEntityUnderline";
    } else if (tag == "s" || tag == "del" || tag == "strike") {
        t.ctor = "messageEntityStrike";
    } else if (tag == "blockquote") {
        t.ctor = "messageEntityBlockquote";
        t.extra["collapsed"] = Val::i64(attrs.count("expandable") > 0 ? 1 : 0);
    } else if (tag == "code") {
        t.ctor = "messageEntityCode";
    } else if (tag == "pre") {
        t.ctor = "messageEntityPre";
        t.extra["language"] = Val::str(a("language"));
    } else if (tag == "spoiler" || tag == "tg-spoiler") {
        t.ctor = "messageEntitySpoiler";
    } else if (tag == "a") {
        const std::string url = a("href");
        static const char kMention[] = "tg://user?id=";
        if (url.rfind(kMention, 0) == 0 && url.size() > std::strlen(kMention)) {
            t.ctor = "inputMessageEntityMentionName";
            t.extra["user_id"] = Val::str(url.substr(std::strlen(kMention)));
        } else if (url.rfind("mailto:", 0) == 0) {
            t.ctor = "messageEntityEmail";
        } else {
            t.ctor = "messageEntityTextUrl";
            t.extra["url"] = Val::str(url);
        }
    } else if (tag == "emoji" || tag == "tg-emoji") {
        t.ctor = "messageEntityCustomEmoji";
        t.extra["document_id"] =
            Val::i64(int_or(tag == "tg-emoji" ? a("emoji-id") : a("id"), 0));
    } else if (tag == "tg-time") {
        t.ctor = "messageEntityFormattedDate";
        t.extra["date"] = Val::i64(int_or(a("unix"), 0));
        std::string fmt = a("format");
        bool relative = false, day_of_week = false, short_date = false, long_date = false;
        bool short_time = false, long_time = false;
        if (!fmt.empty()) {
            bool ok = true;
            for (char c : fmt)
                if (std::strchr("rwdtDT", c) == nullptr) ok = false;
            if (!ok && fmt != "r") fmt.clear();  // invalid format: treated as plain
            if (fmt == "r") {
                relative = true;
            } else if (!fmt.empty()) {
                if (fmt.find('w') != std::string::npos) day_of_week = true;
                if (fmt.find('d') != std::string::npos) short_date = true;
                if (fmt.find('D') != std::string::npos) long_date = true;
                if (fmt.find('t') != std::string::npos) short_time = true;
                if (fmt.find('T') != std::string::npos) long_time = true;
            }
        }
        t.extra["relative"] = Val::i64(relative);
        t.extra["short_time"] = Val::i64(short_time);
        t.extra["long_time"] = Val::i64(long_time);
        t.extra["short_date"] = Val::i64(short_date);
        t.extra["long_date"] = Val::i64(long_date);
        t.extra["day_of_week"] = Val::i64(day_of_week);
    }
    return t;
}

// Split "name=value name2='v v'" pairs (html attribute list, simplified).
void split_attrs(const std::string& in, std::map<std::string, std::string>& out) {
    size_t i = 0;
    while (i < in.size()) {
        while (i < in.size() && std::isspace(static_cast<unsigned char>(in[i]))) ++i;
        const size_t ks = i;
        while (i < in.size() && in[i] != '=' && !std::isspace(static_cast<unsigned char>(in[i])))
            ++i;
        if (i == ks) break;
        std::string key = in.substr(ks, i - ks);
        std::transform(key.begin(), key.end(), key.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        while (i < in.size() && std::isspace(static_cast<unsigned char>(in[i]))) ++i;
        std::string val;
        if (i < in.size() && in[i] == '=') {
            ++i;
            while (i < in.size() && std::isspace(static_cast<unsigned char>(in[i]))) ++i;
            if (i < in.size() && (in[i] == '"' || in[i] == '\'')) {
                const char q = in[i++];
                const size_t vs = i;
                while (i < in.size() && in[i] != q) ++i;
                val = in.substr(vs, i - vs);
                if (i < in.size()) ++i;
            } else {
                const size_t vs = i;
                while (i < in.size() && !std::isspace(static_cast<unsigned char>(in[i]))) ++i;
                val = in.substr(vs, i - vs);
            }
        }
        if (!key.empty()) out[key] = html_unescape(val);
    }
}

// The two whitespace subs kurigram's HTML.parse applies before parsing:
//   re.sub(r"^\s*(<[\w<>=\s\"]*>)\s*", r"\1", text)
//   re.sub(r"\s*(</[\w</>]*>)\s*$", r"\1", text)
// A wrapping tag keeps the whitespace that hugs it; text without such a tag is
// left untouched.
namespace {

bool open_tag_char(char c) {
    const unsigned char u = static_cast<unsigned char>(c);
    return std::isalnum(u) || c == '<' || c == '>' || c == '=' || c == '"' || c == '_' ||
           std::isspace(u);
}

bool close_tag_char(char c) {
    const unsigned char u = static_cast<unsigned char>(c);
    return std::isalnum(u) || c == '<' || c == '>' || c == '/' || c == '_' || std::isspace(u);
}

bool is_space(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

}  // namespace

std::string strip_edges(const std::string& text) {
    std::string result = text;

    size_t b = 0;
    while (b < result.size() && is_space(result[b])) ++b;
    if (b < result.size() && result[b] == '<') {
        size_t j = b + 1;
        while (j < result.size() && open_tag_char(result[j])) ++j;
        // The class run cannot contain "/", so a "</tag>" tail ends it and the
        // regex backtracks to the last ">" inside the run.
        if (j < result.size() && result[j] == '>') ++j;
        size_t t = std::string::npos;
        for (size_t i = b + 1; i < j && i < result.size(); ++i)
            if (result[i] == '>') t = i;
        if (t != std::string::npos) {
            size_t k = t + 1;
            while (k < result.size() && is_space(result[k])) ++k;
            result = result.substr(b, t - b + 1) + result.substr(k);
        }
    }

    size_t p = result.size();
    while (p > 0 && is_space(result[p - 1])) --p;
    if (p > 0 && result[p - 1] == '>') {
        for (size_t s = 0; s + 1 < p; ++s) {
            if (result[s] != '<' || result[s + 1] != '/') continue;
            bool ok = true;
            for (size_t i = s + 2; ok && i + 1 < p; ++i)
                if (!close_tag_char(result[i])) ok = false;
            if (!ok) continue;
            size_t q = s;
            while (q > 0 && is_space(result[q - 1])) --q;
            result = result.substr(0, q) + result.substr(s, p - s);
            break;
        }
    }
    return result;
}

// Extra field readers: Val holds either a string or a number, so read by kind.
const std::string& extra_str(const Entity& e, const char* k, const std::string& empty) {
    const auto it = e.extra.find(k);
    if (it == e.extra.end() || it->second.k != Val::STR) return empty;
    return it->second.s;
}
int64_t extra_i64(const Entity& e, const char* k) {
    const auto it = e.extra.find(k);
    if (it == e.extra.end() || !it->second.is_int()) return 0;
    return it->second.as_int();
}
bool extra_flag(const Entity& e, const char* k) { return extra_i64(e, k) != 0; }

// Open/close tags per raw entity ctor, for unparse().
bool entity_tags(const Entity& e, std::string& open, std::string& close) {
    if (e.ctor == "messageEntityBold" || e.ctor == "messageEntityItalic" ||
        e.ctor == "messageEntityUnderline" || e.ctor == "messageEntityStrike") {
        const char letter = e.ctor == "messageEntityBold"
                                ? 'b'
                                : e.ctor == "messageEntityItalic"
                                      ? 'i'
                                      : e.ctor == "messageEntityUnderline" ? 'u' : 's';
        open = std::string("<") + letter + ">";
        close = std::string("</") + letter + ">";
        return true;
    }
    if (e.ctor == "messageEntityPre") {
        const std::string lang = extra_str(e, "language", "");
        open = lang.empty() ? "<pre>" : "<pre language=\"" + lang + "\">";
        close = "</pre>";
        return true;
    }
    if (e.ctor == "messageEntityBlockquote") {
        open = extra_flag(e, "collapsed") ? "<blockquote expandable>" : "<blockquote>";
        close = "</blockquote>";
        return true;
    }
    if (e.ctor == "messageEntityCode") {
        open = "<code>";
        close = "</code>";
        return true;
    }
    if (e.ctor == "messageEntitySpoiler") {
        open = "<spoiler>";
        close = "</spoiler>";
        return true;
    }
    if (e.ctor == "messageEntityTextUrl") {
        open = "<a href=\"" + extra_str(e, "url", "") + "\">";
        close = "</a>";
        return true;
    }
    if (e.ctor == "inputMessageEntityMentionName") {
        open = "<a href=\"tg://user?id=" + std::to_string(extra_i64(e, "user_id")) + "\">";
        close = "</a>";
        return true;
    }
    if (e.ctor == "messageEntityCustomEmoji") {
        open = "<tg-emoji emoji-id=\"" + std::to_string(extra_i64(e, "document_id")) + "\">";
        close = "</tg-emoji>";
        return true;
    }
    if (e.ctor == "messageEntityFormattedDate") {
        // parse() only keeps the six flags, so rebuild the format string from
        // them to keep a parse -> unparse round-trip faithful.
        std::string fmt = extra_flag(e, "relative") ? "r" : "";
        if (fmt.empty()) {
            if (extra_flag(e, "day_of_week")) fmt += "w";
            if (extra_flag(e, "short_date")) fmt += "d";
            if (extra_flag(e, "long_date")) fmt += "D";
            if (extra_flag(e, "short_time")) fmt += "t";
            if (extra_flag(e, "long_time")) fmt += "T";
        }
        const std::string stamp = std::to_string(extra_i64(e, "date"));
        open = fmt.empty() ? "<tg-time unix=\"" + stamp + "\">"
                           : "<tg-time unix=\"" + stamp + "\" format=\"" + fmt + "\">";
        close = "</tg-time>";
        return true;
    }
    return false;
}

}  // namespace

// ---------------- entity references ----------------

std::string html_unescape(const std::string& s) {
    static const std::map<std::string, std::string> kNamed = {
        {"amp", "&"}, {"lt", "<"}, {"gt", ">"}, {"quot", "\""}, {"apos", "'"},
        {"nbsp", "\xC2\xA0"},
    };
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        if (s[i] != '&') {
            out.push_back(s[i++]);
            continue;
        }
        const size_t semi = s.find(';', i + 1);
        if (semi == std::string::npos || semi - i > 32) {
            out.push_back(s[i++]);
            continue;
        }
        const std::string body = s.substr(i + 1, semi - i - 1);
        std::string repl;
        bool ok = false;
        if (body.rfind("#x", 0) == 0 || body.rfind("#X", 0) == 0) {
            try {
                std::u16string one;
                one.push_back(static_cast<char16_t>(std::stoul(body.substr(2), nullptr, 16)));
                repl = utf16::decode(one);
                ok = true;
            } catch (const std::exception&) {
            }
        } else if (!body.empty() && body[0] == '#') {
            try {
                std::u16string one;
                one.push_back(static_cast<char16_t>(std::stoul(body.substr(1), nullptr, 10)));
                repl = utf16::decode(one);
                ok = true;
            } catch (const std::exception&) {
            }
        } else {
            const auto it = kNamed.find(body);
            if (it != kNamed.end()) {
                repl = it->second;
                ok = true;
            }
        }
        if (ok) {
            out += repl;
            i = semi + 1;
        } else {
            out.push_back(s[i++]);
        }
    }
    return out;
}

std::string html_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

// ---------------- Entity <-> Val ----------------

Val Entity::to_val() const {
    std::map<std::string, Val> f = extra;
    f["offset"] = Val::i64(offset);
    f["length"] = Val::i64(length);
    return Val::obj(ctor, std::move(f));
}

std::optional<Entity> Entity::from_val(const Val& v) {
    if (v.k != Val::OBJ || v.name.rfind("messageEntity", 0) != 0) return std::nullopt;
    Entity e;
    e.ctor = v.name;
    e.offset = v.num("offset");
    e.length = v.num("length");
    for (const auto& [k, kv] : v.o)
        if (k != "offset" && k != "length") e.extra[k] = kv;
    return e;
}

// CPython's HTMLParser runs with convert_charrefs=True, so text data reaches
// handle_data() already unescaped once, and kurigram's handle_data() unescapes
// it a second time. Attribute values only get the single tokenizer pass.
std::string html_unescape_data(const std::string& s) { return html_unescape(html_unescape(s)); }

// ---------------- parse ----------------

TextEntities Html::parse(const std::string& text_) const {
    const std::u16string src = utf16::encode(strip_edges(text_));
    std::u16string out;
    std::vector<Entity> entities;
    std::map<std::string, std::vector<OpenEntity>> tag_entities;

    auto append_text = [&](const std::string& plain) {
        const std::u16string chunk = utf16::encode(plain);
        for (auto& [tag, stack] : tag_entities)
            for (auto& e : stack) e.length += static_cast<int64_t>(chunk.size());
        out.append(chunk);
    };

    for (size_t i = 0; i < src.size();) {
        if (src[i] != u'<') {
            size_t j = i;
            while (j < src.size() && src[j] != u'<') ++j;
            append_text(html_unescape_data(u16_to_utf8(src, i, j)));
            i = j;
            continue;
        }
        if (src.compare(i, 4, u"<!--") == 0) {  // comment
            const size_t end = src.find(u"-->", i + 4);
            i = end == std::u16string::npos ? src.size() : end + 3;
            continue;
        }
        // A quoted attribute value may contain ">", like html.parser allows.
        size_t j = std::u16string::npos;
        char quote = 0;
        for (size_t k = i + 1; k < src.size(); ++k) {
            if (quote) {
                if (src[k] == quote) quote = 0;
            } else if (src[k] == u'"' || src[k] == u'\'') {
                quote = static_cast<char>(src[k]);
            } else if (src[k] == u'>') {
                j = k;
                break;
            }
        }
        if (j == std::u16string::npos) {  // unterminated: the remainder is text
            append_text(html_unescape_data(u16_to_utf8(src, i, src.size())));
            break;
        }
        // Only a name or "/name" can open/close a tag; a '<' followed by anything
        // else (e.g. "< y") is literal text, like the html.parser reference does.
        if (j == i + 1 || !(std::isalpha(static_cast<unsigned char>(src[i + 1])) ||
                            src[i + 1] == u'/')) {
            append_text("<");
            ++i;
            continue;
        }
        const std::string inner = u16_to_utf8(src, i + 1, j);
        i = j + 1;
        // A closing tag needs "/" plus at least one more name character.
        if (inner[0] == '/' && (inner.size() < 2 ||
                                !std::isalpha(static_cast<unsigned char>(inner[1]))))
            continue;

        std::string tag = inner, attrs_str;
        if (const size_t sp = inner.find_first_of(" \t\r\n"); sp != std::string::npos) {
            tag = inner.substr(0, sp);
            attrs_str = inner.substr(sp + 1);
        }
        std::transform(tag.begin(), tag.end(), tag.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (!tag.empty() && tag.front() == '/') {
            const std::string name = tag.substr(1);
            if (auto it = tag_entities.find(name);
                it != tag_entities.end() && !it->second.empty()) {
                const OpenEntity& oe = it->second.back();
                entities.push_back(Entity{oe.ctor, oe.offset, oe.length, oe.extra});
                it->second.pop_back();
                if (it->second.empty()) tag_entities.erase(it);
            }
            continue;  // unmatched closing tag: ignored, like the reference impl
        }

        std::map<std::string, std::string> attrs;
        split_attrs(attrs_str, attrs);
        const TagInfo info = classify_tag(tag, attrs);
        if (info.ctor.empty()) continue;  // unknown tag: ignored
        OpenEntity oe;
        oe.ctor = info.ctor;
        oe.offset = static_cast<int64_t>(out.size());
        oe.extra = info.extra;
        tag_entities[tag].push_back(std::move(oe));
    }

    // Zero-length entities are dropped, the rest sorted by offset (kurigram's
    // `sorted(entities, key=lambda e: e.offset)`), stable on ties.
    std::vector<Entity> kept;
    for (auto& e : entities)
        if (e.length > 0) kept.push_back(std::move(e));
    std::stable_sort(kept.begin(), kept.end(),
                     [](const Entity& a, const Entity& b) { return a.offset < b.offset; });
    return TextEntities{utf16::decode(out), std::move(kept)};
}

// ---------------- unparse ----------------

std::string Html::unparse(const std::string& text, std::vector<Entity> entities) {
    const std::u16string src = utf16::encode(text);

    // Mirrors kurigram's recursive(): each call returns how many entities it
    // consumed, so nested entities are visited exactly once.
    std::vector<std::pair<std::string, int64_t>> offsets;
    std::function<size_t(size_t)> recursive = [&](size_t entity_i) -> size_t {
        const Entity& e = entities[entity_i];
        const int64_t start = e.offset;
        const int64_t end = start + e.length;
        std::string open, close;
        if (!entity_tags(e, open, close)) return 1;  // unsupported: consume, emit nothing
        offsets.emplace_back(open, start);
        size_t internal = entity_i + 1;
        while (internal < entities.size() && entities[internal].offset < end)
            internal += recursive(internal);
        offsets.emplace_back(close, end);
        return internal - entity_i;
    };

    std::stable_sort(entities.begin(), entities.end(), [](const Entity& a, const Entity& b) {
        if (a.offset != b.offset) return a.offset < b.offset;
        return a.length > b.length;
    });

    for (size_t i = 0; i < entities.size();) i += recursive(i);
    if (offsets.empty()) return text;

    // Mirrors kurigram's tail: last_offset starts at the last collected offset
    // and every slice is clamped like python's, so offsets past the text end
    // (or an offset beyond last_offset) cannot run off the buffer.
    std::u16string res = src;
    int64_t last = offsets.empty() ? 0 : offsets.back().second;
    for (auto it = offsets.rbegin(); it != offsets.rend(); ++it) {
        const int64_t size = static_cast<int64_t>(res.size());
        const int64_t pos = std::min<int64_t>(it->second, size);
        const int64_t seg_end = std::max<int64_t>(pos, std::min<int64_t>(last, size));
        const std::u16string seg =
            utf16::encode(html_escape(u16_to_utf8(res, static_cast<size_t>(pos),
                                                  static_cast<size_t>(seg_end))));
        const std::u16string tail =
            res.substr(static_cast<size_t>(std::min<int64_t>(seg_end, size)));
        res = res.substr(0, static_cast<size_t>(pos)) + utf16::encode(it->first) + seg + tail;
        last = it->second;
    }
    return utf16::decode(res);
}

}  // namespace cppgram
