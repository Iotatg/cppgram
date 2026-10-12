#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "cppgram/client.hpp"

// Session parser supporting the two formats used across the KanhaMusic ecosystem:
//   * pyrogram string sessions (SESSION_TYPE=pyrogram): 271 raw bytes, big-endian
//     dc_id(1) api_id(4) test_mode(1) auth_key(256) user_id(8) bot(1)
//   * gogram string sessions: ascii "1BvE" + base64url json {key,hash,dc_id,ip_addr,app_id,...}
namespace cppgram {

namespace {

std::string b64url_decode(const std::string& in) {
    // Supports both standard base64 (+/) and base64url (-_) alphabets.
    static const std::string alpha =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    int rev[256];
    std::fill(std::begin(rev), std::end(rev), -1);
    for (int i = 0; i < 64; i++) rev[static_cast<unsigned char>(alpha[i])] = i;
    rev[static_cast<unsigned char>('+')] = 62;
    rev[static_cast<unsigned char>('/')] = 63;
    std::string out;
    int bits = 0, acc = 0;
    for (unsigned char c : in) {
        if (c == '=' || c == '\n' || c == '\r') continue;
        int v = rev[c];
        if (v < 0) throw std::runtime_error("mtproto: bad base64url char");
        acc = (acc << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((acc >> bits) & 0xff));
            acc &= (1 << bits) - 1;
        }
    }
    return out;
}

uint32_t be32(const std::string& s, size_t off) {
    return (static_cast<uint32_t>(static_cast<uint8_t>(s[off])) << 24) |
           (static_cast<uint32_t>(static_cast<uint8_t>(s[off + 1])) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(s[off + 2])) << 8) |
           static_cast<uint32_t>(static_cast<uint8_t>(s[off + 3]));
}

uint64_t be64(const std::string& s, size_t off) {
    uint64_t v = 0;
    for (size_t i = 0; i < 8; i++) v = (v << 8) | static_cast<uint8_t>(s[off + i]);
    return v;
}

}  // namespace

namespace {

std::vector<std::string> split_str(const std::string& s, const std::string& sep) {
    std::vector<std::string> out;
    size_t start = 0;
    for (;;) {
        const size_t at = s.find(sep, start);
        if (at == std::string::npos) {
            out.push_back(s.substr(start));
            return out;
        }
        out.push_back(s.substr(start, at - start));
        start = at + sep.size();
    }
}

std::string b64url_encode(const std::string& in) {
    static const char* alpha =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string out;
    int val = 0, bits = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        bits += 8;
        while (bits >= 0) {
            out.push_back(alpha[(val >> bits) & 0x3f]);
            bits -= 6;
        }
    }
    if (bits > -6) out.push_back(alpha[((val << 8) >> (bits + 8)) & 0x3f]);
    return out;
}

// Standard base64 with padding: what Go's encoding/json produces for []byte.
std::string b64_std(const std::string& in) {
    static const char* alpha =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((in.size() * 4 + 2) / 3);
    int val = 0, bits = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        bits += 8;
        while (bits >= 0) {
            out.push_back(alpha[(val >> bits) & 0x3f]);
            bits -= 6;
        }
    }
    if (bits > -6) out.push_back(alpha[((val << 8) >> (bits + 8)) & 0x3f]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

}  // namespace

std::string Client::dc_ip(int dc_id) {
    // gogram default DC table (main net)
    switch (dc_id) {
        case 1: return "149.154.175.58";
        case 2: return "149.154.167.50";
        case 3: return "149.154.175.100";
        case 4: return "149.154.167.91";
        case 5: return "91.108.56.151";
        default: return {};
    }
}

std::string Client::pyrogram_session_string(const Session& ses) {
    // kurigram SESSION_STRING_FORMAT ">BI?256sQ?" (271 bytes, urlsafe b64,
    // padding stripped) — mutually readable with pyrogram string sessions.
    std::string raw;
    raw.push_back(static_cast<char>(ses.dc_id));
    for (int i = 3; i >= 0; i--) raw.push_back(static_cast<char>((ses.api_id >> (8 * i)) & 0xff));
    raw.push_back(0);  // test_mode
    raw += ses.auth_key;
    for (int i = 7; i >= 0; i--) raw.push_back(static_cast<char>((ses.user_id >> (8 * i)) & 0xff));
    raw.push_back(0);  // is_bot
    return b64url_encode(raw);
}

