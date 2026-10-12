// Implementation of the kurigram `pyrogram/utils` parity header.
#include "cppgram/utils.hpp"

#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <cstring>

namespace cppgram::utils {

namespace peers {

std::optional<int64_t> get_raw_peer_id(int64_t peer) {
    if (peer < 0) {
        if (-MAX_CHAT_ID <= peer) return -peer;
        if (ZERO_CHANNEL_ID - MAX_CHANNEL_ID <= peer && peer != ZERO_CHANNEL_ID)
            return ZERO_CHANNEL_ID - peer;
        if (ZERO_CHANNEL_ID - MAX_MONOFORUM_CHANNEL_ID <= peer) return ZERO_CHANNEL_ID - peer;
    } else if (0 < peer && peer <= MAX_USER_ID) {
        return peer;
    }
    return std::nullopt;
}

namespace {

// kurigram reads the id attribute off peers whose constructor carries one of
// `user_id`, `chat_id`, `channel_id`. Only the first matching attribute wins.
bool peer_has_field(const Val& p, const char* f) { return p.has(f) && p.at(f).k != Val::NIL; }

}  // namespace

std::optional<int64_t> get_peer_id(const Val& peer) {
    if (peer.k != Val::OBJ) return std::nullopt;
    if (peer_has_field(peer, "user_id")) return peer.num("user_id");
    if (peer_has_field(peer, "chat_id")) return -peer.num("chat_id");
    if (peer_has_field(peer, "channel_id")) return ZERO_CHANNEL_ID - peer.num("channel_id");
    return std::nullopt;
}

std::optional<int64_t> get_raw_peer_id(const Val& peer) {
    if (peer.k != Val::OBJ) return std::nullopt;
    if (peer_has_field(peer, "user_id")) return peer.num("user_id");
    if (peer_has_field(peer, "chat_id")) return peer.num("chat_id");
    if (peer_has_field(peer, "channel_id")) return peer.num("channel_id");
    if (peer.k == Val::INT || peer.k == Val::NUM) return get_raw_peer_id(peer.as_int());
    return std::nullopt;
}

std::string get_peer_type(int64_t peer_id) {
    if (peer_id < 0) {
        if (-MAX_CHAT_ID <= peer_id) return "chat";
        if (ZERO_CHANNEL_ID - MAX_CHANNEL_ID <= peer_id && peer_id != ZERO_CHANNEL_ID)
            return "channel";
        if (ZERO_SECRET_CHAT_ID + (-1LL << 31) <= peer_id && peer_id != ZERO_SECRET_CHAT_ID)
            return "secret_chat";
        if (ZERO_CHANNEL_ID - MAX_MONOFORUM_CHANNEL_ID <= peer_id) return "monoforum";
    } else if (0 < peer_id && peer_id <= MAX_USER_ID) {
        return "user";
    }
    throw std::invalid_argument("Peer id invalid: " + std::to_string(peer_id));
}

}  // namespace peers

namespace amounts {

int get_premium_duration_day_count(int month_count) {
    if (month_count <= 0 || month_count > 10000000) return 7;
    return month_count * 30 + month_count / 3 + month_count / 12;
}

}  // namespace amounts

namespace datetimes {

std::optional<std::time_t> timestamp_to_datetime(int64_t ts) {
    if (ts == 0) return std::nullopt;
    return static_cast<std::time_t>(ts);
}

std::optional<int64_t> datetime_to_timestamp(std::optional<std::time_t> dt) {
    if (!dt) return std::nullopt;
    return static_cast<int64_t>(*dt);
}

std::optional<int64_t> datetime_to_timestamp(int64_t from_now_seconds) {
    return static_cast<int64_t>(::time(nullptr)) + from_now_seconds;
}

}  // namespace datetimes

// ---------------- text ----------------

std::vector<std::string> split_text(const std::string& text, size_t max_length) {
    // kurigram splits on code points (len on str); utf-8 sequences are 1..4 bytes,
    // so count leading bytes that are not continuation bytes (0b10xxxxxx).
    auto ulen = [](const std::string& s) {
        size_t n = 0;
        for (unsigned char c : s)
            if ((c & 0xC0) != 0x80) ++n;
        return n;
    };
    // Byte offset of the n-th code point.
    auto uoff = [&text](size_t n) {
        size_t i = 0, seen = 0;
        while (i < text.size() && seen < n) {
            ++seen;
            ++i;
            while (i < text.size() && (static_cast<unsigned char>(text[i]) & 0xC0) == 0x80) ++i;
        }
        return i;
    };

    if (ulen(text) <= max_length) return {text};

    std::vector<std::string> chunks;
    size_t pos = 0;
    while (pos < text.size()) {
        const std::string remaining = text.substr(pos);
        if (ulen(remaining) <= max_length) {
            chunks.push_back(remaining);
            break;
        }
        const size_t limit = uoff(max_length);
        const std::string cut = remaining.substr(0, limit);

        const size_t last_newline = cut.rfind('\n');
        size_t take = 0, skip = 0;
        if (last_newline != std::string::npos && last_newline > 0) {
            take = last_newline;
            skip = last_newline + 1;
        } else {
            const size_t last_space = cut.rfind(' ');
            if (last_space != std::string::npos && last_space > cut.size() / 2) {
                take = last_space;
                skip = last_space + 1;
            } else {
                take = cut.size();
                skip = limit;
            }
        }
        if (take > 0) chunks.push_back(cut.substr(0, take));
        pos += skip;
    }
    return chunks;
}

std::optional<std::string> get_first_url(const std::string& text) {
    // Mirrors kurigram's pyrogram/utils/text.py:get_first_url, which searches for
    //   (https?):\/\/([\w_-]+(?:(?:\.[\w_-]+)+))([\w.,@?^=%&:\/~+#-]*[\w@?^=%&\/~+#-])
    // Hand-rolled instead of using <regex>: the url is the scheme, a greedy domain
    // of [\w_-] labels joined by dots, and a path of at least one char that may not
    // end with "." or ",". Python's \w is unicode aware, so utf-8 bytes >= 0x80 are
    // treated as word characters here.
    auto is_word = [](char c) {
        const unsigned char u = static_cast<unsigned char>(c);
        return c != '\0' && (std::isalnum(u) || c == '_' || u >= 0x80);
    };
    auto is_label = [&is_word](char c) { return is_word(c) || c == '-'; };
    // [\w.,@?^=%&:/~+#-]
    auto is_url_char = [&is_word](char c) {
        return c != '\0' && (is_word(c) || std::strchr(".,@?^=%&:/~+#-", c) != nullptr);
    };
    // [\w@?^=%&/~+#-], i.e. the url charset minus "." and ","
    auto is_url_tail = [&is_word](char c) {
        return c != '\0' && (is_word(c) || std::strchr("@?^=%&/~+#-", c) != nullptr);
    };

    for (size_t i = 0; i + 7 <= text.size(); ++i) {
        if (text[i] != 'h') continue;
        const bool https = text.compare(i, 8, "https://") == 0;
        const bool http = !https && text.compare(i, 7, "http://") == 0;
        if (!https && !http) continue;
        const size_t start = i + (https ? 8 : 7);
        size_t j = start;
        while (j < text.size() && is_url_char(text[j])) ++j;
        // The path needs a final character outside the "."/"," set; drop the tail
        // that a url must not end with until one is found.
        while (j > start && !is_url_tail(text[j - 1])) --j;
        if (j == start) continue;
        // Greedy domain: [\w_-]+(\.[\w_-]+)*, never ending on a ".".
        size_t h = start;
        while (h < j && is_label(text[h])) ++h;
        if (h == start) continue;  // the domain must start with a label
        while (h + 1 < j && text[h] == '.' && is_label(text[h + 1])) {
            ++h;
            while (h < j && is_label(text[h])) ++h;
        }
        const std::string host = text.substr(start, h - start);
        if (host.find('.') == std::string::npos) {
            // Not a valid domain at this offset, keep scanning.
            continue;
        }
        if (h == j) {
            // No path at all: the regex gives the domain's last label to the path,
            // which only works when the domain keeps two labels of its own.
            size_t k = j;
            while (k > start && text[k - 1] != '.') --k;
            if (k == start) continue;
            if (text.substr(start, k - 1 - start).find('.') == std::string::npos) continue;
        }
        return text.substr(i, j - i);
    }
    return std::nullopt;
}

// ---------------- TL JSON values ----------------

Val obj_to_jsonvalue(const Val& obj) {
    switch (obj.k) {
        case Val::NIL: return Val::obj("jsonNull");
        case Val::INT:
        case Val::NUM: return Val::obj("jsonNumber", {{"value", obj}});
        default: break;
    }
    if (obj.k == Val::STR) return Val::obj("jsonString", {{"value", obj}});
    if (obj.k == Val::VEC) {
        std::vector<Val> out;
        out.reserve(obj.v.size());
        for (const auto& e : obj.v) out.push_back(obj_to_jsonvalue(e));
        return Val::obj("jsonArray", {{"value", Val::vec(std::move(out))}});
    }
    if (obj.k == Val::OBJ) {
        // jsonBool is dialect-specific: kurigram checks isinstance(obj, bool) before
        // int, and a parsed "true"/"false" arrives as a bool Val, which we carry as an
        // OBJ named jsonBool.
        if (obj.name == "jsonBool") return obj;
        std::vector<Val> out;
        for (const auto& [k, v] : obj.o)
            out.push_back(Val::obj("jsonObjectValue", {{"key", Val::str(k)},
                                                       {"value", obj_to_jsonvalue(v)}}));
        return Val::obj("jsonObject", {{"value", Val::vec(std::move(out))}});
    }
    throw std::invalid_argument("Unsupported type for obj_to_jsonvalue");
}

Val jsonvalue_to_obj(const Val& obj) {
    if (obj.k != Val::OBJ) throw std::invalid_argument("Unsupported type for jsonvalue_to_obj");
    if (obj.name == "jsonNull") return Val();
    if (obj.name == "jsonBool" || obj.name == "jsonNumber" || obj.name == "jsonString")
        return obj.at("value");
    if (obj.name == "jsonArray") {
        std::vector<Val> out;
        for (const auto& e : obj.list("value")) out.push_back(jsonvalue_to_obj(e));
        return Val::vec(std::move(out));
    }
    if (obj.name == "jsonObject") {
        std::map<std::string, Val> out;
        for (const auto& kv : obj.list("value"))
            out[kv.text("key")] = jsonvalue_to_obj(kv.at("value"));
        Val v;
        v.k = Val::OBJ;
        v.o = std::move(out);
        return v;
    }
    throw std::invalid_argument("Unsupported type for jsonvalue_to_obj: " + obj.name);
}

// ---------------- crypto ----------------

std::string sha256(const std::string& data) {
    std::string out(32, '\0');
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, data.data(), data.size());
    unsigned int len = 0;
    EVP_DigestFinal_ex(ctx, reinterpret_cast<unsigned char*>(out.data()), &len);
    EVP_MD_CTX_free(ctx);
    return out;
}

