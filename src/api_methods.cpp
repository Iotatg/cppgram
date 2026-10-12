// High-level convenience API (pyrogram-style surface) built on the generic
// schema engine. Every method here is thin sugar over Client::api(); the full
// raw RPC surface remains reachable via api()/api_json().
#include "cppgram/client.hpp"
#include "cppgram/args.hpp"
#include "cppgram/utils.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <strings.h>

namespace cppgram {

namespace {

// Callers pass nullptr for "I don't care about errors", so every check has to
// survive a null pointer just like the write side already does.
inline bool has_err(const std::string* err) { return err && !err->empty(); }

// Build {"k":v,...} JSON argument strings safely for integer/bool args.
std::string peer_json(const Peer& p) {
    char buf[160];
    switch (p.kind) {
        case PeerKind::Chat:
            std::snprintf(buf, sizeof(buf), "{\"_ctor\":\"inputPeerChat\",\"chat_id\":%lld}",
                          (long long)p.id);
            return buf;
        case PeerKind::User:
            std::snprintf(buf, sizeof(buf), "{\"_ctor\":\"inputPeerUser\",\"user_id\":%lld,\"access_hash\":%lld}",
                          (long long)p.id, (long long)p.access_hash);
            return buf;
        case PeerKind::Channel:
            std::snprintf(buf, sizeof(buf), "{\"_ctor\":\"inputPeerChannel\",\"channel_id\":%lld,\"access_hash\":%lld}",
                          (long long)p.id, (long long)p.access_hash);
            return buf;
    }
    return "{\"_ctor\":\"inputPeerEmpty\"}";
}

// {"id":[{"_ctor":"inputMessageID",...}]} style arrays of InputMessage
std::string input_messages_json(const std::vector<long long>& msg_ids) {
    std::string out = "[";
    for (size_t i = 0; i < msg_ids.size(); i++) {
        if (i) out += ',';
        char b[80];
        std::snprintf(b, sizeof(b), "{\"_ctor\":\"inputMessageID\",\"id\":%lld}", (long long)msg_ids[i]);
        out += b;
    }
    out += ']';
    return out;
}

std::string escape_json(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { out += '\\'; out += (char)c; }
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c < 0x20) {
            char b[8];
            std::snprintf(b, sizeof(b), "\\u%04x", c);
            out += b;
        } else {
            out += (char)c;
        }
    }
    out += '"';
    return out;
}

}  // namespace

// ------------------------------- identity / users

Val Client::get_me(std::string* err) {
    if (me_.k != Val::NIL) return me_;
    Val r = api_json("users.getUsers", "{\"id\":[{\"_ctor\":\"inputUserSelf\"}]}", err);
    if (has_err(err)) return {};
    // response may be boxed users{users:[...]} or a bare Vector<User>
    const Val* src = &r;
    if (r.k == Val::OBJ) src = &r.at("users");
    if (src->k != Val::VEC || src->v.empty()) {
        *err = "get_me: empty response";
        return {};
    }
    me_ = src->v[0];
    return me_;
}

Val Client::get_users(const std::vector<long long>& ids, std::string* err) {
    std::string arr = "[";
    for (size_t i = 0; i < ids.size(); i++) {
        if (i) arr += ',';
        arr += "{\"_ctor\":\"inputUser\",\"user_id\":" + std::to_string(ids[i]) + ",\"access_hash\":0}";
    }
    arr += ']';
    return api_json("users.getUsers", "{\"id\":" + arr + "}", err);
}

Val Client::resolve_username(const std::string& username, std::string* err) {
    Val r = api_json("contacts.resolveUsername",
                     "{\"username\":" + escape_json(username) + "}", err);
    if (has_err(err)) return {};
    for (const Val& u : r.list("users")) cache_peer_from_node(u);
    for (const Val& c : r.list("chats")) cache_peer_from_node(c);
    return r;
}

// ------------------------------- dialogs / chats

Val Client::get_dialogs_full(int limit, std::string* err) {
    char args[96];
    std::snprintf(args, sizeof(args), "{\"limit\":%d,\"pinned\":true}", limit);
    Val r = api_json("messages.getDialogs", args, err);
    if (has_err(err)) return {};
    for (const Val& c : r.list("chats")) cache_peer_from_node(c);
    for (const Val& u : r.list("users")) cache_peer_from_node(u);
    return r;
}

