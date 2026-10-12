#include "cppgram/client.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace cppgram {

namespace {

int64_t marked_from_peer(const Val& pid) {
    if (pid.name == "peerChannel") return -(100000000000LL + pid.num("channel_id"));
    if (pid.name == "peerUser") return pid.num("user_id");
    if (pid.name == "peerChat") return -pid.num("chat_id");
    return 0;
}

std::string status_name(const Val& st) {
    if (st.name == "userStatusOnline") return "online";
    if (st.name == "userStatusOffline") return "offline";
    if (st.name == "userStatusRecently") return "recently";
    if (st.name == "userStatusLastWeek") return "last_week";
    if (st.name == "userStatusLastMonth") return "last_month";
    if (st.name.empty()) return "unknown";
    return st.name;
}

std::string action_name(const Val& a) {
    if (a.name == "sendMessageTypingAction") return "typing";
    if (a.name == "sendMessageUploadPhotoAction") return "upload_photo";
    if (a.name == "sendMessageRecordVideoAction") return "record_video";
    if (a.name == "sendMessageUploadVideoAction") return "upload_video";
    if (a.name == "sendMessageRecordAudioAction") return "record_audio";
    if (a.name == "sendMessageUploadAudioAction") return "upload_audio";
    if (a.name == "sendMessageUploadDocumentAction") return "upload_document";
    if (a.name == "sendMessageGeoLocationAction") return "find_location";
    if (a.name == "sendMessageChooseStickerAction") return "choose_sticker";
    if (a.name.empty()) return "typing";
    return a.name;
}

void fanout(const std::vector<std::pair<int, std::function<void(const Val&)>>>& hs, const Val& v) {
    for (auto& p : hs)
        if (p.second) p.second(v);
}

}  // namespace

int Client::register_typed(const std::string& kind, std::function<void(const Val&)> fn) {
    std::lock_guard<std::mutex> g(typed_mu_);
    int h = ++msg_handle_seq_;
    typed_handlers_[kind].emplace_back(h, std::move(fn));
    return h;
}

void Client::off_handler(const std::string& kind, int handle) {
    std::lock_guard<std::mutex> g(typed_mu_);
    auto it = typed_handlers_.find(kind);
    if (it == typed_handlers_.end()) return;
    it->second.erase(std::remove_if(it->second.begin(), it->second.end(),
                                    [&](auto& p) { return p.first == handle; }),
                     it->second.end());
}

bool Client::has_handler(const std::string& kind) const {
    std::lock_guard<std::mutex> g(typed_mu_);
    auto it = typed_handlers_.find(kind);
    return it != typed_handlers_.end() && !it->second.empty();
}

int Client::on_edited_message(MessageFilter f, EditedMessageHandler fn) {
    return register_typed("edited_message", [f, fn](const Val& u) {
        const Val& m = u.has("message") ? u.at("message") : u;
        EditedMessageEvent e;
        e.id = m.num("id");
        e.chat_id = marked_from_peer(m.at("peer_id"));
        e.text = m.text("message");
        e.date = m.num("date");
        e.raw = m;
        Message mm;
        mm.id = e.id;
        mm.chat_id = e.chat_id;
        mm.text = e.text;
        mm.date = e.date;
        mm.incoming = true;
        if (f.matches(mm)) fn(e);
    });
}

int Client::on_deleted_messages(DeletedMessagesHandler fn) {
    return register_typed("deleted_messages", [fn](const Val& u) {
        DeletedMessagesEvent e;
        e.raw = u;
        if (u.has("channel_id")) e.chat_id = -(100000000000LL + u.num("channel_id"));
        for (auto& x : u.list("messages")) e.message_ids.push_back(x.as_int());
        fn(e);
    });
}

int Client::on_callback_query(CallbackQueryHandler fn) {
    return register_typed("callback_query", [fn](const Val& u) {
        CallbackQuery q;
        q.query_id = u.num("query_id");
        q.user_id = u.num("user_id");
        q.msg_id = u.num("msg_id");
        q.chat_instance = u.num("chat_instance");
        q.data = u.text("data");
        q.game_short_name = u.text("game_short_name");
        q.chat_id = marked_from_peer(u.at("peer"));
        q.raw = u;
        fn(q);
    });
}

