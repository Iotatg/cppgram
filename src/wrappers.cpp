#include "cppgram/client.hpp"

#include <cstdio>
#include <cstdlib>
#include <random>
#include <utility>

namespace cppgram {

namespace {

std::string pj(const Peer& p) {
    char buf[180];
    switch (p.kind) {
        case PeerKind::Chat:
            std::snprintf(buf, sizeof(buf), "{\"_ctor\":\"inputPeerChat\",\"chat_id\":%lld}",
                          (long long)p.id);
            return buf;
        case PeerKind::User:
            std::snprintf(buf, sizeof(buf),
                          "{\"_ctor\":\"inputPeerUser\",\"user_id\":%lld,\"access_hash\":%lld}",
                          (long long)p.id, (long long)p.access_hash);
            return buf;
        case PeerKind::Channel:
            std::snprintf(buf, sizeof(buf),
                          "{\"_ctor\":\"inputPeerChannel\",\"channel_id\":%lld,\"access_hash\":%lld}",
                          (long long)p.id, (long long)p.access_hash);
            return buf;
    }
    return "{\"_ctor\":\"inputPeerEmpty\"}";
}

std::string ich(const Peer& p) {
    char buf[160];
    std::snprintf(buf, sizeof(buf),
                  "{\"_ctor\":\"inputChannel\",\"channel_id\":%lld,\"access_hash\":%lld}",
                  (long long)p.id, (long long)p.access_hash);
    return buf;
}

std::string iuser(long long id) {
    return "{\"_ctor\":\"inputUser\",\"user_id\":" + std::to_string(id) + ",\"access_hash\":0}";
}

std::string iuser_peer(long long id) {
    return "{\"_ctor\":\"inputPeerUser\",\"user_id\":" + std::to_string(id) + ",\"access_hash\":0}";
}

// Telegram takes coordinates as degrees with 8 decimals; a plain double is
// formatted through a std::string so an out-of-range value cannot be truncated
// into malformed JSON.
std::string fmt_coord(double v) {
    char b[512];
    std::snprintf(b, sizeof(b), "%.8f", v);
    return b;
}

std::string esc(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += (char)c;
        } else if (c == '\n')
            out += "\\n";
        else if (c == '\r')
            out += "\\r";
        else if (c < 0x20) {
            char b[8];
            std::snprintf(b, sizeof(b), "\\u%04x", c);
            out += b;
        } else
            out += (char)c;
    }
    out += '"';
    return out;
}

long long rnd() {
    static std::mt19937_64 g{std::random_device{}()};
    return static_cast<long long>(g());
}

Val send_media(Client* c, const Peer& p, const std::string& media, std::string* err) {
    std::string a = "{\"peer\":" + pj(p) + ",\"media\":" + media +
                    ",\"message\":\"\",\"random_id\":" + std::to_string(rnd()) + "}";
    return c->api_json("messages.sendMedia", a, err);
}

std::string ints(const std::vector<long long>& ids) {
    std::string o = "[";
    for (size_t i = 0; i < ids.size(); i++) {
        if (i) o += ',';
        o += std::to_string(ids[i]);
    }
    o += ']';
    return o;
}

}  // namespace

Val Client::set_chat_title(const Peer& p, const std::string& title, std::string* err) {
    if (p.kind == PeerKind::Channel)
        return api_json("channels.editTitle",
                        "{\"channel\":" + ich(p) + ",\"title\":" + esc(title) + "}", err);
    return api_json("messages.editChatTitle",
                    "{\"chat_id\":" + std::to_string(p.id) + ",\"title\":" + esc(title) + "}", err);
}

Val Client::set_chat_description(const Peer& p, const std::string& about, std::string* err) {
    return api_json("messages.editChatAbout", "{\"peer\":" + pj(p) + ",\"about\":" + esc(about) + "}",
                    err);
}

Val Client::set_chat_username(const Peer& p, const std::string& username, std::string* err) {
    return api_json("channels.updateUsername",
                    "{\"channel\":" + ich(p) + ",\"username\":" + esc(username) + "}", err);
}

Val Client::set_slow_mode(const Peer& p, int seconds, std::string* err) {
    return api_json("channels.toggleSlowMode",
                    "{\"channel\":" + ich(p) + ",\"seconds\":" + std::to_string(seconds) + "}", err);
}

Val Client::set_chat_ttl(const Peer& p, int period, std::string* err) {
    return api_json("messages.setHistoryTTL",
                    "{\"peer\":" + pj(p) + ",\"period\":" + std::to_string(period) + "}", err);
}

Val Client::set_chat_protected_content(const Peer& p, bool enabled, std::string* err) {
    return api_json("messages.toggleNoForwards",
                    std::string("{\"peer\":") + pj(p) + ",\"enabled\":" + (enabled ? "true" : "false") + "}",
                    err);
}