Val Client::get_chat(const Peer& p, bool full, std::string* err) {
    if (p.kind == PeerKind::Channel) {
        std::string a = "{\"channel\":" + input_channel_json(p) + "}";
        if (full) return api_json("channels.getFullChannel", a, err);
        return api_json("channels.getChannels", "{\"id\":[" + input_channel_json(p) + "]}", err);
    }
    char a[160];
    std::snprintf(a, sizeof(a), full
                      ? "{\"chat_id\":{\"_ctor\":\"inputChat\",\"chat_id\":%lld,\"access_hash\":0}}"
                      : "{\"id\":[{\"_ctor\":\"inputChat\",\"chat_id\":%lld,\"access_hash\":0}]}",
                  (long long)p.id);
    return api_json(full ? "channels.getFullChannel" : "messages.getChats", a, err);
}

bool Client::join_chat(const Peer& p, std::string* err) {
    std::string a = "{\"channel\":" + input_channel_json(p) + ",\"invite_hash\":\"20\"}";
    api_json("channels.joinChannel", a, err);
    return !has_err(err);
}

bool Client::leave_chat(const Peer& p, std::string* err) {
    std::string a = "{\"channel\":" + input_channel_json(p) + "}";
    api_json("channels.leaveChannel", a, err);
    return !has_err(err);
}

// ------------------------------- members / permissions

Val Client::get_chat_members(const Peer& p, int offset, int limit, std::string* err) {
    std::string a = "{\"channel\":" + input_channel_json(p) +
                    ",\"filter\":{\"_ctor\":\"channelParticipantsRecent\"},\"offset\":" +
                    std::to_string(offset) + ",\"limit\":" + std::to_string(limit) + ",\"hash\":0}";
    Val r = api_json("channels.getParticipants", a, err);
    if (has_err(err)) return {};
    for (const Val& u : r.list("users")) cache_peer_from_node(u);
    return r;
}

bool Client::ban_chat_member(const Peer& chat, long long user_id, bool revoke, std::string* err) {
    std::string a = "{\"channel\":" + input_channel_json(chat) +
                    ",\"participant\":{\"_ctor\":\"inputPeerUser\",\"user_id\":" +
                    std::to_string(user_id) + ",\"access_hash\":0}" +
                    ",\"banned_rights\":{\"_ctor\":\"chatBannedRights\",\"view_messages\":true,"
                    "\"use_messages\":true,\"send_messages\":true,\"send_media\":true,"
                    "\"send_stickers\":true,\"send_gifs\":true,\"send_games\":true,"
                    "\"send_inline\":true,\"embed_links\":true,\"send_polls\":true,"
                    "\"change_info\":true,\"admin\":false,\"until_date\":4294967295}" +
                    (revoke ? ",\"revoke\":true" : "");
    api_json("channels.editBanned", a, err);
    return !has_err(err);
}

bool Client::unban_chat_member(const Peer& chat, long long user_id, std::string* err) {
    std::string a = "{\"channel\":" + input_channel_json(chat) +
                    ",\"participant\":{\"_ctor\":\"inputPeerUser\",\"user_id\":" +
                    std::to_string(user_id) + ",\"access_hash\":0},\"banned_rights\":{\"_ctor\":\"chatBannedRights\"}}";
    api_json("channels.editBanned", a, err);
    return !has_err(err);
}

bool Client::promote_chat_member(const Peer& chat, long long user_id, bool admin, std::string* err) {
    std::string rights = admin
        ? "{\"_ctor\":\"chatAdminRights\",\"change_info\":true,\"delete_messages\":true,"
          "\"invite_users\":true,\"manage_call\":true,\"pin_messages\":true,\"post_messages\":true}"
        : "{\"_ctor\":\"chatAdminRights\"}";
    std::string a = "{\"channel\":" + input_channel_json(chat) +
                    ",\"user_id\":{\"_ctor\":\"inputUser\",\"user_id\":" + std::to_string(user_id) +
                    ",\"access_hash\":0},\"admin_rights\":" + rights + (admin ? ",\"rank\":\"Admin\"" : "");
    api_json("channels.editAdmin", a, err);
    return !has_err(err);
}

// ------------------------------- message list ops

Val Client::fetch_messages(const Peer& p, const std::vector<long long>& ids, std::string* err) {
    std::string a = "{\"peer\":" + peer_json(p) + ",\"id\":" + input_messages_json(ids) + "}";
    return api_json("messages.getMessages", a, err);
}

Val Client::fetch_history_full(const Peer& p, int limit, long long offset_id, std::string* err) {
    char a[400];
    std::snprintf(a, sizeof(a),
                  "{\"peer\":%s,\"limit\":%d,\"offset_id\":%lld,\"offset_date\":0,"
                  "\"min_id\":0,\"max_id\":0,\"hash\":0}",
                  peer_json(p).c_str(), limit, (long long)offset_id);
    Val r = api_json("messages.getHistory", a, err);
    if (has_err(err)) return {};
    for (const Val& c : r.list("chats")) cache_peer_from_node(c);
    for (const Val& u : r.list("users")) cache_peer_from_node(u);
    return r;
}

