// Implementation of the kurigram markdown parser/unparser (parser/markdown.py).
#include "cppgram/markdown.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <optional>
#include <set>
#include <string>

namespace cppgram {

namespace {

// pyrogram delimiter table (parser/markdown.py).
constexpr const char* BOLD_DELIM = "**";
constexpr const char* ITALIC_DELIM = "__";
constexpr const char* UNDERLINE_DELIM = "--";
constexpr const char* STRIKE_DELIM = "~~";
constexpr const char* SPOILER_DELIM = "||";
constexpr const char* CODE_DELIM = "`";
constexpr const char* PRE_DELIM = "```";
constexpr const char* BLOCKQUOTE_DELIM = ">";
constexpr const char* BLOCKQUOTE_EXPANDABLE_DELIM = "**>";
constexpr const char* BLOCKQUOTE_EXPANDABLE_END_DELIM = "||";

// Delimiter priority of the reference regex alternation.
constexpr const char* kDelims[] = {PRE_DELIM,   CODE_DELIM, STRIKE_DELIM, UNDERLINE_DELIM,
                                   ITALIC_DELIM, BOLD_DELIM, SPOILER_DELIM};

bool is_fixed_width_delim(const std::string& d) {
    return d == CODE_DELIM || d == PRE_DELIM;
}

bool starts_with(const std::string& s, const char* p) {
    const size_t n = std::strlen(p);
    return s.size() >= n && s.compare(0, n, p) == 0;
}

bool ends_with(const std::string& s, const char* p) {
    const size_t n = std::strlen(p);
    return s.size() >= n && s.compare(s.size() - n, n, p) == 0;
}

// kurigram's utils.replace_once(): replace the first occurrence of `old` at or
// after `start`, which is what keeps the reported match position meaningful even
// after earlier replacements shifted the text.
std::string replace_once(const std::string& source, const std::string& old,
                         const std::string& replacement, size_t start) {
    const size_t at = source.find(old, start);
    if (at == std::string::npos) return source;
    return source.substr(0, at) + replacement + source.substr(at + old.size());
}

// python html.escape(s) with the default quote=True.
std::string md_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#x27;"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

std::vector<std::optional<std::string>> split_lines(const std::string& text) {
    std::vector<std::optional<std::string>> lines;
    size_t i = 0;
    while (true) {
        const size_t nl = text.find('\n', i);
        std::string line = text.substr(i, nl == std::string::npos ? std::string::npos : nl - i);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(std::move(line));
        if (nl == std::string::npos) break;
        i = nl + 1;
    }
    return lines;
}

// ---------------- stage 1: quotes -> html ----------------

std::string escape_and_create_quotes(const std::string& text, bool strict) {
    std::vector<std::optional<std::string>> text_lines = split_lines(text);
    std::vector<std::pair<size_t, std::string>> to_quote;
    std::set<size_t> html_escaped;
    auto covered = [&](size_t idx) { return html_escaped.count(idx) > 0; };
    auto strip_delim = [](const std::string& line, const char* delim) {
        size_t n = std::strlen(delim);
        if (line.compare(0, n, delim) == 0 && line.size() > n && line[n] == ' ') ++n;
        return line.substr(std::min(n, line.size()));
    };
    auto create_blockquote = [&](bool expandable) {
        if (to_quote.empty()) return;
        std::string joined;
        for (size_t i = 0; i < to_quote.size(); ++i) {
            if (i) joined += "\n";
            joined += to_quote[i].second;
        }
        const size_t first = to_quote.front().first;
        text_lines[first] = std::string("<blockquote") + (expandable ? " expandable" : "") + ">" +
                            joined + "</blockquote>";
        for (size_t i = 1; i < to_quote.size(); ++i) text_lines[to_quote[i].first] = std::nullopt;
        to_quote.clear();
    };

    bool inside_blockquote = false;
    for (size_t index = 0; index < text_lines.size(); ++index) {
        if (!text_lines[index].has_value()) continue;
        const std::string line = *text_lines[index];
        if (starts_with(line, BLOCKQUOTE_EXPANDABLE_DELIM) && !inside_blockquote) {
            std::string stripped = strip_delim(line, BLOCKQUOTE_EXPANDABLE_DELIM);
            to_quote.emplace_back(index, strict ? md_escape(stripped) : stripped);
            html_escaped.insert(index);
            inside_blockquote = true;
            continue;
        }
        if (ends_with(line, BLOCKQUOTE_EXPANDABLE_END_DELIM) && inside_blockquote) {
            std::string unwrapped = line;
            if (starts_with(unwrapped, BLOCKQUOTE_DELIM))
                unwrapped = strip_delim(unwrapped, BLOCKQUOTE_DELIM);
            std::string stripped =
                unwrapped.substr(0, unwrapped.size() - std::strlen(BLOCKQUOTE_EXPANDABLE_END_DELIM));
            to_quote.emplace_back(index, strict ? md_escape(stripped) : stripped);
            html_escaped.insert(index);
            inside_blockquote = false;
            create_blockquote(true);
            continue;
        }
        if (inside_blockquote) {
            std::string parsed = strip_delim(line, BLOCKQUOTE_DELIM);
            to_quote.emplace_back(index, strict ? md_escape(parsed) : parsed);
            html_escaped.insert(index);
        }
    }

    for (size_t index = 0; index < text_lines.size(); ++index) {
        if (!text_lines[index].has_value()) continue;
        const std::string line = *text_lines[index];
        if (starts_with(line, BLOCKQUOTE_DELIM)) {
            std::string parsed = strip_delim(line, BLOCKQUOTE_DELIM);
            to_quote.emplace_back(index, strict ? md_escape(parsed) : parsed);
            html_escaped.insert(index);
        } else if (!to_quote.empty()) {
            create_blockquote(false);
        }
    }
    create_blockquote(false);

    if (strict)
        for (size_t idx = 0; idx < text_lines.size(); ++idx)
            if (!covered(idx) && text_lines[idx].has_value())
                text_lines[idx] = md_escape(*text_lines[idx]);

    std::string out;
    bool first = true;
    for (const auto& l : text_lines) {
        if (!l.has_value()) continue;
        if (!first) out += "\n";
        out += *l;
        first = false;
    }
    return out;
}

// ---------------- link helpers ----------------

std::map<std::string, std::string> parse_qs(const std::string& q) {
    std::map<std::string, std::string> out;
    size_t i = 0;
    while (i <= q.size()) {
        const size_t amp = q.find('&', i);
        const std::string kv = q.substr(i, amp == std::string::npos ? std::string::npos : amp - i);
        const size_t eq = kv.find('=');
        if (eq == std::string::npos) out[kv] = "";
        else out[kv.substr(0, eq)] = kv.substr(eq + 1);
        if (amp == std::string::npos) break;
        i = amp + 1;
    }
    return out;
}

// scheme://netloc[/path][?query], enough for tg://emoji?id= and tg://time?unix=.
struct Url {
    std::string netloc, query;
};
Url urlparse(const std::string& url) {
    Url u;
    const size_t sep = url.find("://");
    if (sep == std::string::npos) return u;
    const std::string rest = url.substr(sep + 3);
    const size_t slash = rest.find('/');
    const size_t qm = rest.find('?');
    const size_t host_end = (slash == std::string::npos) ? rest.size() : slash;
    if (qm != std::string::npos && qm < host_end) u.netloc = rest.substr(0, qm);
    else u.netloc = rest.substr(0, host_end);
    if (qm != std::string::npos) u.query = rest.substr(qm + 1);
    return u;
}

}  // namespace

TextEntities Markdown::parse(const std::string& text_, bool strict) const {
    std::string text = escape_and_create_quotes(text_, strict);

    std::set<std::string> delims;
    bool is_fixed_width = false;

    // Mirrors `for match in re.finditer(MARKDOWN_RE, text)`: the matches are
    // located once over the text as it is at the start of the loop, then applied
    // in order against the string the loop keeps rewriting.
    struct Match {
        size_t start;
        size_t len;
        std::string delim;   // empty for a [text](url) link
        bool emoji_like;
        std::string text_url;
        std::string url;
        std::string full;  // match.group(0), captured before the text is rewritten
    };
    std::vector<Match> matches;
    for (size_t pos = 0; pos < text.size();) {
        Match m{};
        m.start = pos;
        for (const char* d : kDelims) {
            const size_t n = std::strlen(d);
            if (text.compare(pos, n, d) == 0) {
                m.delim = d;
                m.len = n;
                m.full = d;
                break;
            }
        }
        if (m.len == 0 && (text[pos] == '[' || text[pos] == '!')) {
            const size_t bracket = text.find('[', text[pos] == '!' ? pos + 1 : pos);
            if (bracket != std::string::npos) {
                const size_t rb = text.find(']', bracket + 1);
                if (rb != std::string::npos && rb + 1 < text.size() && text[rb + 1] == '(') {
                    const size_t rp = text.find(')', rb + 2);
                    if (rp != std::string::npos && rp > rb + 2) {
                        m.len = rp + 1 - pos;
                        m.emoji_like = text[pos] == '!';
                        m.text_url = text.substr(bracket + 1, rb - bracket - 1);
                        // "(" sits at rb+1, so the url starts at rb+2.
                        m.url = text.substr(rb + 2, rp - rb - 2);
                        m.full = text.substr(pos, m.len);
                    }
                }
            }
        }
        if (m.len == 0) {
            ++pos;
            continue;
        }
        matches.push_back(m);
        pos += m.len;
    }

    for (const Match& m : matches) {
        const std::string& full = m.full;
        if (!m.delim.empty() && is_fixed_width_delim(m.delim)) is_fixed_width = !is_fixed_width;
        if (is_fixed_width && !is_fixed_width_delim(m.delim)) continue;

        if (!m.emoji_like && !m.text_url.empty()) {
            text = replace_once(text, full,
                                std::string("<a href=\"") + m.url + "\">" + m.text_url + "</a>",
                                m.start);
            continue;
        }
        if (m.emoji_like) {
            const Url parsed = urlparse(m.url);
            if (parsed.netloc == "time") {
                const auto params = parse_qs(parsed.query);
                const std::string& unix_time = params.count("unix") ? params.at("unix") : "";
                const std::string& fmt = params.count("format") ? params.at("format") : "";
                text = replace_once(text, full,
                                    fmt.empty()
                                        ? "<tg-time unix=\"" + unix_time + "\">" + m.text_url +
                                              "</tg-time>"
                                        : "<tg-time unix=\"" + unix_time + "\" format=\"" + fmt +
                                              "\">" + m.text_url + "</tg-time>",
                                    m.start);
            } else if (parsed.netloc == "emoji") {
                const auto params = parse_qs(parsed.query);
                const std::string& emoji_id = params.count("id") ? params.at("id") : "";
                text = replace_once(text, full,
                                    "<tg-emoji emoji-id=\"" + emoji_id + "\">" + m.text_url +
                                        "</tg-emoji>",
                                    m.start);
            }
            continue;
        }

        const std::string& delim = m.delim;
        std::string tag;
        if (delim == BOLD_DELIM) tag = "b";
        else if (delim == ITALIC_DELIM) tag = "i";
        else if (delim == UNDERLINE_DELIM) tag = "u";
        else if (delim == STRIKE_DELIM) tag = "s";
        else if (delim == CODE_DELIM) tag = "code";
        else if (delim == PRE_DELIM) tag = "pre";
        else if (delim == SPOILER_DELIM) tag = "spoiler";
        else continue;

        if (!delims.count(delim)) {
            delims.insert(delim);
            tag = std::string("<") + tag + ">";
        } else {
            delims.erase(delim);
            tag = std::string("</") + tag + ">";
        }

        if (delim == PRE_DELIM && delims.count(delim)) {
            // The reference slices text[text.find("```"):] and takes its first
            // line; a missing delimiter yields python's -1 slice, i.e. the last
            // character, which is reproduced here.
            const size_t at = text.find(PRE_DELIM);
            std::string delim_and_language;
            if (at == std::string::npos) {
                delim_and_language = text.empty() ? std::string() : text.substr(text.size() - 1);
            } else {
                const size_t stop = text.find('\n', at);
                delim_and_language =
                    text.substr(at, stop == std::string::npos ? std::string::npos : stop - at);
            }
            const std::string language =
                delim_and_language.size() > std::strlen(PRE_DELIM)
                    ? delim_and_language.substr(std::strlen(PRE_DELIM))
                    : std::string();
            text = replace_once(text, delim_and_language, "<pre language=\"" + language + "\">",
                                m.start);
            continue;
        }
        text = replace_once(text, delim, tag, m.start);
    }

    Html html;
    return html.parse(text);
}

// ---------------- unparse ----------------

std::string Markdown::unparse(const std::string& text, std::vector<Entity> entities) {
    std::vector<std::pair<std::string, int64_t>> offsets;
    for (const auto& e : entities) {
        const int64_t start = e.offset;
        const int64_t end = start + e.length;
        const auto extra_str = [&](const char* k) -> std::string {
            const auto it = e.extra.find(k);
            return (it != e.extra.end() && it->second.k == Val::STR) ? it->second.s : std::string();
        };
        const auto extra_num = [&](const char* k) -> int64_t {
            const auto it = e.extra.find(k);
            return (it != e.extra.end() && it->second.is_int()) ? it->second.as_int() : 0;
        };
        if (e.ctor == "messageEntityBold" || e.ctor == "messageEntityItalic" ||
            e.ctor == "messageEntityUnderline" || e.ctor == "messageEntityStrike" ||
            e.ctor == "messageEntitySpoiler") {
            const char* d = e.ctor == "messageEntityBold"      ? BOLD_DELIM
                            : e.ctor == "messageEntityItalic"  ? ITALIC_DELIM
                            : e.ctor == "messageEntityUnderline" ? UNDERLINE_DELIM
                            : e.ctor == "messageEntityStrike"   ? STRIKE_DELIM
                                                                 : SPOILER_DELIM;
            offsets.emplace_back(d, start);
            offsets.emplace_back(d, end);
        } else if (e.ctor == "messageEntityCode") {
            offsets.emplace_back(CODE_DELIM, start);
            offsets.emplace_back(CODE_DELIM, end);
        } else if (e.ctor == "messageEntityPre") {
            offsets.emplace_back(std::string(PRE_DELIM) + extra_str("language") + "\n", start);
            offsets.emplace_back(std::string("\n") + PRE_DELIM, end);
        } else if (e.ctor == "messageEntityBlockquote") {
            // Direct port: split the quoted span into lines and prefix each one,
            // including the reference's quirky end_offset arithmetic.
            const std::u16string units = utf16::encode(text);
            const int64_t begin = std::max<int64_t>(0, start);
            const int64_t stop = std::min<int64_t>(static_cast<int64_t>(units.size()), end);
            std::vector<std::u16string> lines;
            int64_t cursor = begin;
            while (cursor < stop) {
                const int64_t nl = static_cast<int64_t>(
                    std::find(units.begin() + cursor, units.begin() + stop, u'\n') -
                    units.begin());
                if (nl >= stop) break;  // no more newlines
                lines.push_back(units.substr(cursor, nl - cursor));
                cursor = nl + 1;
            }
            lines.push_back(units.substr(cursor, stop - cursor));
            int64_t last_length = 0;
            for (const auto& line : lines) {
                if (line.empty() && last_length == end) continue;
                const int64_t start_offset = start + last_length;
                last_length += static_cast<int64_t>(line.size());
                const int64_t end_offset = start_offset + last_length;
                offsets.emplace_back(std::string(BLOCKQUOTE_DELIM) + " ", start_offset);
                offsets.emplace_back(std::string(), end_offset);
                last_length += 1;
            }
        } else if (e.ctor == "messageEntityTextUrl") {
            offsets.emplace_back("[", start);
            offsets.emplace_back("](" + extra_str("url") + ")", end);
        } else if (e.ctor == "inputMessageEntityMentionName") {
            offsets.emplace_back("[", start);
            offsets.emplace_back("](tg://user?id=" + std::to_string(extra_num("user_id")) + ")",
                                 end);
        } else if (e.ctor == "messageEntityCustomEmoji") {
            offsets.emplace_back("![", start);
            offsets.emplace_back("](tg://emoji?id=" + std::to_string(extra_num("document_id")) + ")",
                                 end);
        } else if (e.ctor == "messageEntityFormattedDate") {
            offsets.emplace_back("![", start);
            offsets.emplace_back("](tg://time?unix=" + std::to_string(extra_num("date")) + ")", end);
        }
    }

    // The reference sorts by (offset, original index) reversed, then inserts.
    std::vector<size_t> order(offsets.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        if (offsets[a].second != offsets[b].second) return offsets[a].second > offsets[b].second;
        return a > b;
    });

    std::string res = text;
    for (size_t idx : order) {
        const int64_t at = offsets[idx].second;
        if (at < 0 || static_cast<size_t>(at) > res.size()) continue;
        res.insert(static_cast<size_t>(at), offsets[idx].first);
    }
    return res;
}

}  // namespace cppgram