Val Client::archive_chats(const std::vector<Peer>& chats, bool archive, std::string* err) {
    std::string arr = "[";
    for (size_t i = 0; i < chats.size(); i++) {
        if (i) arr += ',';
        arr += "{\"_ctor\":\"inputFolderPeer\",\"peer\":" + pj(chats[i]) +
               ",\"folder_id\":" + std::to_string(archive ? 1 : 0) + "}";
    }
    arr += ']';
    return api_json("folders.editPeerFolders", "{\"folder_peers\":" + arr + "}", err);
}

Val Client::mark_chat_unread(const Peer& p, bool unread, std::string* err) {
    std::string a = "{\"peer\":{\"_ctor\":\"inputDialogPeer\",\"peer\":" + pj(p) + "}" +
                    (unread ? ",\"unread\":true" : "") + "}";
    return api_json("messages.markDialogUnread", a, err);
}

Val Client::create_channel(const std::string& title, const std::string& about, bool megagroup,
                           std::string* err) {
    std::string a = "{\"title\":" + esc(title) + ",\"about\":" + esc(about);
    a += megagroup ? ",\"megagroup\":true}" : ",\"broadcast\":true}";
    return api_json("channels.createChannel", a, err);
}

Val Client::create_group(const std::string& title, const std::vector<long long>& user_ids,
                         std::string* err) {
    std::string users = "[";
    for (size_t i = 0; i < user_ids.size(); i++) {
        if (i) users += ',';
        users += iuser(user_ids[i]);
    }
    users += ']';
    return api_json("messages.createChat",
                    "{\"users\":" + users + ",\"title\":" + esc(title) + "}", err);
}

Val Client::delete_channel(const Peer& p, std::string* err) {
    return api_json("channels.deleteChannel", "{\"channel\":" + ich(p) + "}", err);
}

Val Client::get_chat_member(const Peer& chat, long long user_id, std::string* err) {
    if (chat.kind == PeerKind::Channel)
        return api_json("channels.getParticipant",
                        "{\"channel\":" + ich(chat) + ",\"participant\":" + iuser_peer(user_id) + "}",
                        err);
    return api_json("messages.getFullChat", "{\"chat_id\":" + std::to_string(chat.id) + "}", err);
}

Val Client::get_chat_members_count(const Peer& p, std::string* err) {
    Val r = get_chat(p, true, err);
    if (!err->empty()) return {};
    long long n = r.dig("full_chat.participants_count").as_int();
    if (!n) n = r.dig("chats.0.participants_count").as_int();
    return Val::i64(n);
}

Val Client::get_dialogs_count(std::string* err) {
    Val r = api_json("messages.getDialogs", "{\"limit\":1}", err);
    if (!err->empty()) return {};
    return Val::i64(r.num("count") ? r.num("count") : (long long)r.list("dialogs").size());
}

Val Client::restrict_chat_member(const Peer& chat, long long user_id, bool view_messages,
                                 std::string* err) {
    std::string rights = "{\"_ctor\":\"chatBannedRights\",\"until_date\":0";
    if (view_messages) rights += ",\"view_messages\":true";
    rights += "}";
    return api_json("channels.editBanned",
                    "{\"channel\":" + ich(chat) + ",\"participant\":" + iuser_peer(user_id) +
                        ",\"banned_rights\":" + rights + "}",
                    err);
}

Val Client::unpin_all_chat_messages(const Peer& p, std::string* err) {
    return api_json("messages.unpinAllMessages", "{\"peer\":" + pj(p) + "}", err);
}

Val Client::set_administrator_title(const Peer& chat, long long user_id, const std::string& title,
                                    std::string* err) {
    std::string rights =
        "{\"_ctor\":\"chatAdminRights\",\"change_info\":true,\"delete_messages\":true,"
        "\"ban_users\":true,\"invite_users\":true,\"pin_messages\":true,\"manage_topics\":true}";
    return api_json("channels.editAdmin",
                    "{\"channel\":" + ich(chat) + ",\"user_id\":" + iuser(user_id) +
                        ",\"admin_rights\":" + rights + ",\"rank\":" + esc(title) + "}",
                    err);
}

Val Client::get_chat_event_log(const Peer& p, int limit, std::string* err) {
    char a[256];
    std::snprintf(a, sizeof(a),
                  "{\"channel\":%s,\"q\":\"\",\"max_id\":0,\"min_id\":0,\"limit\":%d}",
                  ich(p).c_str(), limit);
    return api_json("channels.getAdminLog", a, err);
}

Val Client::create_forum_topic(const Peer& p, const std::string& title, std::string* err) {
    return api_json("messages.createForumTopic",
                    "{\"peer\":" + pj(p) + ",\"title\":" + esc(title) +
                        ",\"random_id\":" + std::to_string(rnd()) + "}",
                    err);
}