bool Client::forward_messages(const Peer& from, const std::vector<long long>& ids, const Peer& to,
                              bool silent, std::string* err) {
    // messages.forwardMessages#13704a7c: id:Vector<int>, random_id:Vector<long>
    // (both required, plain arrays).
    std::string id_list = "[";
    std::string rands = "[";
    for (size_t i = 0; i < ids.size(); i++) {
        if (i) {
            id_list += ',';
            rands += ',';
        }
        id_list += std::to_string(ids[i]);
        unsigned long long r1 = static_cast<unsigned long long>(std::rand()) << 34;
        unsigned long long r2 = static_cast<unsigned long long>(std::rand()) << 17;
        rands += std::to_string(static_cast<long long>(r1 ^ r2 ^ static_cast<unsigned>(std::rand())));
    }
    id_list += ']';
    rands += ']';
    std::string a = "{\"from_peer\":" + peer_json(from) + ",\"id\":" + id_list +
                    ",\"random_id\":" + rands + ",\"to_peer\":" + peer_json(to) +
                    (silent ? ",\"silent\":true" : "") + "}";
    api_json("messages.forwardMessages", a, err);
    return !has_err(err);
}

bool Client::pin_message(const Peer& p, long long msg_id, bool unpin, std::string* err) {
    char a[320];
    std::snprintf(a, sizeof(a), "{\"peer\":%s,\"id\":%lld,\"unpin\":%s}",
                  peer_json(p).c_str(), (long long)msg_id, unpin ? "true" : "false");
    api_json("messages.updatePinnedMessage", a, err);
    return !has_err(err);
}

bool Client::set_reaction(const Peer& p, long long msg_id, const std::string& emoji, std::string* err) {
    // messages.sendReaction#d30d78d4 flags:# big:flags.1?true
    //   add_to_recent:flags.2?true peer:InputPeer msg_id:int
    //   reaction:flags.0?Vector<Reaction>
    std::string e = emoji.empty()
                        ? ""
                        : ",\"reaction\":[{\"_ctor\":\"reactionEmoji\",\"emoticon\":" +
                              escape_json(emoji) + "}]";
    std::string a = "{\"peer\":" + peer_json(p) + ",\"msg_id\":" + std::to_string(msg_id) + e +
                    "}";
    api_json("messages.sendReaction", a, err);
    return !has_err(err);
}

// ------------------------------- misc helpers

Val Client::invoke_raw(const std::string& method, const std::string& args_json, std::string* err) {
    return api_json(method, args_json, err);
}

std::string Client::input_channel_json(const Peer& p) {
    char b[160];
    std::snprintf(b, sizeof(b), "{\"_ctor\":\"inputChannel\",\"channel_id\":%lld,\"access_hash\":%lld}",
                  (long long)p.id, (long long)p.access_hash);
    return b;
}

void Client::cache_peer_from_node(const Val& node) {
    if (node.k != Val::OBJ) return;
    Peer p;
    std::string id_key;
    if (node.name == "channel" || node.name == "channelForbidden" || node.name == "channelMin" ||
        node.name == "channelForwards") {
        p.kind = PeerKind::Channel;
        id_key = "id";
    } else if (node.name == "inputPeerChannel") {  // persisted peer-cache shape
        p.kind = PeerKind::Channel;
        id_key = "channel_id";
    } else if (node.name == "user" || node.name == "userSelf" || node.name == "userMinimum") {
        p.kind = PeerKind::User;
        id_key = "id";
    } else if (node.name == "inputPeerUser") {  // persisted peer-cache shape
        p.kind = PeerKind::User;
        id_key = "user_id";
    } else if (node.name == "chat" || node.name == "chatForbidden" || node.name == "chatMin") {
        p.kind = PeerKind::Chat;
        id_key = "id";
    } else if (node.name == "inputPeerChat") {  // persisted peer-cache shape
        p.kind = PeerKind::Chat;
        id_key = "chat_id";
    } else {
        return;
    }
    p.id = node.num(id_key.c_str());
    p.access_hash = node.num("access_hash");
    if (p.id == 0) return;
    auto it = peers_.find(p.marked());
    if (it != peers_.end() && it->second.access_hash && !p.access_hash) p.access_hash = it->second.access_hash;
    peers_[p.marked()] = p;
}

// ---------------------------------------------------------------- media transfer