int64_t btoi(const std::string& b) {
    int64_t r = 0;
    for (unsigned char c : b) r = (r << 8) | c;
    return r;
}

std::string itob(int64_t i) {
    std::string out(256, '\0');
    for (int n = 255; n >= 0; --n) {
        out[n] = static_cast<char>(i & 0xFF);
        i >>= 8;
    }
    return out;
}

std::string bytes_xor(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) throw std::invalid_argument("xor length mismatch");
    std::string out(a.size(), '\0');
    for (size_t i = 0; i < a.size(); ++i)
        out[i] = static_cast<char>(static_cast<unsigned char>(a[i]) ^
                                   static_cast<unsigned char>(b[i]));
    return out;
}

std::string pbkdf2_sha512(const std::string& password, const std::string& salt, int iterations) {
    std::string out(64, '\0');
    if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()),
                          reinterpret_cast<const unsigned char*>(salt.data()),
                          static_cast<int>(salt.size()), iterations, EVP_sha512(), out.size(),
                          reinterpret_cast<unsigned char*>(out.data())) != 1)
        throw std::runtime_error("pbkdf2_hmac failed");
    return out;
}

std::string compute_password_hash(const Val& algo, const std::string& password) {
    if (algo.k != Val::OBJ || algo.name.find("passwordKdfAlgo") == std::string::npos)
        throw std::invalid_argument("compute_password_hash: not a PasswordKdfAlgo");
    const std::string& salt1 = algo.at("salt1").s;
    const std::string& salt2 = algo.at("salt2").s;
    const std::string hash1 = sha256(salt1 + password + salt1);
    const std::string hash2 = sha256(salt2 + hash1 + salt2);
    const std::string hash3 = pbkdf2_sha512(hash2, salt1, 100000);
    return sha256(salt2 + hash3 + salt2);
}