Val Client::edit_forum_topic(const Peer& p, int topic_id, const std::string& title, std::string* err) {
    return api_json("messages.editForumTopic",
                    "{\"peer\":" + pj(p) + ",\"topic_id\":" + std::to_string(topic_id) +
                        ",\"title\":" + esc(title) + "}",
                    err);
}

Val Client::close_forum_topic(const Peer& p, int topic_id, bool closed, std::string* err) {
    return api_json("messages.editForumTopic",
                    "{\"peer\":" + pj(p) + ",\"topic_id\":" + std::to_string(topic_id) +
                        ",\"closed\":" + std::string(closed ? "true" : "false") + "}",
                    err);
}

Val Client::delete_forum_topic(const Peer& p, int topic_id, std::string* err) {
    return api_json("messages.deleteTopicHistory",
                    "{\"peer\":" + pj(p) + ",\"top_msg_id\":" + std::to_string(topic_id) + "}", err);
}

Val Client::get_forum_topics(const Peer& p, int limit, std::string* err) {
    char a[256];
    std::snprintf(a, sizeof(a),
                  "{\"peer\":%s,\"offset_date\":0,\"offset_id\":0,\"offset_topic\":0,\"limit\":%d}",
                  pj(p).c_str(), limit);
    return api_json("messages.getForumTopics", a, err);
}

Val Client::transfer_chat_ownership(const Peer& chat, long long user_id, const std::string& password,
                                    std::string* err) {
    Val pw = api_json("account.getPassword", "{}", err);
    if (!err->empty()) return {};
    std::string srp = srp_password_check(pw, password, err);
    if (!err->empty()) return {};
    if (schema::ctor_by_name("channels.editCreator") < 0) {
        // channels.editCreator is absent from the bundled TL schema, so the call
        // could never be encoded; say so instead of the generic unknown method.
        if (err) *err = "api: channels.editCreator is not in the bundled schema layer";
        return {};
    }
    return api_json("channels.editCreator",
                    "{\"channel\":" + ich(chat) + ",\"user_id\":" + iuser(user_id) +
                        ",\"password\":" + srp + "}",
                    err);
}

Val Client::send_poll(const Peer& p, const std::string& question, const std::vector<std::string>& options,
                      bool quiz, int correct_option, std::string* err) {
    if (options.empty()) {
        if (err) *err = "send_poll: at least one option is required";
        return Val();
    }
    std::string answers = "[";
    for (size_t i = 0; i < options.size(); i++) {
        if (i) answers += ',';
        answers += "{\"_ctor\":\"pollAnswer\",\"text\":{\"_ctor\":\"textWithEntities\",\"text\":" +
                   esc(options[i]) + ",\"entities\":[]},\"option\":" + esc(std::to_string(i)) + "}";
    }
    answers += ']';
    std::string poll = "{\"_ctor\":\"poll\",\"id\":" + std::to_string(rnd()) +
                       ",\"question\":{\"_ctor\":\"textWithEntities\",\"text\":" + esc(question) +
                       ",\"entities\":[]},\"answers\":" + answers + ",\"hash\":0";
    if (quiz) poll += ",\"quiz\":true";
    poll += "}";
    std::string media = "{\"_ctor\":\"inputMediaPoll\",\"poll\":" + poll;
    if (quiz && correct_option >= 0 && static_cast<size_t>(correct_option) < options.size()) {
        media += ",\"correct_answers\":[" + esc(std::to_string(correct_option)) + "]";
    }
    media += "}";
    return send_media(this, p, media, err);
}

Val Client::stop_poll(const Peer& p, long long msg_id, std::string* err) {
    std::string media =
        "{\"_ctor\":\"inputMediaPoll\",\"poll\":{\"_ctor\":\"poll\",\"id\":0,\"closed\":true,"
        "\"question\":{\"_ctor\":\"textWithEntities\",\"text\":\"\",\"entities\":[]},"
        "\"answers\":[],\"hash\":0}}";
    return api_json("messages.editMessage",
                    "{\"peer\":" + pj(p) + ",\"id\":" + std::to_string(msg_id) +
                        ",\"media\":" + media + "}",
                    err);
}

Val Client::vote_poll(const Peer& p, long long msg_id, const std::vector<int>& option_ids,
                      std::string* err) {
    std::string opts = "[";
    for (size_t i = 0; i < option_ids.size(); i++) {
        if (i) opts += ',';
        opts += esc(std::to_string(option_ids[i]));
    }
    opts += ']';
    return api_json("messages.sendVote",
                    "{\"peer\":" + pj(p) + ",\"msg_id\":" + std::to_string(msg_id) +
                        ",\"options\":" + opts + "}",
                    err);
}

Val Client::retract_vote(const Peer& p, long long msg_id, std::string* err) {
    return api_json("messages.sendVote",
                    "{\"peer\":" + pj(p) + ",\"msg_id\":" + std::to_string(msg_id) +
                        ",\"options\":[]}",
                    err);
}