int Client::on_message_reaction(MessageReactionHandler fn) {
    return register_typed("message_reaction", [fn](const Val& u) {
        MessageReactionEvent e;
        e.chat_id = marked_from_peer(u.at("peer"));
        e.msg_id = u.num("msg_id");
        e.reactions_blob = u.at("reactions");
        e.raw = u;
        for (auto& r : e.reactions_blob.list("results")) {
            const Val& reaction = r.at("reaction");
            if (reaction.name == "reactionEmoji") e.reactions.push_back(reaction.text("emoticon"));
            else if (reaction.name == "reactionCustomEmoji")
                e.reactions.push_back("custom:" + std::to_string(reaction.num("document_id")));
            e.counts.push_back(static_cast<int32_t>(r.num("count")));
        }
        fn(e);
    });
}

int Client::on_chat_member_updated(ChatMemberHandler fn) {
    return register_typed("chat_member_updated", [fn](const Val& u) {
        ChatMemberUpdated e;
        e.date = u.num("date");
        e.actor_id = u.num("actor_id");
        e.user_id = u.num("user_id");
        if (u.has("channel_id")) e.chat_id = -(100000000000LL + u.num("channel_id"));
        else e.chat_id = -u.num("chat_id");
        e.prev_participant = u.at("prev_participant");
        e.new_participant = u.at("new_participant");
        e.prev_status = e.prev_participant.name;
        e.new_status = e.new_participant.name;
        if (e.new_status.find("Left") != std::string::npos || e.new_status.find("Banned") != std::string::npos)
            e.kind = "leave";
        else if (e.prev_status.empty() || e.prev_status.find("Left") != std::string::npos)
            e.kind = "join";
        else if (e.new_status.find("Admin") != std::string::npos)
            e.kind = "promote";
        else
            e.kind = "rights";
        e.raw = u;
        fn(e);
    });
}

int Client::on_chat_join_request(ChatJoinRequestHandler fn) {
    return register_typed("chat_join_request", [fn](const Val& u) {
        ChatJoinRequest r;
        r.chat_id = marked_from_peer(u.at("peer"));
        r.user_id = u.num("user_id");
        r.date = u.num("date");
        r.about = u.text("about");
        r.invite = u.at("invite");
        r.qts = u.num("qts");
        r.raw = u;
        fn(r);
    });
}

int Client::on_poll(PollHandler fn) {
    return register_typed("poll", [fn](const Val& u) {
        PollUpdate p;
        p.poll_id = u.num("poll_id");
        p.msg_id = u.num("msg_id");
        p.chat_id = marked_from_peer(u.at("peer"));
        p.poll = u.at("poll");
        p.results = u.at("results");
        p.voted = u.name == "updateMessagePollVote";
        for (auto& o : u.list("options")) p.options.push_back(o.s);
        p.raw = u;
        fn(p);
    });
}

int Client::on_user_status(UserStatusHandler fn) {
    return register_typed("user_status", [fn](const Val& u) {
        UserStatusEvent e;
        e.user_id = u.num("user_id");
        e.status = status_name(u.at("status"));
        e.was_online = u.at("status").num("was_online");
        e.raw = u;
        fn(e);
    });
}

int Client::on_typing(TypingHandler fn) {
    return register_typed("typing", [fn](const Val& u) {
        TypingEvent e;
        e.user_id = u.num("user_id");
        if (u.has("chat_id")) e.chat_id = -u.num("chat_id");
        else if (u.has("channel_id")) e.chat_id = -(100000000000LL + u.num("channel_id"));
        else e.chat_id = e.user_id;
        e.action = action_name(u.at("action"));
        e.raw = u;
        fn(e);
    });
}

int Client::on_story(StoryHandler fn) {
    return register_typed("story", [fn](const Val& u) {
        StoryUpdate s;
        s.chat_id = marked_from_peer(u.at("peer"));
        s.story = u.at("story");
        s.story_id = s.story.num("id");
        if (!s.story_id) s.story_id = u.num("id");
        s.raw = u;
        fn(s);
    });
}

int Client::on_inline_query(InlineQueryHandler fn) {
    return register_typed("inline_query", [fn](const Val& u) {
        InlineQueryEvent e;
        e.query_id = u.num("query_id");
        e.user_id = u.num("user_id");
        e.query = u.text("query");
        e.offset = u.text("offset");
        e.raw = u;
        fn(e);
    });
}