// ---------------- base64 / RLE ----------------

namespace {

constexpr char kAlpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

inline int rev(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '-') return 62;
    if (c == '_') return 63;
    if (c == '=') return -1;
    return -2;
}

}  // namespace

std::string b64_encode(const std::string& s) {
    std::string out;
    out.reserve((s.size() * 4 + 2) / 3);
    size_t i = 0;
    for (; i + 3 <= s.size(); i += 3) {
        const uint32_t n = (static_cast<unsigned char>(s[i]) << 16) |
                           (static_cast<unsigned char>(s[i + 1]) << 8) |
                           static_cast<unsigned char>(s[i + 2]);
        out.push_back(kAlpha[(n >> 18) & 63]);
        out.push_back(kAlpha[(n >> 12) & 63]);
        out.push_back(kAlpha[(n >> 6) & 63]);
        out.push_back(kAlpha[n & 63]);
    }
    if (size_t rem = s.size() - i) {
        uint32_t n = static_cast<unsigned char>(s[i]) << 16;
        if (rem == 2) n |= static_cast<unsigned char>(s[i + 1]) << 8;
        out.push_back(kAlpha[(n >> 18) & 63]);
        out.push_back(kAlpha[(n >> 12) & 63]);
        if (rem == 2) out.push_back(kAlpha[(n >> 6) & 63]);
    }
    return out;
}