Val Client::send_dice(const Peer& p, const std::string& emoji, std::string* err) {
    return send_media(this, p, "{\"_ctor\":\"inputMediaDice\",\"emoticon\":" + esc(emoji) + "}", err);
}

Val Client::send_contact(const Peer& p, const std::string& phone, const std::string& first,
                         const std::string& last, std::string* err) {
    std::string m = "{\"_ctor\":\"inputMediaContact\",\"phone_number\":" + esc(phone) +
                    ",\"first_name\":" + esc(first) + ",\"last_name\":" + esc(last) +
                    ",\"vcard\":\"\"}";
    return send_media(this, p, m, err);
}

Val Client::send_location(const Peer& p, double lat, double lon, std::string* err) {
    const std::string geo = "{\"_ctor\":\"inputMediaGeoPoint\",\"geo_point\":{\"_ctor\":\"inputGeoPoint\","
                            "\"lat\":" + fmt_coord(lat) + ",\"long\":" + fmt_coord(lon) + "}}";
    return send_media(this, p, geo, err);
}

Val Client::send_venue(const Peer& p, double lat, double lon, const std::string& title,
                       const std::string& address, std::string* err) {
    // title/address are unbounded user strings, so the document is assembled
    // with std::string: a fixed buffer could cut one in half mid-escape and
    // produce invalid JSON.
    const std::string geo = "{\"_ctor\":\"inputMediaVenue\",\"geo_point\":{\"_ctor\":\"inputGeoPoint\","
                            "\"lat\":" + fmt_coord(lat) + ",\"long\":" + fmt_coord(lon) +
                            "},\"title\":" + esc(title) + ",\"address\":" + esc(address) +
                            ",\"provider\":\"\",\"venue_id\":\"\",\"venue_type\":\"\"}";
    return send_media(this, p, geo, err);
}

Val Client::send_sticker(const Peer& p, long long file_id, long long access_hash,
                         const std::string& file_ref_b64, std::string* err) {
    std::string m = "{\"_ctor\":\"inputMediaDocument\",\"id\":{\"_ctor\":\"inputDocument\",\"id\":" +
                    std::to_string(file_id) + ",\"access_hash\":" + std::to_string(access_hash) +
                    ",\"file_reference\":" + esc(file_ref_b64) + "}}";
    return send_media(this, p, m, err);
}

Val Client::copy_message(const Peer& from, long long msg_id, const Peer& to, std::string* err) {
    return forward_messages(from, {msg_id}, to, false, err) ? Val::i64(msg_id) : Val{};
}

Val Client::get_scheduled_messages(const Peer& p, std::string* err) {
    return api_json("messages.getScheduledHistory",
                    "{\"peer\":" + pj(p) + ",\"hash\":0}", err);
}

Val Client::read_mentions(const Peer& p, std::string* err) {
    return api_json("messages.readMentions", "{\"peer\":" + pj(p) + "}", err);
}

Val Client::read_reactions(const Peer& p, std::string* err) {
    return api_json("messages.readReactions", "{\"peer\":" + pj(p) + "}", err);
}

Val Client::view_messages(const Peer& p, const std::vector<long long>& ids, std::string* err) {
    return api_json("messages.getMessagesViews",
                    "{\"peer\":" + pj(p) + ",\"id\":" + ints(ids) + ",\"increment\":true}", err);
}

Val Client::get_discussion_message(const Peer& p, long long msg_id, std::string* err) {
    return api_json("messages.getDiscussionMessage",
                    "{\"peer\":" + pj(p) + ",\"msg_id\":" + std::to_string(msg_id) + "}", err);
}

Val Client::get_discussion_replies(const Peer& p, long long msg_id, int limit, std::string* err) {
    char a[256];
    std::snprintf(a, sizeof(a),
                  "{\"peer\":%s,\"msg_id\":%lld,\"offset_id\":0,\"offset_date\":0,\"add_offset\":0,"
                  "\"limit\":%d,\"max_id\":0,\"min_id\":0,\"hash\":0}",
                  pj(p).c_str(), (long long)msg_id, limit);
    return api_json("messages.getReplies", a, err);
}

Val Client::delete_chat_history(const Peer& p, bool revoke, std::string* err) {
    std::string a = "{\"peer\":" + pj(p) + ",\"max_id\":0";
    if (revoke) a += ",\"revoke\":true";
    a += "}";
    return api_json("messages.deleteHistory", a, err);
}

Val Client::send_chat_action(const Peer& p, const std::string& action, std::string* err) {
    std::string ctor = "sendMessageTypingAction";
    if (action == "upload_photo") ctor = "sendMessageUploadPhotoAction";
    else if (action == "record_video") ctor = "sendMessageRecordVideoAction";
    else if (action == "upload_video") ctor = "sendMessageUploadVideoAction";
    else if (action == "record_audio") ctor = "sendMessageRecordAudioAction";
    else if (action == "upload_audio") ctor = "sendMessageUploadAudioAction";
    else if (action == "upload_document") ctor = "sendMessageUploadDocumentAction";
    else if (action == "find_location") ctor = "sendMessageGeoLocationAction";
    else if (action == "choose_sticker") ctor = "sendMessageChooseStickerAction";
    else if (action == "cancel") ctor = "sendMessageCancelAction";
    return api_json("messages.setTyping",
                    "{\"peer\":" + pj(p) + ",\"action\":{\"_ctor\":" + esc(ctor) + "}}", err);
}