namespace {

// Compact MD5 (RFC 1321): inputFile.md5_checksum requires whole-file digest.
std::string md5_hex(const std::string& msg) {
    static const uint32_t K[64] = {
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
        0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
        0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
        0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
        0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
        0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391};
    static const int R[64] = {
        7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
        5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
        4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
        6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
    uint32_t a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;
    std::string m = msg;
    uint64_t bits = static_cast<uint64_t>(m.size()) * 8;
    m += static_cast<char>(0x80);
    while (m.size() % 64 != 56) m += '\0';
    for (int i = 0; i < 8; i++) m += static_cast<char>((bits >> (8 * i)) & 0xFF);
    for (size_t off = 0; off < m.size(); off += 64) {
        uint32_t M[16];
        for (int i = 0; i < 16; i++)
            M[i] = static_cast<unsigned char>(m[off + i * 4]) |
                   (static_cast<unsigned char>(m[off + i * 4 + 1]) << 8) |
                   (static_cast<unsigned char>(m[off + i * 4 + 2]) << 16) |
                   (static_cast<unsigned char>(m[off + i * 4 + 3]) << 24);
        uint32_t A = a0, B = b0, C = c0, D = d0;
        for (int i = 0; i < 64; i++) {
            uint32_t F;
            int g;
            if (i < 16) { F = (B & C) | (~B & D); g = i; }
            else if (i < 32) { F = (D & B) | (~D & C); g = (5 * i + 1) % 16; }
            else if (i < 48) { F = B ^ C ^ D; g = (3 * i + 5) % 16; }
            else { F = C ^ (B | ~D); g = (7 * i) % 16; }
            F += A + K[i] + M[g];
            A = D; D = C; C = B;
            B += (F << R[i]) | (F >> (32 - R[i]));
        }
        a0 += A; b0 += B; c0 += C; d0 += D;
    }
    char hex[33];
    uint32_t w[4] = {a0, b0, c0, d0};
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            std::snprintf(hex + i * 8 + j * 2, 3, "%02x", (w[i] >> (8 * j)) & 0xFF);
    hex[32] = 0;
    return hex;
}

int64_t rnd64() {
    static thread_local uint64_t x =
        static_cast<uint64_t>(std::random_device{}()) << 32 ^
        static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    x ^= x << 13; x ^= x >> 7; x ^= x << 17;
    return static_cast<int64_t>(x ? x : 1);
}

}  // namespace

Val Client::upload_file(const std::string& data, const std::string& name, std::string* err) {
    const int64_t fid = rnd64();
    const size_t part_size = 512 * 1024;
    int parts = 0;
    for (size_t off = 0; off < data.size() || off == 0; off += part_size, parts++) {
        std::string chunk = data.substr(off, part_size);
        Val args = Val::obj("upload.saveFilePart", {
            {"file_id", Val::i64(fid)},
            {"file_part", Val::i64(parts)},
            {"bytes", Val::str(std::move(chunk))},
        });
        api("upload.saveFilePart", args, err);
        if (has_err(err)) return {};
        if (data.size() - off <= part_size) { parts++; break; }
    }
    return Val::obj("inputFile", {
        {"id", Val::i64(fid)},
        {"parts", Val::i64(parts)},
        {"name", Val::str(name)},
        {"md5_checksum", Val::str(md5_hex(data))},
    });
}

Val Client::send_photo_message(const Peer& p, const std::string& jpeg, const std::string& name,
                               const std::string& caption, std::string* err) {
    Val file = upload_file(jpeg, name, err);
    if (file.k == Val::NIL) return {};
    Val media = Val::obj("inputMediaUploadedPhoto", {{"file", std::move(file)}});
    std::string pj_err;
    Val peer_v = parse_json(peer_json(p), &pj_err);
    Val args = Val::obj("messages.sendMedia", {
        {"peer", std::move(peer_v)},
        {"media", std::move(media)},
        {"message", Val::str(caption)},
        {"random_id", Val::i64(rnd64())},
    });
    return api("messages.sendMedia", args, err);
}

bool Client::download_location(const Val& loc, const std::string& path, std::string* err) {
    std::string out;
    const int64_t limit = 512 * 1024;
    for (int64_t off = 0;;) {
        Val args = Val::obj("upload.getFile", {
            {"location", loc},
            {"offset", Val::i64(off)},
            {"limit", Val::i64(limit)},
        });
        Val r = api("upload.getFile", args, err);
        if (has_err(err)) return false;
        if (r.k != Val::OBJ || r.name != "upload.file") {
            if (err) *err = "upload.getFile: unexpected " + r.name;
            return false;
        }
        const std::string& chunk = r.text("bytes");
        out += chunk;
        off += static_cast<int64_t>(chunk.size());
        if (chunk.empty() || chunk.size() < static_cast<size_t>(limit)) break;
    }
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) { if (err) *err = "cannot open " + path; return false; }
    std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
    return true;
}