std::string b64_decode(const std::string& s) {
    std::string out;
    out.reserve(s.size() * 3 / 4);
    uint32_t acc = 0;
    int bits = 0;
    for (char c : s) {
        const int v = rev(c);
        if (v == -2) throw std::invalid_argument("invalid base64 char");
        if (v < 0) break;
        acc = (acc << 6) | static_cast<uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((acc >> bits) & 0xFF));
        }
    }
    return out;
}

namespace {

inline void put_i32(std::string& b, int32_t v) {
    b.push_back(static_cast<char>(v & 0xFF));
    b.push_back(static_cast<char>((v >> 8) & 0xFF));
    b.push_back(static_cast<char>((v >> 16) & 0xFF));
    b.push_back(static_cast<char>((v >> 24) & 0xFF));
}
inline void put_i64(std::string& b, int64_t v) {
    put_i32(b, static_cast<int32_t>(v & 0xFFFFFFFF));
    put_i32(b, static_cast<int32_t>((v >> 32) & 0xFFFFFFFF));
}
inline int32_t get_i32(const std::string& b, size_t& o) {
    if (o + 4 > b.size()) throw std::out_of_range("tl read");
    const int32_t v = static_cast<int32_t>(
        (static_cast<uint32_t>(static_cast<unsigned char>(b[o]))) |
        (static_cast<uint32_t>(static_cast<unsigned char>(b[o + 1])) << 8) |
        (static_cast<uint32_t>(static_cast<unsigned char>(b[o + 2])) << 16) |
        (static_cast<uint32_t>(static_cast<unsigned char>(b[o + 3])) << 24));
    o += 4;
    return v;
}
inline int64_t get_i64(const std::string& b, size_t& o) {
    const int64_t lo = static_cast<uint32_t>(get_i32(b, o));
    const int64_t hi = static_cast<int32_t>(get_i32(b, o));
    return lo | (hi << 32);
}

}  // namespace

std::string rle_encode(const std::string& s) {
    std::string r;
    r.reserve(s.size() + 8);
    size_t zero_run = 0;
    auto flush = [&]() {
        if (zero_run) {
            r.push_back('\0');
            r.push_back(static_cast<char>(zero_run > 254 ? 254 : zero_run));
            if (zero_run > 254) {
                // A run longer than what one byte can express needs the zero pair
                // repeated; python emits one entry per 254 boundary via the loop.
                size_t left = zero_run - 254;
                while (left) {
                    const size_t n = left > 254 ? 254 : left;
                    r.push_back('\0');
                    r.push_back(static_cast<char>(n));
                    left -= n;
                }
            }
            zero_run = 0;
        }
    };
    for (char c : s) {
        if (c == '\0') {
            ++zero_run;
        } else {
            flush();
            r.push_back(c);
        }
    }
    flush();
    return r;
}