Val Client::get_contacts(std::string* err) {
    return api_json("contacts.getContacts", "{\"hash\":0}", err);
}

Val Client::add_contact(long long user_id, const std::string& first, const std::string& last,
                        const std::string& phone, std::string* err) {
    return api_json("contacts.addContact",
                    "{\"id\":" + iuser(user_id) + ",\"first_name\":" + esc(first) +
                        ",\"last_name\":" + esc(last) + ",\"phone\":" + esc(phone) + "}",
                    err);
}

Val Client::delete_contacts(const std::vector<long long>& user_ids, std::string* err) {
    std::string arr = "[";
    for (size_t i = 0; i < user_ids.size(); i++) {
        if (i) arr += ',';
        arr += iuser(user_ids[i]);
    }
    arr += ']';
    return api_json("contacts.deleteContacts", "{\"id\":" + arr + "}", err);
}

Val Client::search_contacts(const std::string& query, int limit, std::string* err) {
    return api_json("contacts.search",
                    "{\"q\":" + esc(query) + ",\"limit\":" + std::to_string(limit) + "}", err);
}

Val Client::import_contacts(const std::string& phone, const std::string& first, const std::string& last,
                            std::string* err) {
    std::string c = "[{\"_ctor\":\"inputPhoneContact\",\"client_id\":" + std::to_string(rnd()) +
                    ",\"phone\":" + esc(phone) + ",\"first_name\":" + esc(first) +
                    ",\"last_name\":" + esc(last) + "}]";
    return api_json("contacts.importContacts", "{\"contacts\":" + c + "}", err);
}

Val Client::block_user(long long user_id, std::string* err) {
    return api_json("contacts.block", "{\"id\":" + iuser_peer(user_id) + "}", err);
}

Val Client::unblock_user(long long user_id, std::string* err) {
    return api_json("contacts.unblock", "{\"id\":" + iuser_peer(user_id) + "}", err);
}

Val Client::update_profile(const std::string& first, const std::string& last, const std::string& about,
                           std::string* err) {
    std::string a = "{";
    bool any = false;
    auto add = [&](const char* k, const std::string& v) {
        if (v.empty()) return;
        if (any) a += ',';
        any = true;
        a += std::string("\"") + k + "\":" + esc(v);
    };
    add("first_name", first);
    add("last_name", last);
    add("about", about);
    a += "}";
    return api_json("account.updateProfile", a, err);
}

Val Client::set_username(const std::string& username, std::string* err) {
    return api_json("account.updateUsername", "{\"username\":" + esc(username) + "}", err);
}

Val Client::update_status(bool offline, std::string* err) {
    return api_json("account.updateStatus",
                    std::string("{\"offline\":") + (offline ? "true" : "false") + "}", err);
}

Val Client::get_common_chats(long long user_id, int limit, std::string* err) {
    return api_json("messages.getCommonChats",
                    "{\"user_id\":" + iuser(user_id) + ",\"max_id\":0,\"limit\":" +
                        std::to_string(limit) + "}",
                    err);
}

Val Client::get_chat_photos(const Peer& p, int limit, std::string* err) {
    if (p.kind == PeerKind::User)
        return api_json("photos.getUserPhotos",
                        "{\"user_id\":" + iuser(p.id) + ",\"offset\":0,\"max_id\":0,\"limit\":" +
                            std::to_string(limit) + "}",
                        err);
    return get_chat(p, true, err);
}


Val Client::set_emoji_status(long long document_id, std::string* err) {
    if (!document_id)
        return api_json("account.updateEmojiStatus",
                        "{\"emoji_status\":{\"_ctor\":\"emojiStatusEmpty\"}}", err);
    return api_json("account.updateEmojiStatus",
                    "{\"emoji_status\":{\"_ctor\":\"emojiStatus\",\"document_id\":" +
                        std::to_string(document_id) + "}}",
                    err);
}

Val Client::get_password_hint(std::string* err) {
    Val r = api_json("account.getPassword", "{}", err);
    if (!err->empty()) return {};
    return Val::str(r.text("hint"));
}

Val Client::get_active_sessions(std::string* err) {
    return api_json("account.getAuthorizations", "{}", err);
}

Val Client::reset_sessions(std::string* err) {
    return api_json("auth.resetAuthorizations", "{}", err);
}

