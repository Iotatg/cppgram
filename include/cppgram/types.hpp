#pragma once

#include <cstdint>
#include <string>
#include <vector>

// High-level user-client API on top of the raw MTProto transport.

namespace cppgram {

enum class PeerKind : int { User = 0, Chat = 1, Channel = 2 };

struct Peer {
    PeerKind kind{PeerKind::User};
    int64_t id{0};
    int64_t access_hash{0};

    // Marked chat id (Bot-API style): channels/basic chats -> -100xxxx / -xxx,
    // users -> +user_id.
    int64_t marked() const {
        switch (kind) {
            case PeerKind::Channel: return -(100000000000LL + id);
            case PeerKind::Chat: return -id;
            case PeerKind::User: return id;
        }
        return 0;
    }
    static Peer from_marked(int64_t m) {
        Peer p;
        if (m > 0) { p.kind = PeerKind::User; p.id = m; }
        else if (m <= -100000000000LL) { p.kind = PeerKind::Channel; p.id = -(m + 100000000000LL); }
        else { p.kind = PeerKind::Chat; p.id = -m; }
        return p;
    }
};

struct DialogInfo {
    Peer peer;
    std::string title;
    int64_t top_message_id{0};
};

struct MessageInfo {
    int64_t chat_id{0};  // marked form
    int64_t id{0};
    int64_t from_user{0};
    int64_t date{0};
    std::string text;
    bool outgoing{false};
};

}  // namespace cppgram
