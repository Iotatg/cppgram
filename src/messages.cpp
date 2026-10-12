#include "cppgram/client.hpp"
#include "cppgram/tl.hpp"

#include <chrono>
#include <cstring>
#include <random>
#include <cstdio>
#include <cstdlib>

// CRCs verified against tdesktop dev api.tl (telegram layer 228/229).
#define CRC_SEND_MESSAGE 0xfef48f62u
#define CRC_EDIT_MESSAGE 0xb106e66cu
#define CRC_DELETE_MESSAGES 0xe58e95d2u
#define CRC_GET_HISTORY 0x4423e6c5u
#define CRC_GET_DIALOGS 0xa0f4cb4fu
#define CRC_SET_TYPING 0x58943ee2u
#define CRC_TYPING_ACTION 0x16bf744eu
#define CRC_READ_HISTORY 0x0e306d3au
#define CRC_INPUT_PEER_CHANNEL 0x27bcbbfcu
#define CRC_INPUT_PEER_USER 0xdde8a54cu
#define CRC_INPUT_PEER_CHAT 0x35a95cb9u
#define CRC_INPUT_PEER_EMPTY 0x7f3b18eau
#define CRC_INPUT_REPLY_TO 0x3bd4b7c2u
#define CRC_UPDATE_MESSAGE_ID 0x4e90bfd6u
#define CRC_PEER_CHANNEL 0xa2a5371eu
#define CRC_PEER_USER 0x59511722u

namespace cppgram {

namespace {

int64_t random64() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    int64_t v;
    do {
        v = static_cast<int64_t>(rng());
    } while (v == 0);
    return v;
}

// inputPeerChannel#27bcbbfc channel_id:long access_hash:long
// inputPeerUser#dde8a54c  user_id:long    access_hash:long
// inputPeerChat#35a95cb9  chat_id:long
void write_peer(TlWriter& w, const Peer& p) {
    switch (p.kind) {
        case PeerKind::Channel:
            w.u32(CRC_INPUT_PEER_CHANNEL);
            w.i64(p.id);
            w.i64(p.access_hash);
            break;
        case PeerKind::Chat:
            w.u32(CRC_INPUT_PEER_CHAT);
            w.i64(p.id);
            break;
        case PeerKind::User:
            w.u32(CRC_INPUT_PEER_USER);
            w.i64(p.id);
            w.i64(p.access_hash);
            break;
    }
}

std::string serialize_send(const Peer& to, const std::string& text, int64_t reply_to) {
    // messages.sendMessage#fef48f62 flags:# no_webpage:flags.1?true ...
    //   peer:InputPeer reply_to:flags.0?InputReplyTo message:string random_id:long
    TlWriter w;
    w.u32(CRC_SEND_MESSAGE);
    int32_t flags = 0b10;  // no_webpage
    if (reply_to != 0) flags |= 0b1;
    w.i32(flags);
    write_peer(w, to);
    if (reply_to != 0) {
        w.u32(CRC_INPUT_REPLY_TO);
        w.i32(0);  // flags: bare reply_to_msg_id
        w.i32(static_cast<int32_t>(reply_to));
    }
    w.str(text);
    w.i64(random64());
    return w.out();
}

std::string serialize_edit(const Peer& to, int64_t message_id, const std::string& text) {
    // messages.editMessage#b106e66c flags:# no_webpage:flags.1 peer
    //   id:int message:flags.11?string ...
    TlWriter w;
    w.u32(CRC_EDIT_MESSAGE);
    w.i32((0b1u << 11) | 0b10u);
    write_peer(w, to);
    w.i32(static_cast<int32_t>(message_id));
    w.str(text);
    return w.out();
}

void collect_peers(const std::string& resp, std::vector<Peer>* out) {
    // Layer 228+ dialogs carry bare Peer{peerChannel|peerUser|peerChat, id} —
    // access hashes are no longer required by the server for input peers.
    // Scan aligned 12-byte peer records; ids > 0 filter most false positives.
    for (size_t p = 0; p + 12 <= resp.size(); p += 4) {
        uint32_t m;
        std::memcpy(&m, resp.data() + p, 4);
        Peer pe;
        if (m == CRC_PEER_CHANNEL) pe.kind = PeerKind::Channel;
        else if (m == CRC_PEER_USER) pe.kind = PeerKind::User;
        else continue;
        TlReader rr(resp, p);
        rr.u32();
        pe.id = rr.i64();
        // heuristic validity: channel ids are 1..2^52, never absurd
        if (rr.ok() && pe.id > 0 && pe.id < (1LL << 52)) out->push_back(pe);
    }
}

}  // namespace