Val Client::sign_in_bot(const std::string& bot_token, std::string* err) {
    char a[320];
    std::snprintf(a, sizeof(a),
                  "{\"flags\":0,\"api_id\":%d,\"api_hash\":\"\",\"bot_auth_token\":%s}",
                  session_.api_id, esc(bot_token).c_str());
    return api_json("auth.importBotAuthorization", a, err);
}

Val Client::export_chat_invite_link(const Peer& p, std::string* err) {
    return api_json("messages.exportChatInvite", "{\"peer\":" + pj(p) + "}", err);
}

Val Client::create_chat_invite_link(const Peer& p, int expire_date, int usage_limit, std::string* err) {
    std::string a = "{\"peer\":" + pj(p);
    if (expire_date) a += ",\"expire_date\":" + std::to_string(expire_date);
    if (usage_limit) a += ",\"usage_limit\":" + std::to_string(usage_limit);
    a += "}";
    return api_json("messages.exportChatInvite", a, err);
}

Val Client::revoke_chat_invite_link(const Peer& p, const std::string& link, std::string* err) {
    return api_json("messages.editExportedChatInvite",
                    "{\"peer\":" + pj(p) + ",\"link\":" + esc(link) + ",\"revoked\":true}", err);
}

Val Client::approve_chat_join_request(const Peer& p, long long user_id, std::string* err) {
    return api_json("messages.hideChatJoinRequest",
                    "{\"peer\":" + pj(p) + ",\"user_id\":" + iuser(user_id) + ",\"approved\":true}",
                    err);
}

Val Client::decline_chat_join_request(const Peer& p, long long user_id, std::string* err) {
    return api_json("messages.hideChatJoinRequest",
                    "{\"peer\":" + pj(p) + ",\"user_id\":" + iuser(user_id) + "}", err);
}

Val Client::get_chat_join_requests(const Peer& p, int limit, std::string* err) {
    char a[320];
    std::snprintf(a, sizeof(a),
                  "{\"peer\":%s,\"requested\":true,\"offset_date\":0,\"offset_user\":{\"_ctor\":"
                  "\"inputUserEmpty\"},\"limit\":%d}",
                  pj(p).c_str(), limit);
    return api_json("messages.getChatInviteImporters", a, err);
}

Val Client::get_folders(std::string* err) {
    return api_json("messages.getDialogFilters", "{}", err);
}

Val Client::delete_folder(int folder_id, std::string* err) {
    return api_json("messages.updateDialogFilter",
                    "{\"id\":" + std::to_string(folder_id) + "}", err);
}

Val Client::join_folder(const std::string& slug, std::string* err) {
    return api_json("chatlists.joinChatlistInvite",
                    "{\"slug\":" + esc(slug) + ",\"peers\":[]}", err);
}

Val Client::leave_folder(int folder_id, std::string* err) {
    return api_json("chatlists.leaveChatlist",
                    "{\"chatlist\":{\"_ctor\":\"inputChatlistDialogFilter\",\"filter_id\":" +
                        std::to_string(folder_id) + "},\"peers\":[]}",
                    err);
}

Val Client::get_all_stories(std::string* err) {
    return api_json("stories.getAllStories", "{}", err);
}

Val Client::get_chat_stories(const Peer& p, std::string* err) {
    return api_json("stories.getPeerStories", "{\"peer\":" + pj(p) + "}", err);
}

Val Client::read_chat_stories(const Peer& p, int max_id, std::string* err) {
    return api_json("stories.readStories",
                    "{\"peer\":" + pj(p) + ",\"max_id\":" + std::to_string(max_id) + "}", err);
}

Val Client::delete_stories(const Peer& p, const std::vector<int>& ids, std::string* err) {
    std::string arr = "[";
    for (size_t i = 0; i < ids.size(); i++) {
        if (i) arr += ',';
        arr += std::to_string(ids[i]);
    }
    arr += ']';
    return api_json("stories.deleteStories", "{\"peer\":" + pj(p) + ",\"id\":" + arr + "}", err);
}

Val Client::get_sticker_set(const std::string& short_name, std::string* err) {
    return api_json("messages.getStickerSet",
                    "{\"stickerset\":{\"_ctor\":\"inputStickerSetShortName\",\"short_name\":" +
                        esc(short_name) + "},\"hash\":0}",
                    err);
}

Val Client::search_stickers(const std::string& q, int limit, std::string* err) {
    return api_json("messages.searchStickers",
                    "{\"q\":" + esc(q) + ",\"emoticon\":\"\",\"lang_code\":[\"en\"],\"offset\":0,"
                    "\"limit\":" + std::to_string(limit) + ",\"hash\":0}",
                    err);
}

Val Client::get_favorite_stickers(std::string* err) {
    return api_json("messages.getFavedStickers", "{\"hash\":0}", err);
}