bool Client::download_user_photo(long long user_id, const std::string& path, std::string* err) {
    Peer pu;
    pu.kind = PeerKind::User;
    pu.id = user_id;
    std::string e2;
    if (!peer_for(user_id, &pu, &e2) || !pu.access_hash) {
        // Try seeding access hashes from the contacts list once.
        Val cs = api_json("contacts.getContacts", "{}", err);
        if (!has_err(err)) {
            for (const Val& u : cs.list("users")) cache_peer_from_node(u);
        }
        if (err) err->clear();
        peer_for(user_id, &pu, &e2);
    }
    if (!pu.access_hash) { *err = "no cached access_hash for " + std::to_string(user_id); return false; }
    Val r = api_json("photos.getUserPhotos",
                     "{\"user_id\":{\"_ctor\":\"inputUser\",\"user_id\":" + std::to_string(user_id) +
                     ",\"access_hash\":" + std::to_string(pu.access_hash) +
                     "},\"offset\":0,\"max_id\":0,\"limit\":1}", err);
    if (has_err(err)) return false;
    std::vector<Val> photos = r.list("photos");
    if (photos.empty()) { *err = "no photos"; return false; }
    const Val& ph = photos[0];
    if (ph.name != "photo") { *err = "photo empty (id=" + std::to_string(ph.num("id")) + ")"; return false; }
    // Pick the largest real size type; an empty thumb_size is rejected with
    // FILE_REFERENCE_EXPIRED by current servers.
    std::string thumb;
    long long best = -1;
    for (const Val& sz : ph.list("sizes")) {
        if (sz.name == "photoSizeEmpty") continue;
        long long w = sz.num("w"), h = sz.num("h");
        if (sz.name == "photoStrippedSize") { if (thumb.empty()) thumb = sz.text("type"); continue; }
        if (sz.name == "photoPathSize") continue;
        if (w <= 0 || h <= 0) { thumb = sz.text("type"); continue; }
        if (w * h > best) { best = w * h; thumb = sz.text("type"); }
    }
    for (const Val& sz : ph.list("video_sizes")) {
        long long w = sz.num("w"), h = sz.num("h");
        if (w * h > best) { best = w * h; thumb = sz.text("type"); }
    }
    if (thumb.empty()) { *err = "photo has no downloadable size"; return false; }
    const char* tv = getenv("CG_THUMB");
    std::string thumb_s = tv ? tv : thumb;
    Val loc = Val::obj("inputPhotoFileLocation", {
        {"id", Val::i64(ph.num("id"))},
        {"access_hash", Val::i64(ph.num("access_hash"))},
        {"file_reference", Val::str(ph.text("file_reference"))},
        {"thumb_size", Val::str(thumb_s)},
    });
    return download_location(loc, path, err);
}

Val Client::send_document_media(const Peer& p, const std::string& data, const std::string& name,
                                const std::string& mime, std::vector<Val> attributes,
                                bool force_file, const std::string& caption, std::string* err) {
    if (attributes.empty())
        attributes.push_back(Val::obj("documentAttributeFilename", {{"file_name", Val::str(name)}}));
    Val file = upload_file(data, name, err);
    if (file.k == Val::NIL) return {};
    std::map<std::string, Val> media_fields = {
        {"file", std::move(file)},
        {"mime_type", Val::str(mime)},
        {"attributes", Val::vec(std::move(attributes))},
    };
    if (force_file) media_fields["force_file"] = Val::i64(1);
    Val media = Val::obj("inputMediaUploadedDocument", std::move(media_fields));
    std::string pj_err;
    Val peer_v = parse_json(peer_json(p), &pj_err);
    Val args = Val::obj("messages.sendMedia", {
        {"peer", std::move(peer_v)},
        {"media", std::move(media)},
        {"message", Val::str(caption)},
        {"random_id", Val::i64(rnd64())},
    });
    return api("messages.sendMedia", args, err);
}

Val Client::send_document_message(const Peer& p, const std::string& data,
                                  const std::string& file_name, const std::string& caption,
                                  bool force_file, std::string* err) {
    std::string mime = "application/octet-stream";
    auto ends = [&](const char* s) {
        size_t n = strlen(s);
        return file_name.size() >= n &&
               strcasecmp(file_name.c_str() + file_name.size() - n, s) == 0;
    };
    if (ends(".txt") || ends(".md") || ends(".log")) mime = "text/plain";
    else if (ends(".pdf")) mime = "application/pdf";
    else if (ends(".zip")) mime = "application/zip";
    else if (ends(".png")) mime = "image/png";
    else if (ends(".mp3")) mime = "audio/mpeg";
    else if (ends(".mp4")) mime = "video/mp4";
    return send_document_media(p, data, file_name, mime, {}, force_file, caption, err);
}