// ---------------------------------------------------------------- peer cache

void Client::cache_peer(const Peer& p) {
    std::lock_guard<std::mutex> g(send_mu_);
    if (p.id != 0) peers_[p.marked()] = p;
}

bool Client::peer_for(int64_t marked_chat_id, Peer* out, std::string* err) {
    {
        std::lock_guard<std::mutex> g(send_mu_);
        auto it = peers_.find(marked_chat_id);
        if (it != peers_.end()) {
            *out = it->second;
            return true;
        }
    }
    Peer p = Peer::from_marked(marked_chat_id);
    if (p.kind == PeerKind::Channel) {
        int64_t chan = 0, ah = 0;
        if (!resolve_channel(marked_chat_id, &chan, &ah, err)) return false;
        p.id = chan;
        p.access_hash = ah;
        cache_peer(p);
        *out = p;
        return true;
    }
    if (p.kind == PeerKind::Chat) {
        cache_peer(p);
        *out = p;
        return true;
    }
    // Unknown user: still attempt with zero access hash (server resolves
    // modern ids); callers see RPC errors if the account lacks the peer.
    cache_peer(p);
    *out = p;
    return true;
}

// ------------------------------------------------------------------- dialogs

bool Client::refresh_dialog_peers(int limit, std::string* err) {
    TlWriter w;
    // messages.getDialogs#a0f4cb4f flags:# offset_date:int offset_id:int
    //   offset_peer:InputPeer limit:int   (no flags set -> 4 plain fields after)
    w.u32(CRC_GET_DIALOGS);
    w.i32(0);  // flags
    w.i32(0);  // offset_date
    w.i32(0);  // offset_id
    w.u32(CRC_INPUT_PEER_EMPTY);
    w.i32(limit);
    w.i64(0);  // hash (layer 228+)
    auto resp = call_rpc(w.out(), err, true);
    if (resp.empty()) return false;
    if (getenv("CG_DEBUG")) {
        std::fprintf(stderr, "resp len=%zu magic=%08x %08x %08x\n", resp.size(),
                     resp.size() > 0 ? *(uint32_t*)resp.data() : 0,
                     resp.size() > 4 ? *(uint32_t*)(resp.data() + 4) : 0,
                     resp.size() > 8 ? *(uint32_t*)(resp.data() + 8) : 0);
        std::string dumpfile = getenv("CG_DUMP") ? getenv("CG_DUMP") : "";
        if (!dumpfile.empty()) {
            FILE* f = fopen(dumpfile.c_str(), "wb");
            if (f) { fwrite(resp.data(), 1, resp.size(), f); fclose(f); }
        }
    }
    std::vector<Peer> peers;
    collect_peers(resp, &peers);
    if (getenv("CG_DEBUG")) std::fprintf(stderr, "peers found=%zu\n", peers.size());
    size_t before = 0;
    {
        std::lock_guard<std::mutex> g(send_mu_);
        before = peers_.size();
    }
    for (auto& p : peers) cache_peer(p);
    // The dialogs payload also carries users/chats sections with real access
    // hashes; cache those so the peer cache stays useful for exports.
    {
        std::string derr;
        Val root = decode_tl(resp, &derr);
        if (derr.empty()) {
            for (const Val& c : root.list("chats")) cache_peer_from_node(c);
            for (const Val& u : root.list("users")) cache_peer_from_node(u);
        }
    }
    {
        std::lock_guard<std::mutex> g(send_mu_);
        return peers_.size() >= before;
    }
}