std::string Client::session_string(bool pyrogram_format) const {
    if (pyrogram_format) return pyrogram_session_string(session_);
    // gogram format: "1BvE" + base64url(json) (gogram internal/session/string.go).
    // Go marshals []byte with the standard base64 alphabet plus padding, so the
    // inner fields must use it to stay loadable by gogram.
    const std::string key_b64 = b64_std(session_.auth_key);
    const std::string hash_b64 = b64_std(session_.auth_key.substr(0, 8));
    const std::string js = "{\"key\":\"" + key_b64 + "\",\"hash\":\"" + hash_b64 +
                           "\",\"dc_id\":" + std::to_string(session_.dc_id) +
                           ",\"ip_addr\":\"" + session_.ip + "\",\"app_id\":" +
                           std::to_string(session_.api_id) + "}";
    return "1BvE" + b64url_encode(js);
}

Session Client::parse_session_string(const std::string& encoded) {
    Session ses;
    if (encoded.empty()) throw std::runtime_error("mtproto: empty session string");
    if (encoded.size() > 8192) throw std::runtime_error("mtproto: session string too long");

    // gogram format: prefix + base64url of the JSON session record. Go marshals
    // the key and hash fields with the standard base64 alphabet, and accepts
    // either alphabet when decoding, so b64url_decode covers both.
    if (encoded.compare(0, 4, "1BvE") == 0) {
        const std::string js = b64url_decode(encoded.substr(4));
        std::string perr;
        const Val doc = parse_json(js, &perr);
        if (!perr.empty()) throw std::runtime_error("mtproto: gogram session json: " + perr);
        if (doc.k != Val::OBJ) throw std::runtime_error("mtproto: gogram session not an object");
        const Val& key = doc.at("key");
        const std::string key_b64 = key.k == Val::STR ? key.s : std::string();
        if (key_b64.empty()) throw std::runtime_error("mtproto: gogram session missing key");
        if (doc.at("dc_id").k == Val::NIL) throw std::runtime_error("mtproto: gogram session missing dc_id");
        const long long dc_id = doc.at("dc_id").as_int();
        if (dc_id < 1 || dc_id > 5)
            throw std::runtime_error("mtproto: gogram session dc=" + std::to_string(dc_id));
        ses.auth_key = b64url_decode(key_b64);
        if (ses.auth_key.size() != 256)
            throw std::runtime_error("mtproto: gogram key length " + std::to_string(ses.auth_key.size()));
        ses.dc_id = static_cast<int>(dc_id);
        const Val& ip = doc.at("ip_addr");
        if (ip.k == Val::STR && !ip.s.empty()) ses.ip = ip.s;
        if (ses.ip.empty()) ses.ip = dc_ip(ses.dc_id);
        const Val& app_id = doc.at("app_id");
        ses.api_id = app_id.k == Val::NIL ? 0 : static_cast<int32_t>(app_id.as_int());
        return ses;
    }

    // gogram legacy format: prefix + base64url of
    // auth_key(256) ":_:" hash(8) ":_:" ip_addr ":_:" dc_id ":_:" app_id.
    if (encoded.compare(0, 4, "1BvX") == 0) {
        const std::string raw = b64url_decode(encoded.substr(4));
        const std::string sep = ":_:";
        const std::vector<std::string> parts = split_str(raw, sep);
        if (parts.size() != 5)
            throw std::runtime_error("mtproto: gogram legacy session fields=" +
                                     std::to_string(parts.size()));
        ses.auth_key = parts[0];
        ses.dc_id = std::atoi(parts[3].c_str());
        if (ses.auth_key.size() != 256)
            throw std::runtime_error("mtproto: gogram legacy key length " +
                                     std::to_string(ses.auth_key.size()));
        if (ses.dc_id < 1 || ses.dc_id > 5)
            throw std::runtime_error("mtproto: gogram legacy session dc=" + std::to_string(ses.dc_id));
        ses.ip = parts[2].empty() ? dc_ip(ses.dc_id) : parts[2];
        ses.api_id = static_cast<int32_t>(std::atoll(parts[4].c_str()));
        return ses;
    }

    // pyrogram format (SESSION_TYPE=pyrogram): base64 of 271 bytes.
    // A plain base64url variant is accepted as well.
    const std::string raw = b64url_decode(encoded);
    if (raw.size() == 271) {
        ses.dc_id = static_cast<uint8_t>(raw[0]);
        ses.api_id = static_cast<int32_t>(be32(raw, 1));
        // raw[5] = test_mode
        ses.auth_key = raw.substr(6, 256);
        ses.user_id = static_cast<int64_t>(be64(raw, 262));
        // raw[270] = bot flag
        if (ses.dc_id < 1 || ses.dc_id > 5)
            throw std::runtime_error("mtproto: pyrogram session dc=" + std::to_string(ses.dc_id));
        ses.ip = dc_ip(ses.dc_id);
        return ses;
    }
    throw std::runtime_error("mtproto: unknown session format len=" + std::to_string(raw.size()));
}

}  // namespace cppgram