Val Client::send_voice_message(const Peer& p, const std::string& ogg, double duration_sec,
                               const std::string& caption, std::string* err) {
    std::vector<Val> attrs = {
        Val::obj("documentAttributeAudio", {
            {"voice", Val::i64(1)},
            {"duration", Val::i64(duration_sec < 0 ? 0 : static_cast<long long>(duration_sec + 0.5))},
        }),
        Val::obj("documentAttributeFilename", {{"file_name", Val::str("voice.ogg")}}),
    };
    return send_document_media(p, ogg, "voice.ogg", "audio/ogg; codecs=opus", std::move(attrs),
                               false, caption, err);
}

Val Client::send_video_message(const Peer& p, const std::string& mp4, const std::string& file_name,
                               const std::string& caption, double duration, int w, int h,
                               std::string* err) {
    std::vector<Val> attrs = {
        Val::obj("documentAttributeVideo", {
            {"supports_streaming", Val::i64(1)},
            {"duration", Val::dbl(duration)},
            {"w", Val::i64(w)},
            {"h", Val::i64(h)},
        }),
        Val::obj("documentAttributeFilename", {{"file_name", Val::str(file_name)}}),
    };
    return send_document_media(p, mp4, file_name, "video/mp4", std::move(attrs), false, caption, err);
}

bool Client::download_document_msg(const Peer& p, long long msg_id, const std::string& path,
                                   std::string* err) {
    Val r = fetch_messages(p, {msg_id}, err);
    if (has_err(err)) return false;
    std::vector<Val> msgs = r.list("messages");
    if (msgs.empty()) { *err = "message not found"; return false; }
    const Val& doc = msgs[0].at("media").at("document");
    if (doc.name != "document") { *err = "message has no document (" + msgs[0].at("media").name + ")"; return false; }
    Val loc = Val::obj("inputDocumentFileLocation", {
        {"id", Val::i64(doc.num("id"))},
        {"access_hash", Val::i64(doc.num("access_hash"))},
        {"file_reference", Val::str(doc.text("file_reference"))},
        {"thumb_size", Val::str("")},
    });
    return download_location(loc, path, err);
}

// ---------------------------------------------------------------- auth flows

namespace {

// auth.sendCode answers 303 PHONE_MIGRATE_<dc> (or NETWORK_MIGRATE_<dc>) when the
// account's home DC is not the one we asked, which is the normal case for a
// first login: the number was registered somewhere else. The reply is not a
// failure, it is Telegram handing us the DC we should have used.
bool migrate_target_dc(const std::string& err, int* dc) {
    if (err.find(":PHONE_MIGRATE_") == std::string::npos &&
        err.find(":NETWORK_MIGRATE_") == std::string::npos)
        return false;
    const size_t us = err.rfind('_');
    if (us == std::string::npos) return false;
    const std::string tail = err.substr(us + 1);
    if (tail.empty() || tail.size() > 2) return false;
    for (char c : tail)
        if (c < '0' || c > '9') return false;
    const int v = std::atoi(tail.c_str());
    if (v < 1 || v > 5) return false;
    *dc = v;
    return true;
}

}  // namespace

Val Client::auth_send_code(const std::string& phone, int32_t api_id, const std::string& api_hash,
                           std::string* err) {
    auto ask = [&]() {
        return api("auth.sendCode", Val::obj("auth.sendCode", {
                                                {"phone_number", Val::str(phone)},
                                                {"api_id", Val::i64(api_id)},
                                                {"api_hash", Val::str(api_hash)},
                                                {"settings", Val::obj("codeSettings", {})},
                                            }),
                   err);
    };
    Val r = ask();
    int dc = 0;
    if (!has_err(err) || !migrate_target_dc(*err, &dc)) return r;
    // Re-key on the DC Telegram pointed at and ask there. The key we hold is
    // bound to nothing until auth.sends the code, so a fresh handshake is all
    // that is needed to be a valid client on the new DC.
    const std::string ip = dc_ip(dc);
    if (ip.empty()) return r;
    std::string key, e2;
    if (!generate_auth_key(ip, 443, dc, &key, &e2)) {
        *err = "sendCode: migrate to dc" + std::to_string(dc) + ": " + e2;
        return {};
    }
    session_.dc_id = dc;
    session_.ip = ip;
    session_.auth_key = std::move(key);
    disconnect();
    if (!connect(&e2)) {
        *err = "sendCode: reconnect to dc" + std::to_string(dc) + ": " + e2;
        return {};
    }
    err->clear();
    return ask();
}