int Client::on_chosen_inline_result(ChosenInlineResultHandler fn) {
    return register_typed("chosen_inline_result", [fn](const Val& u) {
        ChosenInlineResultEvent e;
        e.user_id = u.num("user_id");
        e.query = u.text("query");
        e.result_id = u.text("id");
        e.msg_id = u.at("msg_id").num("id");
        e.raw = u;
        fn(e);
    });
}

int Client::on_raw_update(RawUpdateHandler fn) {
    return register_typed("raw_update", [fn](const Val& u) {
        RawUpdateEvent e;
        e.update = u;
        fn(e);
    });
}

int Client::on_error(ErrorHandler fn) {
    return register_typed("error", [fn](const Val& u) {
        fn(static_cast<int>(u.num("code")), u.text("message"));
    });
}

int Client::on_connect(VoidHandler fn) {
    return register_typed("connect", [fn](const Val&) { fn(); });
}

int Client::on_disconnect(VoidHandler fn) {
    return register_typed("disconnect", [fn](const Val&) { fn(); });
}

int Client::on_chat_boost(RawUpdateHandler fn) {
    return register_typed("chat_boost", [fn](const Val& u) {
        RawUpdateEvent e;
        e.update = u;
        fn(e);
    });
}

int Client::on_message_reaction_count(MessageReactionCountHandler fn) {
    return register_typed("message_reaction_count", [fn](const Val& u) {
        MessageReactionCountUpdate e;
        if (u.has("peer")) e.chat_id = marked_from_peer(u.at("peer"));
        else if (u.has("chat_id")) e.chat_id = u.num("chat_id");
        e.message_id = u.num("msg_id");
        e.date = u.num("date");
        e.raw = u;
        for (auto& r : u.at("reactions").list("results")) {
            const Val& reaction = r.at("reaction");
            if (reaction.name == "reactionEmoji") e.reactions.push_back(reaction.text("emoticon"));
            else if (reaction.name == "reactionCustomEmoji")
                e.reactions.push_back("custom:" + std::to_string(reaction.num("document_id")));
            e.counts.push_back(static_cast<int32_t>(r.num("count")));
        }
        fn(e);
    });
}

int Client::on_pre_checkout_query(PreCheckoutQueryHandler fn) {
    return register_typed("pre_checkout_query", [fn](const Val& u) {
        PreCheckoutQuery e;
        e.query_id = u.num("query_id");
        e.user_id = u.num("user_id");
        e.currency = u.text("currency");
        e.total_amount = u.num("total_amount");
        e.invoice_payload = u.text("payload");
        e.shipping_option_id = u.text("shipping_option_id");
        if (u.has("info")) {
            const Val& info = u.at("info");
            e.order_name = info.text("name");
            e.order_email = info.text("email_address");
            e.order_phone = info.text("phone_number");
            if (info.has("shipping_address")) {
                const Val& sa = info.at("shipping_address");
                e.shipping_city = sa.text("city");
                e.shipping_country_code = sa.text("country_iso2");
                e.shipping_post_code = sa.text("post_code");
                e.shipping_state = sa.text("state");
                e.shipping_street_line1 = sa.text("street_line1");
                e.shipping_street_line2 = sa.text("street_line2");
            }
        }
        e.raw = u;
        fn(e);
    });
}

int Client::on_shipping_query(ShippingQueryHandler fn) {
    return register_typed("shipping_query", [fn](const Val& u) {
        ShippingQuery e;
        e.query_id = u.num("query_id");
        e.user_id = u.num("user_id");
        e.invoice_payload = u.text("payload");
        if (u.has("shipping_address")) {
            const Val& sa = u.at("shipping_address");
            e.shipping_city = sa.text("city");
            e.shipping_country_code = sa.text("country_iso2");
            e.shipping_post_code = sa.text("post_code");
            e.shipping_state = sa.text("state");
            e.shipping_street_line1 = sa.text("street_line1");
            e.shipping_street_line2 = sa.text("street_line2");
        }
        e.raw = u;
        fn(e);
    });
}

int Client::on_purchased_paid_media(PurchasedPaidMediaHandler fn) {
    return register_typed("purchased_paid_media", [fn](const Val& u) {
        PurchasedPaidMediaEvent e;
        e.query_id = u.num("query_id");
        e.user_id = u.num("user_id");
        e.payload = u.text("payload");
        e.raw = u;
        fn(e);
    });
}