Val Client::get_privacy(const std::string& key, std::string* err) {
    std::string ctor = "inputPrivacyKeyStatusTimestamp";
    if (key == "phone") ctor = "inputPrivacyKeyPhoneNumber";
    else if (key == "last_seen") ctor = "inputPrivacyKeyStatusTimestamp";
    else if (key == "profile_photo") ctor = "inputPrivacyKeyProfilePhoto";
    else if (key == "forwards") ctor = "inputPrivacyKeyForwards";
    else if (key == "calls") ctor = "inputPrivacyKeyPhoneCall";
    else if (key == "groups") ctor = "inputPrivacyKeyChatInvite";
    else if (key == "added_by_phone") ctor = "inputPrivacyKeyAddedByPhone";
    return api_json("account.getPrivacy", "{\"key\":{\"_ctor\":" + esc(ctor) + "}}", err);
}

Val Client::get_account_ttl(std::string* err) {
    return api_json("account.getAccountTTL", "{}", err);
}

Val Client::set_account_ttl(int days, std::string* err) {
    return api_json("account.setAccountTTL",
                    "{\"ttl\":{\"_ctor\":\"accountDaysTTL\",\"days\":" + std::to_string(days) + "}}",
                    err);
}

Val Client::apply_boost(const Peer& p, std::string* err) {
    return api_json("premium.applyBoost", "{\"peer\":" + pj(p) + "}", err);
}

Val Client::get_boosts(const Peer& p, std::string* err) {
    return api_json("premium.getBoostsList", "{\"peer\":" + pj(p) + ",\"offset\":\"\",\"limit\":100}",
                    err);
}

Val Client::get_boosts_status(const Peer& p, std::string* err) {
    return api_json("premium.getBoostsStatus", "{\"peer\":" + pj(p) + "}", err);
}

Val Client::get_stars_balance(std::string* err) {
    return api_json("payments.getStarsStatus", "{\"peer\":{\"_ctor\":\"inputPeerSelf\"}}", err);
}

Val Client::get_available_gifts(std::string* err) {
    return api_json("payments.getStarGifts", "{\"hash\":0}", err);
}

Val Client::send_gift(long long user_id, long long gift_id, const std::string& text, std::string* err) {
    std::string inv = "{\"_ctor\":\"inputInvoiceStarGift\",\"peer\":" + iuser_peer(user_id) +
                      ",\"gift_id\":" + std::to_string(gift_id);
    if (!text.empty())
        inv += ",\"message\":{\"_ctor\":\"textWithEntities\",\"text\":" + esc(text) +
               ",\"entities\":[]}";
    inv += "}";
    Val form = api_json("payments.getPaymentForm", "{\"invoice\":" + inv + "}", err);
    if (!err->empty()) return form;
    return api_json("payments.sendStarsForm",
                    "{\"form_id\":" + std::to_string(form.num("form_id")) + ",\"invoice\":" + inv + "}",
                    err);
}

Val Client::get_app_config(std::string* err) {
    return api_json("help.getAppConfig", "{\"hash\":0}", err);
}

Val Client::get_ton_balance(std::string* err) {
    return api_json("payments.getStarsTransactions",
                    "{\"peer\":{\"_ctor\":\"inputPeerSelf\"},\"offset\":\"\",\"limit\":0,\"ton\":true}",
                    err);
}

Val Client::send_rich_message(const Peer& p, const std::string& md, int64_t reply_to_msg_id,
                              std::string* err) {
    std::string plain;
    std::vector<Val> ents = parse_markdown(md, &plain);
    std::string blocks = "[{\"_ctor\":\"pageBlockParagraph\",\"text\":{\"_ctor\":\"textPlain\",\"text\":" +
                         esc(plain) + "}}]";
    std::string rich = "{\"_ctor\":\"inputRichMessage\",\"blocks\":" + blocks + "}";
    std::string a = "{\"peer\":" + pj(p) + ",\"message\":\"\",\"random_id\":" + std::to_string(rnd()) +
                    ",\"rich_message\":" + rich;
    if (reply_to_msg_id)
        a += ",\"reply_to\":{\"_ctor\":\"inputReplyToMessage\",\"reply_to_msg_id\":" +
             std::to_string(reply_to_msg_id) + "}";
    if (!ents.empty()) {
        a += ",\"entities\":[";
        for (size_t i = 0; i < ents.size(); i++) {
            if (i) a += ',';
            a += to_json(ents[i]);
        }
        a += "]";
    }
    a += "}";
    return api_json("messages.sendMessage", a, err);
}

Val Client::set_bot_profile_photo(long long bot_id, const std::string& jpeg, std::string* err) {
    if (jpeg.empty()) {
        return api_json("photos.updateProfilePhoto",
                        "{\"bot\":" + iuser(bot_id) + ",\"id\":{\"_ctor\":\"inputPhotoEmpty\"}}", err);
    }
    Val file = upload_file(jpeg, "bot.jpg", err);
    if (file.k == Val::NIL) return {};
    Val args = Val::obj("photos.uploadProfilePhoto", {
        {"bot", parse_json(iuser(bot_id), err)},
        {"file", std::move(file)},
    });
    return api("photos.uploadProfilePhoto", args, err);
}