bool Client::auth_sign_in(const std::string& phone, const std::string& code_hash,
                          const std::string& code, std::string* err) {
    Val args = Val::obj("auth.signIn", {
        {"phone_number", Val::str(phone)},
        {"phone_code_hash", Val::str(code_hash)},
        {"phone_code", Val::str(code)},
    });
    Val r = api("auth.signIn", args, err);
    if (has_err(err)) return false;
    if (r.name != "auth.authorization") {
        *err = "signIn: " + (r.name.empty() ? "empty" : r.name);
        return false;
    }
    long long uid = r.dig("user.id").i;
    if (!uid) { *err = "signIn: no user in authorization"; return false; }
    session_.user_id = uid;
    return true;
}

bool Client::auth_sign_up(const std::string& phone, const std::string& code_hash,
                          const std::string& first_name, const std::string& last_name,
                          std::string* err) {
    Val args = Val::obj("auth.signUp", {
        {"phone_number", Val::str(phone)},
        {"phone_code_hash", Val::str(code_hash)},
        {"first_name", Val::str(first_name)},
        {"last_name", Val::str(last_name)},
    });
    Val r = api("auth.signUp", args, err);
    if (has_err(err)) return false;
    if (r.name != "auth.authorization") {
        *err = "signUp: " + (r.name.empty() ? "empty" : r.name);
        return false;
    }
    long long uid = r.dig("user.id").i;
    if (!uid) { *err = "signUp: no user in authorization"; return false; }
    session_.user_id = uid;
    return true;
}

bool Client::auth_resend_code(const std::string& phone, const std::string& code_hash,
                              std::string* err) {
    Val args = Val::obj("auth.resendCode", {
        {"phone_number", Val::str(phone)},
        {"phone_code_hash", Val::str(code_hash)},
    });
    api("auth.resendCode", args, err);
    return !has_err(err);
}

bool Client::auth_cancel_code(const std::string& phone, std::string* err) {
    api_json("auth.cancelCode", "{\"phone_number\":" + escape_json(phone) + "}", err);
    return !has_err(err);
}

// Builds the auth.checkPassword argument tree. Kept separate from
// Client::auth_check_password so the byte handling can be tested without a live
// account: the JSON boxes cannot hold raw bytes, so A and M1 arrive base64url
// and the encoder writes Val::STR verbatim — skipping the decode sent 342 bytes
// of alphabet on the wire and Telegram answered SRP_A_INVALID.
Val srp_check_password_args(const Val& password_obj, const std::string& password,
                            std::string* err) {
    std::string srp = srp_password_check(password_obj, password, err);
    if (has_err(err)) return {};
    std::string pj_err;
    Val srp_obj = parse_json(srp, &pj_err);
    if (!pj_err.empty()) {
        if (err) *err = "srp json: " + pj_err;
        return {};
    }
    for (const char* f : {"A", "M1"}) {
        auto it = srp_obj.o.find(f);
        if (it == srp_obj.o.end() || it->second.k != Val::STR) continue;
        try {
            it->second = Val::str(utils::b64_decode(it->second.s));
        } catch (const std::exception& e) {
            if (err) *err = std::string("srp: bad base64 in ") + f + ": " + e.what();
            return {};
        }
    }
    return Val::obj("auth.checkPassword", {{"password", srp_obj}});
}

bool Client::auth_check_password(const std::string& password, std::string* err) {
    Val pw = api("account.getPassword", Val::obj("account.getPassword", {}), err);
    if (has_err(err)) return false;
    Val args = srp_check_password_args(pw, password, err);
    if (err && !err->empty()) return false;
    Val r = api("auth.checkPassword", args, err);
    if (has_err(err)) return false;
    if (r.name != "auth.authorization") {
        *err = "checkPassword: " + (r.name.empty() ? "empty" : r.name);
        return false;
    }
    long long uid = r.dig("user.id").i;
    if (!uid) { *err = "checkPassword: no user in authorization"; return false; }
    session_.user_id = uid;
    return true;
}

bool Client::auth_log_out(std::string* err) {
    api("auth.logOut", Val::obj("auth.logOut", {}), err);
    return !has_err(err);
}

// ---------------------------------------------------------------- markdown / media extras