std::string rle_decode(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    bool z = false;
    for (char c : s) {
        if (c == '\0') {
            z = true;
            continue;
        }
        if (z) {
            r.append(static_cast<unsigned char>(c), '\0');
            z = false;
        } else {
            r.push_back(c);
        }
    }
    return r;
}

// ---------------- inline message ids ----------------

std::string pack_inline_message_id(const Val& msg_id) {
    const int32_t dc = static_cast<int32_t>(msg_id.has("dc_id") ? msg_id.num("dc_id") : 0);
    const int64_t id = msg_id.num("id");
    const int64_t access_hash = msg_id.num("access_hash");
    std::string b;
    if (msg_id.name == "inputBotInlineMessageID64") {
        put_i32(b, dc);
        put_i32(b, static_cast<int32_t>(msg_id.num("owner_id")));
        put_i64(b, id);
        put_i64(b, access_hash);
    } else {
        put_i32(b, dc);
        put_i64(b, id);
        put_i64(b, access_hash);
    }
    return b64_encode(b);
}

Val unpack_inline_message_id(const std::string& inline_message_id) {
    const std::string decoded = rle_decode(b64_decode(inline_message_id));
    size_t o = 0;
    if (decoded.size() == 20) {
        const int32_t dc = get_i32(decoded, o);
        const int64_t id = get_i64(decoded, o);
        const int64_t access_hash = get_i64(decoded, o);
        return Val::obj("inputBotInlineMessageID",
                        {{"dc_id", Val::i64(dc)},
                         {"id", Val::i64(id)},
                         {"access_hash", Val::i64(access_hash)}});
    }
    if (decoded.size() == 32) {
        const int32_t dc = get_i32(decoded, o);
        const int32_t owner = get_i32(decoded, o);
        const int64_t id = get_i64(decoded, o);
        const int64_t access_hash = get_i64(decoded, o);
        return Val::obj("inputBotInlineMessageID64",
                        {{"dc_id", Val::i64(dc)},
                         {"owner_id", Val::i64(owner)},
                         {"id", Val::i64(id)},
                         {"access_hash", Val::i64(access_hash)}});
    }
    throw std::invalid_argument("Invalid inline_message_id length: " + std::to_string(decoded.size()));
}

// ---------------- file ids ----------------

int file_type_of(const std::string& s) {
    // Accepts either the friendly enum name ("PHOTO") or a raw media constructor
    // name ("photo", "document", ...).
    static const std::map<std::string, int> kNamed = {
        {"THUMBNAIL", 0},   {"CHAT_PHOTO", 1},       {"PHOTO", 2},
        {"VOICE", 3},       {"VIDEO", 4},            {"DOCUMENT", 5},
        {"ENCRYPTED", 6},   {"TEMP", 7},             {"STICKER", 8},
        {"AUDIO", 9},       {"ANIMATION", 10},       {"ENCRYPTED_THUMBNAIL", 11},
        {"WALLPAPER", 12},  {"VIDEO_NOTE", 13},      {"SECURE_RAW", 14},
        {"SECURE", 15},     {"BACKGROUND", 16},      {"DOCUMENT_AS_FILE", 17},
        {"thumbnail", 0},   {"chatPhoto", 1},        {"photo", 2},
        {"voice", 3},       {"video", 4},            {"document", 5},
        {"encrypted", 6},   {"temp", 7},             {"sticker", 8},
        {"audio", 9},       {"animation", 10},       {"encryptedThumbnail", 11},
        {"wallpaper", 12},  {"videoNote", 13},       {"secureRaw", 14},
        {"secureFile", 15}, {"background", 16},      {"documentAsFile", 17},
    };
    auto it = kNamed.find(s);
    if (it == kNamed.end()) throw std::invalid_argument("Unknown file type: " + s);
    return it->second;
}

}  // namespace cppgram::utils