std::vector<DialogInfo> Client::get_dialogs(int limit, std::string* err) {
    std::vector<DialogInfo> out;
    if (!refresh_dialog_peers(limit, err)) return out;
    std::lock_guard<std::mutex> g(send_mu_);
    for (auto& [k, p] : peers_) {
        DialogInfo d;
        d.peer = p;
        out.push_back(d);
    }
    return out;
}

// -------------------------------------------------------------- message ops

int64_t Client::send_message_text(const Peer& to, const std::string& text, int64_t reply_to_msg_id,
                                  std::string* err) {
    auto resp = call_rpc(serialize_send(to, text, reply_to_msg_id), err, true);
    if (resp.empty()) return -1;
    for (size_t p = 0; p + 12 <= resp.size(); p += 4) {
        uint32_t m;
        std::memcpy(&m, resp.data() + p, 4);
        if (m == CRC_UPDATE_MESSAGE_ID) {  // updateMessageID{id:int random_id:long}
            TlReader rr(resp, p);
            rr.u32();
            int64_t id = rr.i32();
            if (rr.ok()) return id;
        }
    }
    return 0;  // sent; id unknown
}

bool Client::edit_message_text(const Peer& to, int64_t message_id, const std::string& text, std::string* err) {
    auto resp = call_rpc(serialize_edit(to, message_id, text), err, true);
    if (resp.empty()) return false;
    cache_peer(to);
    return true;
}

bool Client::delete_messages(const Peer& /*to*/, const std::vector<int64_t>& ids, bool revoke, std::string* err) {
    TlWriter w;
    // messages.deleteMessages#e58e95d2 flags:# revoke:flags.0?True id:Vector<int>
    w.u32(CRC_DELETE_MESSAGES);
    w.i32(revoke ? 1 : 0);
    w.vec_header(static_cast<uint32_t>(ids.size()));
    for (auto id : ids) w.i32(static_cast<int32_t>(id));
    return !call_rpc(w.out(), err, true).empty();
}

bool Client::read_history(const Peer& to, int64_t max_id, std::string* err) {
    // messages.readHistory#e306d3a peer:InputPeer max_id:int
    TlWriter w;
    w.u32(CRC_READ_HISTORY);
    write_peer(w, to);
    w.i32(static_cast<int32_t>(max_id));
    return !call_rpc(w.out(), err, true).empty();
}

bool Client::set_typing(const Peer& to, std::string* err) {
    // messages.setTyping#58943ee2 flags:# peer action:SendMessageAction
    TlWriter w;
    w.u32(CRC_SET_TYPING);
    w.i32(0);
    write_peer(w, to);
    w.u32(CRC_TYPING_ACTION);
    return !call_rpc(w.out(), err, true).empty();
}

std::vector<MessageInfo> Client::get_history(const Peer& to, int limit, std::string* err) {
    // messages.getHistory#4423e6c5 peer offset_id offset_date add_offset limit
    //   max_id min_id hash:long
    TlWriter w;
    w.u32(CRC_GET_HISTORY);
    write_peer(w, to);
    w.i32(0);            // offset_id
    w.i32(0);            // offset_date
    w.i32(0);            // add_offset
    w.i32(limit);        // limit
    w.i32(0);            // max_id
    w.i32(0);            // min_id
    w.i64(0);            // hash
    auto resp = call_rpc(w.out(), err, true);
    std::vector<MessageInfo> out;
    if (resp.empty()) return out;
    // Full `message` decode needs the current flags layout; v1 surfaces ids via
    // updateMessageID-style scan of the updates payloads embedded in results.
    for (size_t p = 0; p + 8 <= resp.size(); p += 4) {
        uint32_t m;
        std::memcpy(&m, resp.data() + p, 4);
        if (m == 0x3811663eu || m == 0x98308d36u || m == 0x101389cu) {
            TlReader rr(resp, p + 4);
            rr.i32();  // flags
            int64_t id = rr.i32();
            if (rr.ok() && id > 0) {
                MessageInfo mi;
                mi.chat_id = to.marked();
                mi.id = id;
                out.push_back(mi);
            }
        }
    }
    return out;
}

}  // namespace cppgram