int Client::on_business_connection(BusinessConnectionHandler fn) {
    return register_typed("business_connection", [fn](const Val& u) {
        BusinessConnectionEvent e;
        // updateBotBusinessConnection nests the connection under `connection`.
        const Val& c = u.has("connection") ? u.at("connection") : u;
        e.connection_id = u.text("connection_id");
        e.user_id = c.num("user_id");
        e.dc_id = (int)c.num("dc_id");
        e.date = c.num("date");
        e.is_enabled = c.num("can_reply") != 0;
        if (c.has("rights")) e.can_reply = c.at("rights").num("reply_to_incoming") != 0;
        e.raw = u;
        fn(e);
    });
}

int Client::on_deleted_business_messages(RawUpdateHandler fn) {
    return register_typed("deleted_business_messages", [fn](const Val& u) {
        RawUpdateEvent e;
        e.update = u;
        fn(e);
    });
}

int Client::on_edited_business_message(RawUpdateHandler fn) {
    return register_typed("edited_message", [fn](const Val& u) {
        RawUpdateEvent e;
        e.update = u;
        fn(e);
    });
}

int Client::on_managed_bot(ManagedBotHandler fn) {
    return register_typed("managed_bot", [fn](const Val& u) {
        ManagedBotEvent e;
        e.bot_id = u.num("bot_id");
        e.user_id = u.has("user") ? u.at("user").num("id") : 0;
        e.raw = u;
        fn(e);
    });
}

int Client::on_guest_message(MessageHandler fn) {
    return register_typed("guest_message", [fn](const Val& u) {
        // updateBotSentGuestMessage wraps the message the mini-app guest sent.
        const Val& m = u.has("message") ? u.at("message") : u;
        Message mm;
        mm.chat_id = marked_from_peer(m.at("peer_id"));
        mm.id = m.num("id");
        mm.from_id = m.has("from_id") ? m.at("from_id").num("user_id") : 0;
        mm.date = m.num("date");
        mm.text = m.text("message");
        mm.incoming = true;
        mm.raw = m;
        fn(mm);
    });
}

int Client::on_stopped_message_generation(StoppedMessageGenerationHandler fn) {
    return register_typed("stopped_message_generation", [fn](const Val& u) {
        StoppedMessageGenerationEvent e;
        if (u.has("peer")) e.chat_id = marked_from_peer(u.at("peer"));
        else if (u.has("chat_id")) e.chat_id = u.num("chat_id");
        e.message_thread_id = u.num("top_msg_id");
        e.draft_id = u.text("draft_id");
        e.raw = u;
        fn(e);
    });
}

int Client::on_business_message(RawUpdateHandler fn) {
    return register_typed("message", [fn](const Val& u) {
        RawUpdateEvent e;
        e.update = u;
        fn(e);
    });
}

int Client::on_start(VoidHandler fn) {
    return register_typed("connect", [fn](const Val&) { fn(); });
}

int Client::on_stop(VoidHandler fn) {
    return register_typed("disconnect", [fn](const Val&) { fn(); });
}

void Client::fire_lifecycle(const std::string& kind) {
    std::vector<std::pair<int, std::function<void(const Val&)>>> hs;
    {
        std::lock_guard<std::mutex> g(typed_mu_);
        auto it = typed_handlers_.find(kind);
        if (it != typed_handlers_.end()) hs = it->second;
    }
    fanout(hs, Val{});
}

void Client::dispatch_typed_event(const UpdateEvent& ev) {
    std::string err;
    Val u = parse_json(ev.payload, &err);
    if (err.empty() && !ev.kind.empty()) {
        std::vector<std::pair<int, std::function<void(const Val&)>>> hs, raw;
        {
            std::lock_guard<std::mutex> g(typed_mu_);
            auto it = typed_handlers_.find(ev.kind);
            if (it != typed_handlers_.end()) hs = it->second;
            auto it2 = typed_handlers_.find("raw_update");
            if (it2 != typed_handlers_.end()) raw = it2->second;
        }
        fanout(hs, u);
        fanout(raw, u);
    }
}

void Client::dispatch_update_obj(const Val& updates_root) {
    if (updates_root.name == "updates" || updates_root.name == "updatesCombined") {
        for (const auto& u : updates_root.list("updates")) {
            UpdateEvent e;
            e.kind = u.name;
            e.payload = to_json(u);
            dispatch_typed_event(e);
        }
        return;
    }
    UpdateEvent e;
    e.kind = updates_root.name;
    e.payload = to_json(updates_root);
    dispatch_typed_event(e);
}

}  // namespace cppgram