Val Client::send_markdown(const Peer& p, const std::string& md, int64_t reply_to_msg_id,
                          std::string* err) {
    std::string plain;
    std::vector<Val> entities = parse_markdown(md, &plain);
    std::map<std::string, Val> args = {
        {"peer", parse_json(peer_json(p), err)},
        {"message", Val::str(plain)},
        {"random_id", Val::i64(rnd64())},
        {"entities", Val::vec(std::move(entities))},
    };
    if (reply_to_msg_id) {
        args["reply_to"] = Val::obj("inputReplyToMessage", {{"id", Val::i64(reply_to_msg_id)}});
    }
    return api("messages.sendMessage", Val::obj("messages.sendMessage", std::move(args)), err);
}

Val Client::download_media_msg(const Peer& p, int64_t msg_id, const std::string& path,
                               std::string* err) {
    Val r = fetch_messages(p, {msg_id}, err);
    if (has_err(err)) return {};
    std::vector<Val> msgs = r.list("messages");
    if (msgs.empty()) { *err = "message not found"; return {}; }
    const Val& m = msgs[0];
    const Val& media = m.at("media");
    if (media.k != Val::OBJ || media.name.empty()) { *err = "message has no media"; return {}; }
    if (media.name == "messageMediaPhoto") {
        const Val& ph = media.at("photo");
        if (ph.name != "photo") { *err = "empty photo"; return {}; }
        // Largest real size type wins (empty thumb_size answers FILE_REFERENCE_EXPIRED).
        std::string thumb;
        long long best = -1;
        for (const Val& sz : ph.list("sizes")) {
            if (sz.name == "photoSizeEmpty" || sz.name == "photoPathSize") continue;
            if (sz.name == "photoStrippedSize") { if (thumb.empty()) thumb = sz.text("type"); continue; }
            long long w = sz.num("w"), h = sz.num("h");
            if (w <= 0 || h <= 0) { thumb = sz.text("type"); continue; }
            if (w * h > best) { best = w * h; thumb = sz.text("type"); }
        }
        if (thumb.empty()) { *err = "photo has no downloadable size"; return {}; }
        Val loc = Val::obj("inputPhotoFileLocation", {
            {"id", Val::i64(ph.num("id"))},
            {"access_hash", Val::i64(ph.num("access_hash"))},
            {"file_reference", Val::str(ph.text("file_reference"))},
            {"thumb_size", Val::str(thumb)},
        });
        if (!download_location(loc, path, err)) return {};
        return ph;
    }
    if (media.name == "messageMediaDocument") {
        const Val& doc = media.at("document");
        if (doc.name != "document") { *err = "empty document"; return {}; }
        Val loc = Val::obj("inputDocumentFileLocation", {
            {"id", Val::i64(doc.num("id"))},
            {"access_hash", Val::i64(doc.num("access_hash"))},
            {"file_reference", Val::str(doc.text("file_reference"))},
            {"thumb_size", Val::str("")},
        });
        if (!download_location(loc, path, err)) return {};
        return doc;
    }
    *err = "unsupported media type: " + media.name;
    return {};
}

Val Client::search_messages(const std::string& query, int limit, std::string* err) {
    std::string args = "{\"q\":" + escape_json(query) +
                       ",\"filter\":{\"_ctor\":\"inputMessagesFilterEmpty\"}"
                       ",\"min_date\":0,\"max_date\":0,\"offset_rate\":0"
                       ",\"offset_peer\":{\"_ctor\":\"inputPeerEmpty\"}"
                       ",\"offset_id\":0,\"limit\":" +
                       std::to_string(limit) + "}";
    return api_json("messages.searchGlobal", args, err);
}

// Peer cache persistence: {"<marked_id>": <inputPeer json>, ...}
bool Client::save_peer_cache(const std::string& path, std::string* err) {
    std::string js = "{";
    bool first = true;
    for (const auto& [marked, peer] : peers_) {
        if (!marked || !peer.access_hash) continue;
        if (!first) js += ',';
        first = false;
        js += "\"" + std::to_string(marked) + "\":" + peer_json(peer);
    }
    js += "}";
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) { if (err) *err = "cannot open " + path; return false; }
    std::fwrite(js.data(), 1, js.size(), f);
    std::fclose(f);
    return true;
}

bool Client::load_peer_cache(const std::string& path, std::string* err) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) { if (err) *err = "cannot open " + path; return false; }
    std::string js;
    char buf[8192];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) js.append(buf, n);
    std::fclose(f);
    std::string pj_err;
    Val root = parse_json(js, &pj_err);
    if (!pj_err.empty()) { if (err) *err = "peer cache json: " + pj_err; return false; }
    int loaded = 0;
    for (const auto& [marked, val] : root.o) {
        std::string perr;
        if (val.k == Val::OBJ) cache_peer_from_node(val);
        ++loaded;
    }
    (void)loaded;
    return true;
}

}  // namespace cppgram
