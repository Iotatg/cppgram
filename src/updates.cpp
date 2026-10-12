#include "cppgram/client.hpp"
#include "cppgram/tl.hpp"

#include <cstring>

// CRCs verified against tdesktop dev api.tl (telegram layer 228/229).
#define CRC_GET_STATE 0xedd4882au
#define CRC_GET_DIFFERENCE 0x19c2f763u
#define CRC_UPDATES_STATE 0xa56c2a3eu
#define CRC_DIFF_EMPTY 0x5d75a138u
#define CRC_DIFF 0x00f49ca0u     // updates.difference
#define CRC_DIFF_SLICE 0x0a8fb198u  // updates.differenceSlice
#define CRC_UPDATE_NEW_MSG 0x1f2b0afdu
#define CRC_UPDATE_NEW_CHANNEL_MSG 0x62ba04d9u
#define CRC_UPDATES 0x74ae4240u
#define CRC_UPDATES_COMBINED 0x725b04c3u
#define CRC_UPD_SHORT_MSG 0x313bc7f8u
#define CRC_UPDATE_SHORT_SENT 0x9015e101u

namespace cppgram {

namespace {

// Marked (bot-API style) chat id for a decoded peer_id object.
int64_t marked_chat(const Val& pid) {
    if (pid.name == "peerChannel") return -(100000000000LL + pid.num("channel_id"));
    if (pid.name == "peerUser") return pid.num("user_id");
    if (pid.name == "peerChat") return -pid.num("chat_id");
    return 0;
}

// Maps an Update constructor name to the event kind the handler layer knows.
const char* event_kind_for(const std::string& n) {
    if (n == "updateNewMessage" || n == "updateNewChannelMessage" || n == "updateNewScheduledMessage" ||
        n == "updateQuickReplyMessage")
        return "message";
    if (n == "updateEditMessage" || n == "updateEditChannelMessage") return "edited_message";
    if (n == "updateDeleteMessages" || n == "updateDeleteChannelMessages" ||
        n == "updateDeleteScheduledMessages" || n == "updateDeleteQuickReplyMessages")
        return "deleted_messages";
    if (n == "updateBotCallbackQuery" || n == "updateInlineBotCallbackQuery" ||
        n == "updateBusinessBotCallbackQuery")
        return "callback_query";
    if (n == "updateMessageReactions") return "message_reaction";
    if (n == "updateChatParticipant" || n == "updateChannelParticipant" || n == "updateChatParticipantAdd" ||
        n == "updateChatParticipantDelete" || n == "updateChatParticipantAdmin")
        return "chat_member_updated";
    if (n == "updateBotChatInviteRequester") return "chat_join_request";
    if (n == "updateMessagePoll" || n == "updateMessagePollVote") return "poll";
    if (n == "updateUserStatus") return "user_status";
    if (n == "updateUserTyping" || n == "updateChatUserTyping" || n == "updateChannelUserTyping") return "typing";
    if (n == "updateStory" || n == "updateStoryID") return "story";
    if (n == "updateBotInlineQuery") return "inline_query";
    if (n == "updateBotInlineSend") return "chosen_inline_result";
    if (n == "updateBotChatBoost") return "chat_boost";
    if (n == "updateBotMessageReactions") return "message_reaction_count";
    if (n == "updateBotPrecheckoutQuery") return "pre_checkout_query";
    if (n == "updateBotShippingQuery") return "shipping_query";
    if (n == "updateBotPurchasedPaidMedia") return "purchased_paid_media";
    if (n == "updateBotStoppedMessageGeneration") return "stopped_message_generation";
    if (n == "updateBotBusinessConnection") return "business_connection";
    if (n == "updateBotDeleteBusinessMessage") return "deleted_business_messages";
    if (n == "updateBotManagedBot") return "managed_bot";
    if (n == "updateBotSentGuestMessage") return "guest_message";
    return "raw";
}

// Emits one decoded Update as an UpdateEvent (payload = JSON of the update).
void emit_update_event(const Val& u, const std::function<void(const UpdateEvent&)>& fn) {
    const char* kind = event_kind_for(u.name);
    UpdateEvent e;
    e.kind = kind;
    if (std::strcmp(kind, "message") == 0) {
        const Val& m = u.at("message");
        e.chat_id = marked_chat(m.at("peer_id"));
        std::string payload = to_json(m);
        if (!payload.empty() && payload[0] == '{')
            payload.insert(1, "\"chat\":" + std::to_string(e.chat_id) + ",\"id\":" +
                                   std::to_string(m.num("id")) + ",");
        e.payload = std::move(payload);
    } else if (std::strcmp(kind, "deleted_messages") == 0) {
        e.chat_id = u.has("channel_id") ? -(100000000000LL + u.num("channel_id")) : 0;
        e.payload = std::string("{\"messages\":") + to_json(u.at("messages")) + "}";
    } else if (u.has("chat_id") || u.has("channel_id")) {
        e.chat_id = u.has("channel_id") ? -(100000000000LL + u.num("channel_id")) : u.num("chat_id");
        e.payload = to_json(u);
    } else {
        const Val& pid = u.at("peer");
        if (pid.k == Val::OBJ) e.chat_id = marked_chat(pid);
        e.payload = to_json(u);
    }
    fn(e);
}

void scan_message_events(const std::string& blob,
                         const std::function<void(const UpdateEvent&)>& fn) {
    // Primary path: decode the Updates container with the full schema engine and
    // fan out every inner Update (any constructor) to its event kind, so handlers
    // receive complete context — not just ids.
    std::string d_err;
    Val root = decode_tl_at(blob, 0, &d_err);
    if (d_err.empty() && root.k == Val::OBJ) {
        const std::string& n = root.name;
        if (n == "updates" || n == "updatesCombined") {
            for (const auto& u : root.list("updates")) emit_update_event(u, fn);
            return;
        }
        if (n == "updateShort" || n == "updateShortMessage" || n == "updateShortChatMessage" ||
            n == "updateShortSentMessage" || n == "updatesTooLong") {
            emit_update_event(root, fn);
            return;
        }
    }

    // Fallback: anchored byte scan for inline updateNewMessage /
    // updateNewChannelMessage (pre-engine behaviour).
    for (size_t p = 0; p + 16 <= blob.size(); p += 4) {
        uint32_t m;
        std::memcpy(&m, blob.data() + p, 4);
        if (m != CRC_UPDATE_NEW_MSG && m != CRC_UPDATE_NEW_CHANNEL_MSG) continue;
        std::string err2;
        Val msg = decode_tl_at(blob, p + 4, &err2);
        int64_t id = 0, chat = 0;
        if (err2.empty() && (msg.name == "message" || msg.name == "messageService")) {
            id = msg.num("id");
            chat = marked_chat(msg.at("peer_id"));
        } else {
            TlReader rr(blob, p);
            rr.u32();
            rr.u32();  // message ctor
            uint32_t flags = rr.u32();
            if (flags & (1u << 27)) rr.u32();
            id = rr.i32();
            if (!rr.ok() || id <= 0 || id > 0x7fffffff) continue;
        }
        if (id <= 0 || id > 0x7fffffff) continue;
        UpdateEvent e;
        e.kind = "message";
        e.chat_id = chat;
        std::string payload = err2.empty() ? to_json(msg) : std::string("{}");
        if (!payload.empty() && payload[0] == '{') {
            payload.insert(1, "\"chat\":" + std::to_string(chat) + ",\"id\":" + std::to_string(id) + ",");
            e.payload = std::move(payload);
        } else {
            e.payload = "{\"id\":" + std::to_string(id) +
                        ",\"chat\":" + std::to_string(chat) +
                        ",\"channel\":" + std::to_string(m == CRC_UPDATE_NEW_CHANNEL_MSG ? 1 : 0) + "}";
        }
        fn(e);
        p += 12;
    }
}

bool read_state_tail(const std::string& blob, int64_t* pts, int32_t* date, int64_t* qts) {
    // updates.state appears inline in every difference response; pick the LAST
    // well-formed occurrence.
    for (size_t p = 0; p + 20 <= blob.size(); p += 4) {
        uint32_t m;
        std::memcpy(&m, blob.data() + p, 4);
        if (m != CRC_UPDATES_STATE) continue;
        TlReader rr(blob, p);
        rr.u32();
        int64_t npts = rr.i32();
        int64_t ndate = rr.i32();
        int64_t nqts = rr.i32();
        if (rr.ok()) {
            *pts = npts;
            *date = static_cast<int32_t>(ndate);
            *qts = nqts;
        }
    }
    return true;
}

}  // namespace

void Client::emit_message_events(const std::string& body) {
    std::function<void(const UpdateEvent&)> user;
    {
        std::lock_guard<std::mutex> g(updates_mu_);
        user = update_handler_;
    }
    auto emit = [&](const UpdateEvent& ev) {
        dispatch_typed_event(ev);
        if (ev.kind == "message") dispatch_message_event(ev);
        if (user) user(ev);
    };
    scan_message_events(body, emit);
}

void Client::track_update_pts(const std::string& blob) {
    // Adopt pts from pushed updateShortMessage/updateShortSentMessage frames so
    // the difference poller skips events already delivered via push. Other
    // frames (updates/updatesCombined/updateShort) carry no global pts.
    for (size_t p = 0; p + 12 <= blob.size(); p += 4) {
        uint32_t m;
        std::memcpy(&m, blob.data() + p, 4);
        if (m == CRC_UPD_SHORT_MSG) {
            TlReader rr(blob, p);
            rr.u32();
            rr.u32();      // flags
            rr.i32();      // id
            rr.i64();      // user_id
            rr.str();      // message
            int32_t pts = rr.i32();
            int32_t cnt = rr.i32();
            if (rr.ok() && pts > 0 && cnt >= 0 && cnt < 100000 &&
                pts > static_cast<int32_t>(diff_pts_)) {
                diff_pts_ = pts;
            }
        } else if (m == CRC_UPDATE_SHORT_SENT) {
            TlReader rr(blob, p);
            rr.u32();
            rr.u32();      // flags
            rr.i32();      // id
            int32_t pts = rr.i32();
            int32_t cnt = rr.i32();
            if (rr.ok() && pts > 0 && cnt >= 0 && cnt < 100000 &&
                pts > static_cast<int32_t>(diff_pts_)) {
                diff_pts_ = pts;
            }
        }
    }
}

void Client::poll_difference() {
    std::function<void(const UpdateEvent&)> emit;
    {
        std::lock_guard<std::mutex> g(updates_mu_);
        emit = update_handler_;
    }
    std::string err;

    if (diff_pts_ == 0) {
        TlWriter w;
        w.u32(CRC_GET_STATE);
        auto resp = call_rpc(w.out(), &err, true);
        if (resp.empty()) return;
        read_state_tail(resp, &diff_pts_, &diff_date_, &diff_qts_);
        if (diff_pts_ == 0) return;
        return;  // primed; next call asks for actual deltas
    }

    // Loop across slices until empty.
    for (int guard = 0; guard < 8; guard++) {
        TlWriter w;
        w.u32(CRC_GET_DIFFERENCE);
        w.i32(0);  // flags
        w.i32(static_cast<int32_t>(diff_pts_));
        w.i32(diff_date_);
        w.i32(static_cast<int32_t>(diff_qts_));
        auto resp = call_rpc(w.out(), &err, true);
        if (resp.empty()) return;

        uint32_t top;
        std::memcpy(&top, resp.data(), 4);
        if (emit) scan_message_events(resp, emit);
        int64_t pts = diff_pts_, qts = diff_qts_;
        int32_t date = diff_date_;
        read_state_tail(resp, &pts, &date, &qts);
        diff_pts_ = pts;
        diff_date_ = date;
        diff_qts_ = qts;
        if (top == CRC_DIFF_EMPTY) return;
    }
}

}  // namespace cppgram