Val Client::edit_folder_invite_link(int folder_id, const std::string& slug, const std::string& title,
                                    std::string* err) {
    std::string a = "{\"chatlist\":{\"_ctor\":\"inputChatlistDialogFilter\",\"filter_id\":" +
                    std::to_string(folder_id) + "},\"slug\":" + esc(slug);
    if (!title.empty()) a += ",\"title\":" + esc(title);
    a += "}";
    return api_json("chatlists.editExportedInvite", a, err);
}

Val Client::get_call_members(int64_t call_id, int64_t access_hash, std::string* err) {
    char a[256];
    std::snprintf(a, sizeof(a),
                  "{\"call\":{\"_ctor\":\"inputGroupCall\",\"id\":%lld,\"access_hash\":%lld},"
                  "\"ids\":[],\"sources\":[],\"offset\":\"\",\"limit\":100}",
                  (long long)call_id, (long long)access_hash);
    return api_json("phone.getGroupParticipants", a, err);
}

Val Client::enable_cloud_password(const std::string& new_password, const std::string& hint,
                                  std::string* err) {
    Val pw = api_json("account.getPassword", "{}", err);
    if (!err->empty()) return {};
    if (pw.at("has_password").k == Val::OBJ && pw.at("has_password").name == "boolTrue") {
        *err = "account already has a cloud password";
        return {};
    }
    if (pw.at("new_algo").k != Val::OBJ ||
        pw.at("new_algo").name.rfind("passwordKdfAlgo", 0) != 0) {
        *err = "account.getPassword has no new_algo; cannot set a cloud password";
        return {};
    }
    std::string new_algo;
    const std::string new_hash = srp_new_password_hash(pw, new_password, &new_algo, err);
    if (!err->empty()) return {};
    return api_json("account.updatePasswordSettings",
                    "{\"password\":{\"_ctor\":\"inputCheckPasswordEmpty\"},"
                    "\"new_settings\":{\"_ctor\":\"account.passwordInputSettings\",\"hint\":" +
                        esc(hint) + ",\"new_algo\":" + new_algo +
                        ",\"new_password_hash\":\"" + new_hash + "\"}}",
                    err);
}

Val Client::change_cloud_password(const std::string& current, const std::string& next,
                                  const std::string& hint, std::string* err) {
    Val pw = api_json("account.getPassword", "{}", err);
    if (!err->empty()) return {};
    std::string srp = srp_password_check(pw, current, err);
    if (!err->empty()) return {};
    (void)next;
    (void)hint;
    return api_json("account.updatePasswordSettings",
                    "{\"password\":" + srp +
                        ",\"new_settings\":{\"_ctor\":\"account.passwordInputSettings\"}}",
                    err);
}

Val Client::remove_cloud_password(const std::string& current, std::string* err) {
    Val pw = api_json("account.getPassword", "{}", err);
    if (!err->empty()) return {};
    std::string srp = srp_password_check(pw, current, err);
    if (!err->empty()) return {};
    return api_json("account.updatePasswordSettings",
                    "{\"password\":" + srp +
                        ",\"new_settings\":{\"_ctor\":\"account.passwordInputSettings\","
                        "\"new_algo\":{\"_ctor\":\"passwordKdfAlgoUnknown\"},\"new_password_hash\":\"\"}}",
                    err);
}

Val Client::set_bot_commands(const std::vector<std::pair<std::string, std::string>>& cmds,
                             std::string* err) {
    std::string arr = "[";
    for (size_t i = 0; i < cmds.size(); i++) {
        if (i) arr += ',';
        arr += "{\"_ctor\":\"botCommand\",\"command\":" + esc(cmds[i].first) +
               ",\"description\":" + esc(cmds[i].second) + "}";
    }
    arr += ']';
    return api_json("bots.setBotCommands",
                    "{\"scope\":{\"_ctor\":\"botCommandScopeDefault\"},\"lang_code\":\"\","
                    "\"commands\":" + arr + "}",
                    err);
}

Val Client::send_story(const Peer& p, const std::string& jpeg, const std::string& caption,
                       std::string* err) {
    Val file = upload_file(jpeg, "story.jpg", err);
    if (file.k == Val::NIL) return {};
    Val media = Val::obj("inputMediaUploadedPhoto", {{"file", std::move(file)}});
    std::string pj_err;
    Val peer_v = parse_json(pj(p), &pj_err);
    Val args = Val::obj("stories.sendStory", {
        {"peer", std::move(peer_v)},
        {"media", std::move(media)},
        {"caption", Val::str(caption)},
        {"privacy_rules", Val::vec({Val::obj("inputPrivacyValueAllowAll")})},
        {"random_id", Val::i64(rnd())},
    });
    return api("stories.sendStory", args, err);
}

}  // namespace cppgram
