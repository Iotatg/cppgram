#include "cppgram/client.hpp"

#include "cppgram/enums.hpp"

#include <cstdio>
#include <cstdlib>
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

std::string iuser(long long id, long long ah = 0) {
    char buf[160];
    std::snprintf(buf, sizeof(buf),
                  "{\"_ctor\":\"inputUser\",\"user_id\":%lld,\"access_hash\":%lld}", (long long)id,
                  (long long)ah);
    return buf;
}

std::string iuser_peer(long long id, long long ah = 0) {
    char buf[160];
    std::snprintf(buf, sizeof(buf), "{\"_ctor\":\"inputPeerUser\",\"user_id\":%lld,\"access_hash\":%lld}",
                  (long long)id, (long long)ah);
    return buf;
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
        else if (c == '\t')
            out += "\\t";
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

template <typename T>
std::string nlist(const std::vector<T>& ids) {
    std::string o = "[";
    for (size_t i = 0; i < ids.size(); i++) {
        if (i) o += ',';
        o += std::to_string(ids[i]);
    }
    o += ']';
    return o;
}

// Appends a raw JSON fragment, tolerating an empty string.
std::string frag(const std::string& j) {
    if (j.empty()) return "{}";
    std::string s;
    for (char c : j)
        if (c != '\n' && c != '\t' && c != '\r') s += c;
    return s;
}

// Accepts a bare id or a {"_ctor":...,"id":N} fragment and yields the number.
std::string numeric_of(const std::string& json) {
    std::string g = frag(json);
    if (g.empty() || g == "{}") return "0";
    if (g.find('{') == std::string::npos) return g;  // already a number
    size_t at = g.find("\"id\"");
    if (at == std::string::npos) return "0";
    at = g.find(':', at + 4);
    if (at == std::string::npos) return "0";
    ++at;
    while (at < g.size() && (g[at] == ' ' || g[at] == '"')) ++at;
    size_t end = at;
    while (end < g.size() && (std::isdigit(static_cast<unsigned char>(g[end])) || g[end] == '-'))
        ++end;
    return end > at ? g.substr(at, end - at) : "0";
}

// inputMediaInvoice is the box that carries a star invoice; provider, currency and
// prices all hang off the Invoice inside it, not off the media itself.
std::string input_media_invoice(const std::string& title, const std::string& description) {
    return "{\"_ctor\":\"inputMediaInvoice\",\"title\":" + esc(title) + ",\"description\":" +
           esc(description) + "}";
}

// Joins a string list with a separator; used where a TL field is a plain string
// but the wrapper API takes a list.
std::string join(const std::vector<std::string>& parts, const char* sep) {
    std::string out;
    for (size_t i = 0; i < parts.size(); i++) {
        if (i) out += sep;
        out += parts[i];
    }
    return out;
}

// payments.sendStarsForm only accepts form_id + invoice, so every invoice-driven
// purchase first resolves the form with payments.getPaymentForm.
Val pay_with_invoice(Client& c, const std::string& invoice_json, std::string* err) {
    std::string form_err;
    Val form = c.invoke("payments.getPaymentForm", "{\"invoice\":" + frag(invoice_json) + "}",
                        &form_err);
    if (form.k == Val::NIL || !form.has("form_id")) {
        if (err)
            *err = form_err.empty() ? "payments.getPaymentForm returned no form_id" : form_err;
        return {};
    }
    return c.invoke("payments.sendStarsForm",
                    "{\"form_id\":" + std::to_string(form.at("form_id").as_int()) +
                        ",\"invoice\":" + frag(invoice_json) + "}",
                    err);
}

// Renders a Val already built by the schema engine as a JSON object.
std::string jj(const Val& v) { return to_json(v); }

// Deterministic random_id for outgoing messages.
long long rnd_id() {
    static unsigned seed = static_cast<unsigned>(reinterpret_cast<uintptr_t>(&seed)) ^ 0x9e3779b9;
    seed = seed * 1664525u + 1013904223u;
    return static_cast<long long>(static_cast<unsigned int>(seed) & 0x7fffffff);
}

// inputReplyToMessage JSON fragment (inputReplyToMessage = 0x004ec7f5).
std::string reply_msg(int64_t id) {
    if (id == 0) return "{\"_ctor\":\"inputReplyToMessage\",\"reply_to_msg_id\":0}";
    char buf[96];
    std::snprintf(buf, sizeof(buf),
                  "{\"_ctor\":\"inputReplyToMessage\",\"reply_to_msg_id\":%lld}",
                  (long long)id);
    return buf;
}

// inputMessageEntityPeerColor / chatPeerColor JSON fragment.
std::string peer_color(int accent_color_id, long long bg_emoji_id) {
    return "{\"_ctor\":\"peerColor\",\"color\":" + std::to_string(accent_color_id) +
           ",\"background_emoji_id\":" + std::to_string(bg_emoji_id) + "}";
}

}  // namespace

// ========================= chats =========================

Val Client::add_chat_members(const Peer& chat, const std::vector<long long>& user_ids,
                             int forward_limit, std::string* err) {
    std::string arr = "[";
    for (size_t i = 0; i < user_ids.size(); i++) {
        if (i) arr += ',';
        arr += iuser(user_ids[i]);
    }
    arr += ']';
    if (chat.kind == PeerKind::Channel)
        return api_json("channels.inviteToChannel",
                        "{\"channel\":" + ich(chat) + ",\"users\":" + arr + "}", err);
    // messages.addChatUser takes a single user; basic groups need one call per member.
    Val last;
    for (size_t i = 0; i < user_ids.size(); i++) {
        last = api_json("messages.addChatUser",
                        "{\"chat_id\":" + std::to_string(chat.id) + ",\"user_id\":" +
                            iuser(user_ids[i]) + ",\"fwd_limit\":" +
                            std::to_string(forward_limit) + "}",
                        err);
        if (last.k == Val::NIL) return last;
    }
    if (user_ids.empty())
        return api_json("messages.addChatUser",
                        "{\"chat_id\":" + std::to_string(chat.id) + ",\"user_id\":" +
                            iuser(0) + ",\"fwd_limit\":" + std::to_string(forward_limit) + "}",
                        err);
    return last;
}

Val Client::create_supergroup(const std::string& title, const std::string& about, bool forum,
                              std::string* err) {
    std::string a = "{\"title\":" + esc(title) + ",\"about\":" + esc(about) + ",\"megagroup\":true";
    a += forum ? ",\"forum\":true" : ",\"forum\":false";
    a += ",\"geo_point\":{\"_ctor\":\"inputGeoPointEmpty\"},\"address\":\"\"}";
    return api_json("channels.createChannel", a, err);
}

Val Client::delete_supergroup(const Peer& p, std::string* err) {
    return api_json("channels.deleteChannel", "{\"channel\":" + ich(p) + "}", err);
}

Val Client::delete_chat_photo(const Peer& p, std::string* err) {
    std::string photo = "{\"_ctor\":\"inputChatPhotoEmpty\"}";
    if (p.kind == PeerKind::Channel)
        return api_json("channels.editPhoto", "{\"channel\":" + ich(p) + ",\"photo\":" + photo + "}",
                        err);
    return api_json("messages.editChatPhoto",
                    "{\"chat_id\":" + std::to_string(p.id) + ",\"photo\":" + photo + "}", err);
}

Val Client::set_chat_photo(const Peer& p, const std::string& jpeg, std::string* err) {
    Val file = upload_file(jpeg, "photo.jpg", err);
    if (file.k == Val::NIL) return {};
    std::string photo = "{\"_ctor\":\"inputChatUploadedPhoto\",\"file\":" + jj(file) + "}";
    if (p.kind == PeerKind::Channel)
        return api_json("channels.editPhoto", "{\"channel\":" + ich(p) + ",\"photo\":" + photo + "}",
                        err);
    return api_json("messages.editChatPhoto",
                    "{\"chat_id\":" + std::to_string(p.id) + ",\"photo\":" + photo + "}", err);
}

Val Client::set_chat_permissions(const Peer& p, const std::string& perms_json, std::string* err) {
    return api_json("messages.editChatDefaultBannedRights",
                    "{\"peer\":" + pj(p) + ",\"banned_rights\":" + frag(perms_json) + "}", err);
}

Val Client::set_chat_discussion_group(const Peer& p, const Peer& discussion, std::string* err) {
    return api_json("channels.setDiscussionGroup",
                    "{\"broadcast\":" + ich(p) + ",\"group\":" + ich(discussion) + "}", err);
}

Val Client::set_chat_direct_messages_group(const Peer& p, const Peer& group, std::string* err) {
    (void)group;
    // Layer 230 couples the direct-messages group to broadcast_messages_allowed on
    // channels.updatePaidMessagesPrice; there is no separate group argument.
    return api_json("channels.updatePaidMessagesPrice",
                    "{\"channel\":" + ich(p) + ",\"send_paid_messages_stars\":0,"
                    "\"broadcast_messages_allowed\":true}",
                    err);
}

Val Client::set_send_as_chat(const Peer& p, const Peer& send_as, std::string* err) {
    return api_json("messages.saveDefaultSendAs",
                    "{\"peer\":" + pj(p) + ",\"send_as\":" + pj(send_as) + "}", err);
}

Val Client::get_send_as_chats(const Peer& p, std::string* err) {
    return api_json("channels.getSendAs",
                    "{\"peer\":" + pj(p) + ",\"for_paid_reactions\":false}", err);
}

Val Client::set_chat_member_tag(int64_t chat_id, long long user_id, const std::string& tag,
                                std::string* err) {
    Peer p;
    p.kind = PeerKind::Chat;
    p.id = chat_id;
    return api_json("messages.editChatParticipantRank",
                    "{\"peer\":" + pj(p) + ",\"participant\":" + iuser_peer(user_id) + ",\"rank\":" +
                        esc(tag) + "}",
                    err);
}

Val Client::set_chat_accent_color(const Peer& p, int accent_color_id, long long bg_emoji_id,
                                 std::string* err) {
    if (p.kind == PeerKind::Channel)
        return api_json("channels.updateColor",
                        "{\"channel\":" + ich(p) + ",\"for_profile\":false,\"color\":" +
                            std::to_string(accent_color_id) + ",\"background_emoji_id\":" +
                            std::to_string(bg_emoji_id) + "}",
                        err);
    return api_json("account.updateColor",
                    "{\"for_profile\":false,\"color\":" + peer_color(accent_color_id, bg_emoji_id) +
                        "}",
                    err);
}

Val Client::set_chat_profile_accent_color(const Peer& p, int accent_color_id, long long bg_emoji_id,
                                          std::string* err) {
    if (p.kind != PeerKind::Channel)
        return api_json("account.updateColor",
                        "{\"for_profile\":true,\"color\":" +
                            peer_color(accent_color_id, bg_emoji_id) + "}",
                        err);
    return api_json("channels.updateColor",
                    "{\"channel\":" + ich(p) + ",\"for_profile\":true,\"color\":" +
                        std::to_string(accent_color_id) + ",\"background_emoji_id\":" +
                        std::to_string(bg_emoji_id) + "}",
                    err);
}

Val Client::set_upgraded_gift_colors(const Peer& p, long long collectible_id, std::string* err) {
(void)p;
    return api_json("account.updateColor",
                    "{\"color\":{\"_ctor\":\"inputPeerColorCollectible\",\"collectible_id\":" +
                        std::to_string(collectible_id) + "}}",
                    err);
}

Val Client::toggle_folder_tags(bool enabled, std::string* err) {
    return api_json("messages.toggleDialogFilterTags",
                    std::string("{\"enabled\":") + (enabled ? "true" : "false") + "}", err);
}

Val Client::toggle_join_to_send(const Peer& p, bool enabled, std::string* err) {
    return api_json("channels.toggleJoinToSend",
                    "{\"channel\":" + ich(p) + std::string(",\"enabled\":") +
                        (enabled ? "true" : "false") + "}",
                    err);
}

Val Client::process_chat_has_protected_content_disable_request(const Peer& p,
                                                               int64_t request_msg_id,
                                                               bool approve, std::string* err) {
    return api_json("messages.toggleNoForwards",
                    "{\"peer\":" + pj(p) + ",\"request_msg_id\":" + std::to_string(request_msg_id) +
                        std::string(",\"enabled\":") + (approve ? "true" : "false") + "}",
                    err);
}

Val Client::get_chat_settings(const Peer& p, std::string* err) {
    return api_json("messages.getPeerSettings", "{\"peer\":" + pj(p) + "}", err);
}

Val Client::get_chat_online_count(const Peer& p, std::string* err) {
    return api_json("messages.getOnlines", "{\"peer\":" + pj(p) + "}", err);
}

Val Client::get_similar_channels(const Peer& p, std::string* err) {
    return api_json("channels.getChannelRecommendations", "{\"channel\":" + ich(p) + "}", err);
}

Val Client::get_suitable_discussion_chats(std::string* err) {
    return api_json("channels.getGroupsForDiscussion", "{}", err);
}

Val Client::get_top_chats(int limit, std::string* err) {
    return api_json("contacts.getTopPeers",
                    "{\"offset\":0,\"limit\":" + std::to_string(limit) +
                        ",\"correspondents\":false,\"hash\":0,\"bots_pm\":false,\"bots_inline\":false,"
                        "\"phone_calls\":false,\"forward_users\":false,\"forward_chats\":false,"
                        "\"groups\":false,\"channels\":true,\"bots_app\":false,"
                        "\"bots_guestchat\":false}",
                    err);
}

Val Client::update_chat_notifications(const Peer& p, bool mute, int mute_until, std::string* err) {
    std::string a = "{\"peer\":{\"_ctor\":\"inputNotifyPeer\",\"peer\":" + pj(p) + "},\"settings\":{"
                    "\"_ctor\":\"inputPeerNotifySettings\",\"show_previews\":true,\"silent\":false";
    if (mute) a += ",\"mute_until\":" + std::to_string(mute_until);
    a += "}}";
    return api_json("account.updateNotifySettings", a, err);
}

Val Client::get_chat_photos_count(const Peer& p, std::string* err) {
    return api_json("messages.getSearchCounters",
                    "{\"peer\":" + pj(p) + ",\"top_msg_id\":0,\"filters\":[{\"_ctor\":"
                    "\"inputMessagesFilterChatPhotos\"}]}",
                    err);
}

Val Client::get_personal_channels(const Peer& p, std::string* err) {
(void)p;
    return api_json("channels.getAdminedPublicChannels",
                    "{\"by_location\":false,\"check_limit\":false,\"for_personal\":true,"
                    "\"for_community_peer\":false}", err);
}

Val Client::get_direct_messages_topics(const Peer& p, std::string* err) {
    return api_json("messages.getSavedDialogs",
                    "{\"offset_date\":0,\"offset_id\":0,\"offset_peer\":{\"_ctor\":\"inputPeerEmpty\"},"
                    "\"limit\":100,\"hash\":0,\"parent_peer\":" + pj(p) + "}",
                    err);
}

Val Client::get_direct_messages_topics_by_id(const Peer& p, const std::vector<int>& topic_ids,
                                             std::string* err) {
    std::string ids = "[";
    for (size_t i = 0; i < topic_ids.size(); i++) {
        if (i) ids += ',';
        ids += "{\"_ctor\":\"inputPeerChat\",\"chat_id\":" + std::to_string(topic_ids[i]) + "}";
    }
    ids += ']';
    return api_json("messages.getSavedDialogsByID",
                    "{\"ids\":" + ids + ",\"parent_peer\":" + pj(p) + "}", err);
}

Val Client::set_main_profile_tab(std::string* err) {
    return api_json("account.setMainProfileTab",
                    "{\"tab\":{\"_ctor\":\"profileTabPosts\"}}", err);
}

Val Client::hide_general_forum_topic(const Peer& p, std::string* err) {
    return api_json("messages.editForumTopic",
                    "{\"peer\":" + pj(p) + ",\"topic_id\":1,\"hidden\":true,"
                    "\"title\":\"General\"}",
                    err);
}

Val Client::unhide_general_forum_topic(const Peer& p, std::string* err) {
    return api_json("messages.editForumTopic",
                    "{\"peer\":" + pj(p) + ",\"topic_id\":1,\"hidden\":false,"
                    "\"title\":\"General\"}",
                    err);
}

Val Client::toggle_forum_topics(const Peer& p, bool enabled, bool tabs, std::string* err) {
    return api_json("channels.toggleForum",
                    "{\"channel\":" + ich(p) + ",\"enabled\":" + (enabled ? "true" : "false") +
                        ",\"tabs\":" + (tabs ? "true" : "false") + "}",
                    err);
}

Val Client::unpin_all_forum_topic_messages(const Peer& p, int64_t message_thread_id,
                                           std::string* err) {
    return api_json("messages.unpinAllMessages",
                    "{\"peer\":" + pj(p) + ",\"top_msg_id\":" + std::to_string(message_thread_id) + "}",
                    err);
}

// ========================= invite links =========================

Val Client::get_chat_invite_link(const Peer& p, const std::string& link, std::string* err) {
    return api_json("messages.getExportedChatInvite",
                    "{\"peer\":" + pj(p) + ",\"link\":" + esc(link) + "}", err);
}

Val Client::edit_chat_invite_link(const Peer& p, const std::string& link, const std::string& title,
                                  int64_t expire_date, int usage_limit, bool request_needed,
                                  std::string* err) {
    return api_json("messages.editExportedChatInvite",
                    "{\"revoked\":false,\"peer\":" + pj(p) + ",\"link\":" + esc(link) +
                        ",\"expire_date\":" + std::to_string(expire_date) + ",\"usage_limit\":" +
                        std::to_string(usage_limit) + ",\"request_needed\":" +
                        (request_needed ? "true" : "false") + ",\"title\":" + esc(title) + "}",
                    err);
}

Val Client::delete_chat_invite_link(const Peer& p, const std::string& link, std::string* err) {
    return api_json("messages.deleteExportedChatInvite",
                    "{\"peer\":" + pj(p) + ",\"link\":" + esc(link) + "}", err);
}

Val Client::approve_all_chat_join_requests(const Peer& p, const std::string& link,
                                           std::string* err) {
    return api_json("messages.hideAllChatJoinRequests",
                    "{\"peer\":" + pj(p) + ",\"approved\":true,\"link\":" + esc(link) + "}", err);
}

Val Client::decline_all_chat_join_requests(const Peer& p, const std::string& link,
                                           std::string* err) {
    return api_json("messages.hideAllChatJoinRequests",
                    "{\"peer\":" + pj(p) + ",\"approved\":false,\"link\":" + esc(link) + "}", err);
}

Val Client::get_chat_invite_link_joiners(const Peer& p, const std::string& link, int limit,
                                         int64_t offset_date, std::string* err) {
    return api_json("messages.getChatInviteImporters",
                    "{\"peer\":" + pj(p) + ",\"link\":" + esc(link) +
                        ",\"offset_date\":" + std::to_string(offset_date) +
                        ",\"offset_user\":{\"_ctor\":\"inputUserEmpty\"},\"limit\":" +
                        std::to_string(limit) +
                        ",\"requested\":false,\"subscription_expired\":false}",
                    err);
}

Val Client::get_chat_invite_link_joiners_count(const Peer& p, const std::string& link,
                                              std::string* err) {
    return api_json("messages.getChatInviteImporters",
                    "{\"peer\":" + pj(p) + ",\"link\":" + esc(link) +
                        ",\"offset_date\":0,\"offset_user\":{\"_ctor\":\"inputUserEmpty\"},"
                        "\"limit\":1,\"requested\":false,\"subscription_expired\":false}",
                    err);
}

Val Client::check_chat_folder_invite_link(const std::string& slug, std::string* err) {
    return api_json("chatlists.checkChatlistInvite", "{\"slug\":" + esc(slug) + "}", err);
}

Val Client::get_chats_for_folder_invite_link(long long folder_id, std::string* err) {
    // kurigram implements this client-side (no TL call): pick the folder from
    // get_folders(), bail out when it carries include/exclude rules or excluded
    // chats, and keep only the pinned+included group/channel peers.
    std::string ferr;
    Val folders = get_folders(&ferr);
    if (!ferr.empty()) {
        if (err) *err = ferr;
        return Val();
    }
    const Val* folder = nullptr;
    for (const auto& f : folders.list("filters")) {
        if (f.name == "dialogFilterDefault") continue;
        if (f.num("id") == folder_id) {
            folder = &f;
            break;
        }
    }
    if (!folder) {
        if (err) *err = "Folder not found";
        return Val();
    }
    auto flag = [&folder](const char* key) {
        return folder->has(key) && folder->at(key).as_int() != 0;
    };
    if (!folder->list("exclude_peers").empty() || flag("exclude_muted") || flag("exclude_read") ||
        flag("exclude_archived") || flag("contacts") || flag("non_contacts") || flag("bots") ||
        flag("groups") || flag("broadcasts")) {
        return Val::vec({});
    }
    std::vector<Val> peers;
    for (const auto& f : {folder->list("pinned_peers"), folder->list("include_peers")}) {
        for (const auto& p : f) {
            // Supergroups and channels only; a Chat type is never derived from a
            // plain user peer here, matching kurigram's ChatType filter.
            if (p.name == "inputPeerChannel" || p.name == "inputPeerChat") peers.push_back(p);
        }
    }
    return Val::vec(std::move(peers));
}

Val Client::delete_chat_admin_invite_links(const Peer& p, long long admin_id, std::string* err) {
    return api_json("messages.deleteRevokedExportedChatInvites",
                    "{\"peer\":" + pj(p) + ",\"admin_id\":" + iuser(admin_id) + "}", err);
}

Val Client::get_chat_admin_invite_links(const Peer& p, long long admin_id, int limit, bool revoked,
                                        std::string* err) {
    return api_json("messages.getExportedChatInvites",
                    "{\"peer\":" + pj(p) + ",\"admin_id\":" + iuser(admin_id) + ",\"limit\":" +
                        std::to_string(limit) + ",\"revoked\":" + (revoked ? "true" : "false") +
                        ",\"offset_date\":0,\"offset_link\":\"\"}",
                    err);
}

Val Client::get_chat_admin_invite_links_count(const Peer& p, long long admin_id, bool revoked,
                                             std::string* err) {
    return api_json("messages.getExportedChatInvites",
                    "{\"peer\":" + pj(p) + ",\"admin_id\":" + iuser(admin_id) +
                        ",\"limit\":1,\"revoked\":" + (revoked ? "true" : "false") +
                        ",\"offset_date\":0,\"offset_link\":\"\"}",
                    err);
}

Val Client::get_chat_admins_with_invite_links(const Peer& p, std::string* err) {
    return api_json("messages.getAdminsWithInvites", "{\"peer\":" + pj(p) + "}", err);
}

// ========================= messages =========================

Val Client::add_checklist_tasks(const Peer& p, int64_t message_id, const std::string& tasks_json,
                                std::string* err) {
    return api_json("messages.appendTodoList",
                    "{\"peer\":" + pj(p) + ",\"msg_id\":" + std::to_string(message_id) +
                        ",\"list\":" + frag(tasks_json) + "}", err);
}

Val Client::mark_checklist_tasks_as_done(const Peer& p, int64_t message_id,
                                        const std::vector<int>& ids, std::string* err) {
    std::string ids_j = "[";
    for (size_t i = 0; i < ids.size(); i++) {
        if (i) ids_j += ',';
        ids_j += std::to_string(ids[i]);
    }
    ids_j += ']';
    return api_json("messages.toggleTodoCompleted",
                    "{\"peer\":" + pj(p) + ",\"msg_id\":" + std::to_string(message_id) +
                        ",\"completed\":" + ids_j + ",\"incompleted\":[]}", err);
}

Val Client::add_poll_option(const Peer& p, int64_t message_id, const std::string& option,
                            std::string* err) {
    return api_json("messages.addPollAnswer",
                    "{\"peer\":" + pj(p) + ",\"msg_id\":" + std::to_string(message_id) +
                        ",\"answer\":" + esc(option) + "}", err);
}

Val Client::delete_poll_option(const Peer& p, int64_t message_id, const std::string& option,
                               std::string* err) {
    return api_json("messages.deletePollAnswer",
                    "{\"peer\":" + pj(p) + ",\"msg_id\":" + std::to_string(message_id) +
                        ",\"option\":" + esc(option) + "}", err);
}

Val Client::add_to_gifs(const std::string& input_document_json, bool save, std::string* err) {
    return api_json("messages.saveGif",
                    "{\"id\":" + frag(input_document_json) + ",\"unsave\":" +
                        (save ? "false" : "true") + "}",
                    err);
}

Val Client::approve_suggested_post(const Peer& p, int64_t message_id, const std::string& comment,
                                    std::string* err) {
    return api_json("messages.toggleSuggestedPostApproval",
                    "{\"peer\":" + pj(p) + ",\"msg_id\":" + std::to_string(message_id) +
                        ",\"reject\":false,\"reject_comment\":" + esc(comment) + "}", err);
}

Val Client::decline_suggested_post(const Peer& p, int64_t message_id, const std::string& comment,
                                    std::string* err) {
    return api_json("messages.toggleSuggestedPostApproval",
                    "{\"peer\":" + pj(p) + ",\"msg_id\":" + std::to_string(message_id) +
                        ",\"reject\":true,\"reject_comment\":" + esc(comment) + "}", err);
}

Val Client::compose_text_with_ai(const Peer& p, const std::string& prompt, std::string* err) {
    (void)p;
    return api_json("messages.composeMessageWithAI",
                    "{\"text\":{\"_ctor\":\"textWithEntities\",\"text\":" + esc(prompt) +
                        ",\"entities\":[]}}",
                    err);
}

Val Client::fix_text_with_ai(const Peer& p, const std::string& text, std::string* err) {
    (void)p;
    return api_json("messages.composeMessageWithAI",
                    "{\"text\":{\"_ctor\":\"textWithEntities\",\"text\":" + esc(text) +
                        ",\"entities\":[]},\"proofread\":true}",
                    err);
}

Val Client::summarize_message(const Peer& p, const std::vector<int>& message_ids, std::string* err) {
    std::string ids_j = "[";
    for (size_t i = 0; i < message_ids.size(); i++) {
        if (i) ids_j += ',';
        ids_j += std::to_string(message_ids[i]);
    }
    ids_j += ']';
    return api_json("messages.summarizeText", "{\"peer\":" + pj(p) + ",\"id\":" + ids_j + "}", err);
}

Val Client::translate_message_text(const Peer& p, int64_t message_id, const std::string& to_lang,
                                   std::string* err) {
    return api_json("messages.translateText",
                    "{\"peer\":" + pj(p) + ",\"id\":[{\"_ctor\":\"inputMessageID\",\"id\":" +
                        std::to_string(message_id) + "}],\"to_lang\":" + esc(to_lang) + "}", err);
}

Val Client::translate_text(const std::string& text, const std::string& to_lang, std::string* err) {
    return api_json("messages.translateText",
                    "{\"peer\":{\"_ctor\":\"inputPeerEmpty\"},\"text\":" + esc(text) +
                        ",\"to_lang\":" + esc(to_lang) + "}", err);
}

Val Client::send_screenshot_notification(const Peer& p, const std::vector<int64_t>& msg_ids,
                                         const std::string& jpeg, std::string* err) {
    Val file = upload_file(jpeg, "shot.jpg", err);
    if (file.k == Val::NIL) return {};
    return api_json("messages.sendScreenshotNotification",
                    "{\"peer\":" + pj(p) + ",\"reply_to\":{\"_ctor\":\"inputReplyToMessage\","
                    "\"reply_to_msg_id\":" + (msg_ids.empty() ? std::to_string(0) : std::to_string(msg_ids[0])) +
                        "},\"random_id\":" + std::to_string(rnd_id()) + "}", err);
}

Val Client::forward_media_group(const Peer& from, const std::vector<int64_t>& msg_ids,
                                const Peer& to, std::string* err) {
    return api_json("messages.forwardMessages",
                    "{\"from_peer\":" + pj(from) + ",\"id\":" + nlist(msg_ids) +
                        ",\"to_peer\":" + pj(to) + ",\"random_id\":" + nlist(std::vector<long long>(msg_ids.size(), rnd_id())) +
                        ",\"drop_author\":false,\"drop_media_captions\":false,\"with_my_score\":false}",
                    err);
}

Val Client::copy_media_group(const Peer& to, const std::string& multi_media_json,
                             std::string* err) {
    return api_json("messages.sendMultiMedia",
                    "{\"peer\":" + pj(to) + ",\"multi_media\":" + frag(multi_media_json) +
                        ",\"silent\":false}",
                    err);
}

Val Client::send_cached_media(const Peer& p, const std::string& media_json, const std::string& caption,
                              int64_t reply_to_msg_id, std::string* err) {
    return api_json("messages.sendMedia",
                    "{\"peer\":" + pj(p) + ",\"media\":" + frag(media_json) + ",\"message\":" +
                        esc(caption) + ",\"random_id\":" + std::to_string(rnd_id()) +
                        ",\"reply_to\":" + reply_msg(reply_to_msg_id) + "}", err);
}

Val Client::get_media_group(const Peer& p, int64_t message_id, std::string* err) {
    // messages.getMessages only takes ids; the caller's peer keeps the album scoped.
    (void)p;
    std::string ids = "[";
    for (int64_t i = message_id - 9; i <= message_id + 9; i++) {
        if (i < 1) continue;
        if (ids.size() > 1) ids += ',';
        ids += std::to_string(i);
    }
    ids += ']';
    return api_json("messages.getMessages", "{\"id\":" + ids + "}", err);
}

Val Client::send_media_group_json(const Peer& p, const std::string& media_list_json,
                                 const std::string& caption, bool disable_notification,
                                 std::string* err) {
    return api_json("messages.sendMultiMedia",
                    "{\"peer\":" + pj(p) + ",\"multi_media\":" + frag(media_list_json) +
                        ",\"message\":" + esc(caption) + ",\"random_id\":" + std::to_string(rnd_id()) +
                        std::string(",\"silent\":") + (disable_notification ? "true" : "false") + "}",
                    err);
}

Val Client::send_checklist(const Peer& p, const std::string& checklist_json,
                           const std::string& caption, int64_t reply_to_msg_id, std::string* err) {
    return api_json("messages.sendMedia",
                    "{\"peer\":" + pj(p) + ",\"media\":" + frag(checklist_json) + ",\"message\":" +
                        esc(caption) + ",\"random_id\":" + std::to_string(rnd_id()) +
                        ",\"reply_to\":" + reply_msg(reply_to_msg_id) + "}", err);
}

Val Client::send_live_photo(const Peer& p, const std::string& jpeg, const std::string& caption,
                            std::string* err) {
    Val file = upload_file(jpeg, "live.jpg", err);
    if (file.k == Val::NIL) return {};
    return api_json("messages.sendMedia",
                    "{\"peer\":" + pj(p) + ",\"media\":{\"_ctor\":\"inputMediaUploadedPhoto\","
                    "\"file\":" + jj(file) + "},\"message\":" + esc(caption) +
                        ",\"random_id\":" + std::to_string(rnd_id()) + "}",
                    err);
}

Val Client::send_video_note(const Peer& p, const std::string& data, int duration, int w, int h,
                            std::string* err) {
    Val file = upload_file(data, "note.mp4", err);
    if (file.k == Val::NIL) return {};
    char attrs[320];
    std::snprintf(attrs, sizeof(attrs),
                  "{\"_ctor\":\"documentAttributeVideo\",\"duration\":%d,\"w\":%d,"
                  "\"h\":%d,\"round_message\":true,\"supports_streaming\":true}",
                  duration, w, h);
    return api_json("messages.sendMedia",
                    "{\"peer\":" + pj(p) + ",\"media\":{\"_ctor\":\"inputMediaUploadedDocument\","
                    "\"file\":" + jj(file) + ",\"mime_type\":\"video/mp4\",\"attributes\":[" +
                        attrs + "]},\"message\":\"\",\"random_id\":"
                        + std::to_string(rnd_id()) + "}",
                    err);
}

Val Client::send_paid_media(const Peer& p, const std::string& media_list_json, long long stars,
                            std::string* err) {
    return api_json("messages.sendMultiMedia",
                    "{\"peer\":" + pj(p) + ",\"multi_media\":" + frag(media_list_json) +
                        ",\"silent\":false,\"allow_paid_stars\":" + std::to_string(stars) + "}",
                    err);
}

Val Client::send_paid_reaction(const Peer& p, int64_t message_id, int count,
                               const std::string& reaction_json, std::string* err) {
    // Layer 230 messages.sendPaidReaction carries no reaction field: the server
    // picks the emoji, so only the count and the privacy flag are ours to send.
    (void)reaction_json;
    return api_json("messages.sendPaidReaction",
                    "{\"peer\":" + pj(p) + ",\"msg_id\":" + std::to_string(message_id) +
                        ",\"count\":" + std::to_string(count) +
                        ",\"random_id\":" + std::to_string(rnd_id()) + "}", err);
}

Val Client::send_message_draft(const Peer& p, const std::string& text,
                               const std::string& entities_json, int64_t reply_to_msg_id,
                               std::string* err) {
    return api_json("messages.setTyping",
                    "{\"peer\":" + pj(p) + ",\"top_msg_id\":" + std::to_string(reply_to_msg_id) +
                        ",\"action\":{\"_ctor\":\"sendMessageTextDraftAction\",\"random_id\":" +
                        std::to_string(rnd_id()) + ",\"text\":{\"_ctor\":\"textWithEntities\","
                        "\"text\":" + esc(text) + ",\"entities\":" +
                        (entities_json.empty() ? "[]" : entities_json) + "}}}", err);
}

Val Client::send_rich_message_draft(const Peer& p, const std::string& md, int64_t reply_to_msg_id,
                                    std::string* err) {
    std::string plain;
    std::vector<Val> ents = parse_markdown(md, &plain);
    std::string a = "{\"peer\":" + pj(p) + ",\"top_msg_id\":" + std::to_string(reply_to_msg_id);
    a += ",\"action\":{\"_ctor\":\"sendMessageTextDraftAction\",\"random_id\":" + std::to_string(rnd_id()) +
         ",\"text\":{\"_ctor\":\"textWithEntities\","
         "\"text\":" + esc(plain) + ",\"entities\":[";
    for (size_t i = 0; i < ents.size(); i++) {
        if (i) a += ',';
        a += to_json(ents[i]);
    }
    a += "]}}";
    return api_json("messages.setTyping", a, err);
}

Val Client::send_web_page(const Peer& p, const std::string& url, const std::string& text,
                          std::string* err) {
    return api_json("messages.sendMessage",
                    "{\"peer\":" + pj(p) + ",\"message\":" + esc(text + " " + url) +
                        ",\"random_id\":" + std::to_string(rnd_id()) + ",\"no_webpage\":false}",
                    err);
}

Val Client::edit_message_caption(const Peer& p, int64_t message_id, const std::string& caption,
                                 std::string* err) {
    return api_json("messages.editMessage",
                    "{\"peer\":" + pj(p) + ",\"id\":" + std::to_string(message_id) +
                        ",\"message\":" + esc(caption) + ",\"no_webpage\":false}", err);
}

Val Client::edit_message_media(const Peer& p, int64_t message_id, const std::string& media_json,
                               std::string* err) {
    return api_json("messages.editMessage",
                    "{\"peer\":" + pj(p) + ",\"id\":" + std::to_string(message_id) + ",\"media\":" +
                        frag(media_json) + "}", err);
}

Val Client::edit_message_reply_markup(const Peer& p, int64_t message_id,
                                      const std::string& keyboard_json, std::string* err) {
    return api_json("messages.editMessage",
                    "{\"peer\":" + pj(p) + ",\"id\":" + std::to_string(message_id) +
                        ",\"reply_markup\":" + frag(keyboard_json) + "}", err);
}

Val Client::edit_inline_text(const std::string& inline_message_id, const std::string& text,
                            const std::string& entities_json, std::string* err) {
    return api_json("messages.editInlineBotMessage",
                    "{\"id\":" + esc(inline_message_id) + ",\"message\":" + esc(text) +
                        ",\"no_webpage\":false,\"entities\":" +
                        (entities_json.empty() ? "[]" : entities_json) + "}", err);
}

Val Client::edit_inline_caption(const std::string& inline_message_id, const std::string& caption,
                                std::string* err) {
    return api_json("messages.editInlineBotMessage",
                    "{\"id\":" + esc(inline_message_id) + ",\"message\":" + esc(caption) +
                        ",\"no_webpage\":false,\"entities\":[]}", err);
}

Val Client::edit_inline_media(const std::string& inline_message_id, const std::string& media_json,
                              std::string* err) {
    return api_json("messages.editInlineBotMessage",
                    "{\"id\":" + esc(inline_message_id) + ",\"media\":" + frag(media_json) + "}", err);
}

Val Client::edit_inline_reply_markup(const std::string& inline_message_id,
                                    const std::string& keyboard_json, std::string* err) {
    return api_json("messages.editInlineBotMessage",
                    "{\"id\":" + esc(inline_message_id) + ",\"reply_markup\":" + frag(keyboard_json) +
                        "}", err);
}

Val Client::edit_message_checklist(const Peer& p, int64_t message_id, const std::string& tasks_json,
                                  const std::vector<int>& done_ids, std::string* err) {
    std::string ids_j = "[";
    for (size_t i = 0; i < done_ids.size(); i++) {
        if (i) ids_j += ',';
        ids_j += std::to_string(done_ids[i]);
    }
    ids_j += ']';
    Val r = api_json("messages.editMessage",
                     "{\"peer\":" + pj(p) + ",\"id\":" + std::to_string(message_id) +
                         ",\"media\":" + frag(tasks_json) + "}",
                     err);
    if (r.k == Val::NIL) return r;
    if (done_ids.empty()) return r;
    // Completion never rides on editMessage: layer 230 marks tasks done through
    // messages.toggleTodoCompleted, so the checklist edit only swaps the media.
    return api_json("messages.toggleTodoCompleted",
                    "{\"peer\":" + pj(p) + ",\"msg_id\":" + std::to_string(message_id) +
                        ",\"completed\":" + ids_j + ",\"incompleted\":[]}",
                    err);
}

Val Client::delete_direct_messages_chat_topic_history(const Peer& p, std::string* err) {
    return api_json("messages.deleteSavedHistory", "{\"peer\":" + pj(p) + ",\"max_id\":0}", err);
}

Val Client::get_direct_messages_chat_topic_history(const Peer& p, int limit, std::string* err) {
    return api_json("messages.getSavedHistory",
                    "{\"peer\":" + pj(p) + ",\"limit\":" + std::to_string(limit) +
                        ",\"offset_date\":0,\"offset_id\":0}", err);
}

Val Client::set_direct_messages_chat_topic_is_marked_as_unread(const Peer& p, bool unread,
                                                              std::string* err) {
    return api_json("messages.markDialogUnread",
                    "{\"peer\":{\"_ctor\":\"inputDialogPeer\",\"peer\":" + pj(p) + "},\"unread\":" +
                        (unread ? "true" : "false") + "}", err);
}

Val Client::get_chat_history_count(const Peer& p, std::string* err) {
    return api_json("messages.getHistory",
                    "{\"peer\":" + pj(p) + ",\"offset_id\":0,\"offset_date\":0,\"add_offset\":0,"
                    "\"limit\":1,\"max_id\":0,\"min_id\":0,\"hash\":0}", err);
}

Val Client::get_discussion_replies_count(const Peer& p, int64_t message_id, std::string* err) {
    return api_json("messages.getReplies",
                    "{\"peer\":" + pj(p) + ",\"msg_id\":" + std::to_string(message_id) +
                        ",\"limit\":1}", err);
}

Val Client::get_user_personal_chat_messages(long long user_id, std::string* err) {
    return api_json("messages.getPersonalChannelHistory",
                    "{\"user_id\":" + iuser(user_id) + ",\"limit\":100,\"max_id\":0,"
                    "\"min_id\":0,\"hash\":0}", err);
}

Val Client::get_available_effects(std::string* err) {
    return api_json("messages.getAvailableEffects", "{\"hash\":0}", err);
}

Val Client::search_global(const std::string& q, int limit, bool broadcasts_only, bool groups_only,
                          bool users_only, std::string* err) {
    return api_json("messages.searchGlobal",
                    "{\"folder_id\":0,\"q\":" + esc(q) + ",\"filter\":{\"_ctor\":"
                    "\"inputMessagesFilterEmpty\"},\"min_date\":0,\"max_date\":0,\"offset_rate\":0,"
                    "\"offset_peer\":{\"_ctor\":\"inputPeerEmpty\"},\"offset_id\":0,\"limit\":" +
                        std::to_string(limit) + ",\"broadcasts_only\":" +
                        (broadcasts_only ? "true" : "false") + ",\"groups_only\":" +
                        (groups_only ? "true" : "false") + ",\"users_only\":" +
                        (users_only ? "true" : "false") + "}",
                    err);
}

Val Client::search_global_count(const std::string& q, std::string* err) {
    return api_json("messages.searchGlobal",
                    "{\"folder_id\":0,\"q\":" + esc(q) + ",\"filter\":{\"_ctor\":"
                    "\"inputMessagesFilterEmpty\"},\"min_date\":0,\"max_date\":0,\"offset_rate\":0,"
                    "\"offset_peer\":{\"_ctor\":\"inputPeerEmpty\"},\"offset_id\":0,\"limit\":1}",
                    err);
}

Val Client::search_posts(const std::string& q, int limit, bool hashtag, std::string* err) {
    return api_json("channels.searchPosts",
                    "{\"hashtag\":" + esc(hashtag ? q : std::string()) + ",\"query\":" +
                        esc(hashtag ? std::string() : q) + ",\"offset_rate\":0,\"offset_peer\":{"
                        "\"_ctor\":\"inputPeerEmpty\"},\"offset_id\":0,\"limit\":" +
                        std::to_string(limit) + "}", err);
}

Val Client::search_posts_count(const std::string& q, std::string* err) {
    return api_json("channels.searchPosts",
                    "{\"hashtag\":" + esc(q) + ",\"query\":\"\",\"offset_rate\":0,"
                    "\"offset_peer\":{\"_ctor\":\"inputPeerEmpty\"},\"offset_id\":0,\"limit\":1}",
                    err);
}

Val Client::get_web_app_url(const std::string& bot, const std::string& short_name,
                            const std::string& url, std::string* err) {
    // requestSimpleWebView keys off the bot and the url; a bare short name is not
    // a wire field, so it only selects the side-menu variant.
    (void)short_name;
    std::string j = "{\"bot\":" + esc(bot) + ",\"platform\":\"desktop\"";
    if (url.empty()) {
        j += ",\"from_side_menu\":true";
    } else {
        j += ",\"url\":" + esc(url);
    }
    j += ",\"start_param\":\"\",\"theme_params\":{\"_ctor\":\"dataJSON\"},\"fullscreen\":false}";
    return api_json("messages.requestSimpleWebView", j, err);
}

Val Client::get_main_web_app(const std::string& bot, std::string* err) {
(void)bot;
    return api_json("messages.requestMainWebView",
                    "{\"peer\":{\"_ctor\":\"inputPeerSelf\"},\"bot\":{\"_ctor\":\"inputUserSelf\"},"
                    "\"platform\":\"desktop\",\"start_param\":\"\",\"theme_params\":{"
                    "\"_ctor\":\"dataJSON\"},\"compact\":false,\"fullscreen\":false}", err);
}

Val Client::get_web_app_link_url(const std::string& bot, const std::string& short_name,
                                std::string& url, std::string* err) {
                                (void)bot;
                                (void)url;
    return api_json("messages.requestAppWebView",
                    "{\"peer\":{\"_ctor\":\"inputPeerSelf\"},\"app\":{\"_ctor\":\"inputBotAppShortName\","
                    "\"bot_id\":{\"_ctor\":\"inputUserEmpty\"},\"short_name\":" + esc(short_name) +
                        "},\"platform\":\"desktop\",\"write_allowed\":false,\"start_param\":\"\","
                        "\"theme_params\":{\"_ctor\":\"dataJSON\"}}", err);
}

Val Client::open_web_app(const Peer& p, const std::string& bot, const std::string& short_name,
                         std::string* err) {
                         (void)short_name;
    return api_json("messages.requestWebView",
                    "{\"peer\":" + pj(p) + ",\"bot\":" + esc(bot) + ",\"url\":\"\",\"platform\":"
                    "\"desktop\",\"start_param\":\"\",\"theme_params\":{\"_ctor\":\"dataJSON\"},"
                    "\"fullscreen\":false,\"from_bot_menu\":true}", err);
}

Val Client::start_bot(const Peer& p, const std::string& bot, const std::string& start_param,
                     std::string* err) {
                     (void)bot;
    return api_json("messages.startBot",
                    "{\"bot\":" + iuser(0) + ",\"peer\":" + pj(p) + ",\"start_param\":" +
                        esc(start_param) + ",\"random_id\":" + std::to_string(rnd_id()) + "}", err);
}

Val Client::stream_media(const Peer& p, int64_t message_id, const std::string& path,
                        std::string* err) {
    return download_media_msg(p, message_id, path, err);
}

// ========================= folders =========================

Val Client::reorder_folders(const std::vector<int>& order, std::string* err) {
    return api_json("messages.updateDialogFiltersOrder",
                    "{\"order\":" + nlist(std::vector<long long>(order.begin(), order.end())) + "}",
                    err);
}

// ========================= contacts =========================

Val Client::get_contacts_count(std::string* err) {
    return api_json("contacts.getContacts", "{\"hash\":0}", err);
}

Val Client::get_blocked_message_senders(std::string* err) {
    return api_json("contacts.getBlocked",
                    "{\"offset\":0,\"limit\":100,\"my_stories_from\":false}", err);
}

Val Client::set_contact_note(long long user_id, const std::string& note, std::string* err) {
    return api_json("contacts.updateContactNote",
                    "{\"id\":" + iuser_peer(user_id) + ",\"note\":{\"_ctor\":\"textWithEntities\","
                    "\"text\":" + esc(note) + ",\"entities\":[]}}",
                    err);
}

// ========================= payments =========================

Val Client::add_collection_gifts(const Peer& owner, int collection_id,
                                 const std::string& stargifts_json, std::string* err) {
    return api_json("payments.updateStarGiftCollection",
                    "{\"peer\":" + pj(owner) + ",\"collection_id\":" + std::to_string(collection_id) +
                        ",\"add_stargift\":" + frag(stargifts_json) + ",\"delete_stargift\":[]}", err);
}

Val Client::remove_collection_gifts(const Peer& owner, int collection_id,
                                    const std::string& stargifts_json, std::string* err) {
    return api_json("payments.updateStarGiftCollection",
                    "{\"peer\":" + pj(owner) + ",\"collection_id\":" + std::to_string(collection_id) +
                        ",\"add_stargift\":[],\"delete_stargift\":" + frag(stargifts_json) + "}", err);
}

Val Client::create_gift_collection(const Peer& owner, const std::string& title,
                                   const std::string& gift_ids_json, std::string* err) {
    return api_json("payments.createStarGiftCollection",
                    "{\"peer\":" + pj(owner) + ",\"title\":" + esc(title) + ",\"stargift\":" +
                        frag(gift_ids_json) + "}", err);
}

Val Client::delete_gift_collection(const Peer& owner, int collection_id, std::string* err) {
    return api_json("payments.deleteStarGiftCollection",
                    "{\"peer\":" + pj(owner) + ",\"collection_id\":" + std::to_string(collection_id) +
                        "}", err);
}

Val Client::reorder_gift_collections(const Peer& owner, const std::vector<int>& order,
                                     std::string* err) {
    return api_json("payments.reorderStarGiftCollections",
                    "{\"peer\":" + pj(owner) + ",\"order\":" +
                        nlist(std::vector<long long>(order.begin(), order.end())) + "}", err);
}

Val Client::set_gift_collection_name(const Peer& owner, int collection_id,
                                    const std::string& title, std::string* err) {
    return api_json("payments.updateStarGiftCollection",
                    "{\"peer\":" + pj(owner) + ",\"collection_id\":" + std::to_string(collection_id) +
                        ",\"title\":" + esc(title) + ",\"add_stargift\":[],\"delete_stargift\":[]}",
                    err);
}

Val Client::get_gift_collections(const Peer& owner, std::string* err) {
    return api_json("payments.getStarGiftCollections", "{\"peer\":" + pj(owner) + ",\"hash\":0}",
                    err);
}

Val Client::get_chat_gifts(const Peer& owner, int offset, int limit, bool exclude_unsaved,
                           bool exclude_saved, bool exclude_unlimited, bool exclude_unique,
                           bool exclude_upgradable, bool exclude_unupgradable, std::string* err) {
    return api_json("payments.getSavedStarGifts",
                    "{\"peer\":" + pj(owner) + ",\"offset\":" + esc(std::to_string(offset)) +
                        ",\"limit\":" + std::to_string(limit) + ",\"exclude_unsaved\":" +
                        (exclude_unsaved ? "true" : "false") + ",\"exclude_saved\":" +
                        (exclude_saved ? "true" : "false") + ",\"exclude_unlimited\":" +
                        (exclude_unlimited ? "true" : "false") + ",\"exclude_unique\":" +
                        (exclude_unique ? "true" : "false") + ",\"exclude_upgradable\":" +
                        (exclude_upgradable ? "true" : "false") + ",\"exclude_unupgradable\":" +
                        (exclude_unupgradable ? "true" : "false") + "}", err);
}

Val Client::get_chat_gifts_count(const Peer& owner, std::string* err) {
    return api_json("payments.getSavedStarGifts",
                    "{\"peer\":" + pj(owner) + ",\"offset\":\"\",\"limit\":1}", err);
}

Val Client::apply_gift_code(const std::string& slug, std::string* err) {
    return api_json("payments.applyGiftCode", "{\"slug\":" + esc(slug) + "}", err);
}

Val Client::check_gift_code(const std::string& slug, std::string* err) {
    return api_json("payments.checkGiftCode", "{\"slug\":" + esc(slug) + "}", err);
}

Val Client::get_gift_upgrade_preview(const Peer& p, const std::string& gift_json, std::string* err) {
(void)p;
    return api_json("payments.getStarGiftUpgradePreview", "{\"gift_id\":" + frag(gift_json) + "}",
                    err);
}

Val Client::get_gift_upgrade_variants(const Peer& p, const std::string& gift_json, std::string* err) {
(void)p;
    return api_json("payments.getStarGiftUpgradeAttributes", "{\"gift_id\":" + frag(gift_json) + "}",
                    err);
}

Val Client::upgrade_gift(const Peer& owner, const std::string& gift_json, bool keep_original_details,
                          std::string* err) {
    (void)owner;
    return api_json("payments.upgradeStarGift",
                    "{\"stargift\":" + frag(gift_json) +
                        ",\"keep_original_details\":" + (keep_original_details ? "true" : "false") +
                        "}",
                    err);
}

Val Client::buy_gift_upgrade(const Peer& owner, const std::string& gift_json,
                              const std::string& invoice_json, std::string* err) {
                              (void)owner;
                              (void)gift_json;
    return pay_with_invoice(*this, invoice_json, err);
}

Val Client::transfer_gift(const Peer& owner, const std::string& gift_json, const Peer& to,
                          std::string* err) {
                          (void)owner;
    return api_json("payments.transferStarGift",
                    "{\"stargift\":" + frag(gift_json) + ",\"to_id\":" + pj(to) + "}", err);
}

Val Client::convert_gift_to_stars(const Peer& owner, const std::string& gift_json,
                                  std::string* err) {
                                  (void)owner;
    return api_json("payments.convertStarGift", "{\"stargift\":" + frag(gift_json) + "}", err);
}

Val Client::drop_gift_original_details(const Peer& owner, const std::string& gift_json,
                                        const std::string& invoice_json, std::string* err) {
                                        (void)owner;
                                        (void)gift_json;
    return pay_with_invoice(*this, invoice_json, err);
}

Val Client::craft_gift(const Peer& p, const std::string& gifts_json, const std::string& invoice_json,
                       std::string* err) {
                       (void)p;
                       (void)invoice_json;
    return api_json("payments.craftStarGift",
                    "{\"stargift\":" + frag(gifts_json) + "}",
                    err);
}

Val Client::get_gifts_for_crafting(long long gift_id, int limit, std::string* err) {
    // kurigram pages payments.getCraftStarGifts with gift_id + an empty offset.
    return api_json("payments.getCraftStarGifts",
                    "{\"gift_id\":" + std::to_string(gift_id) + ",\"offset\":\"\",\"limit\":" +
                        std::to_string(limit > 0 && limit < 100 ? limit : 100) + "}",
                    err);
}

Val Client::get_upgraded_gift(const std::string& slug, std::string* err) {
    return api_json("payments.getUniqueStarGift", "{\"slug\":" + esc(slug) + "}", err);
}

Val Client::get_upgraded_gift_value_info(const std::string& slug, std::string* err) {
    return api_json("payments.getUniqueStarGiftValueInfo", "{\"slug\":" + esc(slug) + "}", err);
}

Val Client::get_gift_auction_state(const Peer& p, const std::string& gift_json, std::string* err) {
    (void)p;
    // payments.getStarGiftAuctionState takes the auction (gift id) plus a version.
    return api_json("payments.getStarGiftAuctionState",
                    "{\"auction\":{\"_ctor\":\"inputStarGiftAuction\",\"gift_id\":" +
                        numeric_of(gift_json) + "},\"version\":0}",
                    err);
}

Val Client::place_gift_auction_bid(const Peer& p, const std::string& gift_json, long long stars,
                                    const std::string& invoice_json, std::string* err) {
                                    (void)p;
                                    (void)gift_json;
                                    (void)stars;
    // kurigram buys auction rounds through the invoice form, same as the other
    // star purchases: resolve the form, then send it.
    return pay_with_invoice(*this, invoice_json, err);
}

Val Client::increase_gift_auction_bid(const Peer& p, const std::string& gift_json, long long stars,
                                      const std::string& invoice_json, std::string* err) {
    return place_gift_auction_bid(p, gift_json, stars, invoice_json, err);
}

Val Client::send_gift_purchase_offer(const Peer& p, const std::string& gift_json, long long stars,
                                      long long duration, bool anonym, const std::string& invoice_json,
                                      std::string* err) {
    // payments.sendStarGiftOffer names the gift by slug and needs a random_id; the
    // layer-230 shape has no anonymity flag or invoice.
    (void)anonym;
    (void)invoice_json;
    return api_json("payments.sendStarGiftOffer",
                    "{\"peer\":" + pj(p) + ",\"slug\":" + esc(gift_json) + ",\"price\":" +
                        std::to_string(stars) + ",\"duration\":" + std::to_string(duration) +
                        ",\"random_id\":" + std::to_string(rnd_id()) + "}",
                    err);
}

Val Client::process_gift_purchase_offer(const std::string& msg_id, bool approve, std::string* err) {
    long long id = std::atoll(msg_id.c_str());
    return api_json("payments.resolveStarGiftOffer",
                    "{\"offer_msg_id\":" + std::to_string(id) + ",\"decline\":" +
                        (approve ? "false" : "true") + "}",
                    err);
}

Val Client::search_gifts_for_resale(long long gift_id, int order, bool for_crafting, bool for_stars,
                                    const std::string& attributes_json, int limit,
                                    const std::string& offset, std::string* err) {
    // payments.getResaleStarGifts flags: sort_by_price/sort_by_num come from the
    // GiftForResaleOrder enum, for_craft/stars_only are the crafting and stars filters.
    std::string j = "{\"gift_id\":" + std::to_string(gift_id) + ",\"offset\":" + esc(offset) +
                    ",\"limit\":" + std::to_string(limit);
    if (order == enums::GiftForResaleOrder::PRICE) j += ",\"sort_by_price\":true";
    if (order == enums::GiftForResaleOrder::NUMBER) j += ",\"sort_by_num\":true";
    if (for_crafting) j += ",\"for_craft\":true";
    if (for_stars) j += ",\"stars_only\":true";
    if (!attributes_json.empty()) j += ",\"attributes\":" + frag(attributes_json);
    j += "}";
    return api_json("payments.getResaleStarGifts", j, err);
}

Val Client::send_resold_gift(const Peer& to, const std::string& slug,
                              const std::string& invoice_json, std::string* err) {
                              (void)to;
                              (void)slug;
    return pay_with_invoice(*this, invoice_json, err);
}

Val Client::get_payment_form(const std::string& invoice_json, std::string* err) {
    return api_json("payments.getPaymentForm",
                    "{\"invoice\":" + frag(invoice_json) + "}", err);
}

Val Client::send_payment_form(const Peer& p, int64_t form_id, const std::string& invoice_json,
                              const std::string& payment_json, std::string* err) {
                              (void)p;
                              (void)payment_json;
    return api_json("payments.sendStarsForm",
                    "{\"form_id\":" + std::to_string(form_id) + ",\"invoice\":" +
                        frag(invoice_json) + "}",
                    err);
}

Val Client::edit_star_subscription(const std::string& subscription_id, bool canceled,
                                  std::string* err) {
    return api_json("payments.changeStarsSubscription",
                    "{\"peer\":{\"_ctor\":\"inputPeerSelf\"},\"subscription_id\":" +
                        esc(subscription_id) + ",\"canceled\":" + (canceled ? "true" : "false") + "}",
                    err);
}

Val Client::reuse_star_subscription(const std::string& subscription_id,
                                    const std::string& invoice_json, std::string* err) {
                                    (void)invoice_json;
    return api_json("payments.fulfillStarsSubscription",
                    "{\"peer\":{\"_ctor\":\"inputPeerSelf\"},\"subscription_id\":" +
                        esc(subscription_id) + "}", err);
}

Val Client::gift_premium_with_stars(const Peer& p, int months, long long stars,
                                     const std::string& invoice_json, std::string* err) {
                                     (void)stars;
                                     (void)invoice_json;
    // The star cost lives on the invoice, which inputInvoicePremiumGiftStars builds
    // from the recipient and the month count.
    std::string inv = "{\"_ctor\":\"inputInvoicePremiumGiftStars\",\"user_id\":" + pj(p) +
                      ",\"months\":" + std::to_string(months) + "}";
    return pay_with_invoice(*this, inv, err);
}

Val Client::suggest_birthday(int day, int month, int year, std::string* err) {
    return api_json("users.suggestBirthday",
                    "{\"id\":{\"_ctor\":\"inputUserSelf\"},\"birthday\":{\"_ctor\":\"birthday\","
                    "\"day\":" + std::to_string(day) + ",\"month\":" + std::to_string(month) +
                        ",\"year\":" + (year ? std::to_string(year) : "0") + "}}",
                    err);
}

Val Client::toggle_gift_pinned(const Peer& p, const std::string& stargifts_json, bool pinned,
                               std::string* err) {
                               (void)pinned;
    return api_json("payments.toggleStarGiftsPinnedToTop",
                    "{\"peer\":" + pj(p) + ",\"stargift\":" + frag(stargifts_json) + "}", err);
}

Val Client::hide_gift(const Peer& p, const std::string& gift_json, bool hide, std::string* err) {
(void)p;
    return api_json("payments.saveStarGift",
                    "{\"stargift\":" + frag(gift_json) + ",\"unsave\":" + (hide ? "true" : "false") + "}",
                    err);
}

// ========================= stickers =========================

Val Client::add_favorite_sticker(const std::string& document_json, std::string* err) {
    return api_json("messages.faveSticker",
                    "{\"id\":" + frag(document_json) + ",\"unfave\":false}", err);
}

Val Client::remove_favorite_sticker(const std::string& document_json, std::string* err) {
    return api_json("messages.faveSticker",
                    "{\"id\":" + frag(document_json) + ",\"unfave\":true}", err);
}

Val Client::add_recent_sticker(const std::string& document_json, std::string* err) {
    return api_json("messages.saveRecentSticker",
                    "{\"id\":" + frag(document_json) + ",\"unsave\":false,\"attached\":false}", err);
}

Val Client::remove_recent_sticker(const std::string& document_json, std::string* err) {
    return api_json("messages.saveRecentSticker",
                    "{\"id\":" + frag(document_json) + ",\"unsave\":true,\"attached\":false}", err);
}

Val Client::get_recent_stickers(std::string* err) {
    return api_json("messages.getRecentStickers", "{\"hash\":0}", err);
}

Val Client::clear_recent_stickers(std::string* err) {
    return api_json("messages.clearRecentStickers", "{}", err);
}

Val Client::get_owned_sticker_sets(std::string* err) {
    return api_json("messages.getMyStickers", "{\"offset_id\":0,\"limit\":100}", err);
}

Val Client::add_sticker_to_set(const Peer& owner, const std::string& short_name,
                               const std::string& sticker_json, std::string* err) {
                               (void)owner;
    return api_json("stickers.addStickerToSet",
                    "{\"stickerset\":{\"_ctor\":\"inputStickerSetShortName\",\"short_name\":" +
                        esc(short_name) + "},\"sticker\":" + frag(sticker_json) + "}", err);
}

Val Client::delete_sticker_from_set(const std::string& short_name,
                                    const std::string& document_json, std::string* err) {
    (void)short_name;
    return api_json("stickers.removeStickerFromSet",
                    "{\"sticker\":" + frag(document_json) + "}", err);
}

Val Client::create_new_sticker_set(const Peer& owner, const std::string& title,
                                   const std::string& short_name, const std::string& stickers_json,
                                   bool masquerade, bool animated, bool video, bool text_color,
                                   bool thumb, std::string* err) {
    // Layer 230 createStickerSet has no animated/video/masquerade switches; the
    // set kind is decided by the uploaded sticker documents themselves.
    (void)masquerade;
    (void)animated;
    (void)video;
    (void)thumb;
    return api_json("stickers.createStickerSet",
                    "{\"user_id\":" + pj(owner) +
                        ",\"text_color\":" + (text_color ? "true" : "false") +
                        ",\"title\":" + esc(title) + ",\"short_name\":" +
                        esc(short_name) + ",\"stickers\":" + frag(stickers_json) + ",\"software\":\"cppgram\"}",
                    err);
}

Val Client::delete_sticker_set(const std::string& short_name, std::string* err) {
    return api_json("stickers.deleteStickerSet",
                    "{\"stickerset\":{\"_ctor\":\"inputStickerSetShortName\",\"short_name\":" +
                        esc(short_name) + "}}",
                    err);
}

Val Client::change_sticker_set(const Peer& p, const std::string& short_name, bool install,
                              bool archived, std::string* err) {
    (void)p;
    if (!install)
        return api_json("messages.uninstallStickerSet",
                        "{\"stickerset\":{\"_ctor\":\"inputStickerSetShortName\",\"short_name\":" +
                            esc(short_name) + "}}",
                        err);
    return api_json("messages.installStickerSet",
                    "{\"stickerset\":{\"_ctor\":\"inputStickerSetShortName\",\"short_name\":" +
                        esc(short_name) + "},\"archived\":" + (archived ? "true" : "false") + "}",
                    err);
}

Val Client::reorder_installed_sticker_sets(const std::vector<long long>& ids, bool masquerade,
                                           std::string* err) {
    (void)masquerade;
    return api_json("messages.reorderStickerSets",
                    "{\"order\":" + nlist(ids) + ",\"emojis\":false,\"masks\":false}", err);
}

Val Client::replace_sticker_in_set(const std::string& short_name,
                                   const std::string& old_doc_json,
                                   const std::string& new_sticker_json, std::string* err) {
    (void)short_name;
    return api_json("stickers.replaceSticker",
                    "{\"sticker\":" + frag(old_doc_json) + ",\"new_sticker\":" +
                        frag(new_sticker_json) + "}", err);
}

Val Client::get_custom_emoji_stickers(const std::vector<long long>& ids, std::string* err) {
    return api_json("messages.getCustomEmojiDocuments", "{\"document_id\":" + nlist(ids) + "}", err);
}

Val Client::search_sticker_sets(const std::string& q, bool exclude_featured, std::string* err) {
    return api_json("messages.searchStickerSets",
                    "{\"q\":" + esc(q) + ",\"exclude_featured\":" +
                        (exclude_featured ? "true" : "false") + ",\"hash\":0}", err);
}

Val Client::get_suggested_sticker_set_name(const std::string& title, std::string* err) {
    return api_json("stickers.suggestShortName", "{\"title\":" + esc(title) + "}", err);
}

Val Client::upload_sticker_file(const Peer& owner, const std::string& data, const std::string& mime,
                               const std::string& attrs_json, std::string* err) {
    Val file = upload_file(data, "sticker", err);
    if (file.k == Val::NIL) return {};
    return api_json("messages.uploadMedia",
                    "{\"peer\":" + pj(owner) + ",\"media\":{\"_ctor\":\"inputMediaUploadedDocument\","
                    "\"file\":" + jj(file) + ",\"mime_type\":" + esc(mime) + ",\"attributes\":" +
                        (attrs_json.empty() ? "[]" : attrs_json) + "}}",
                    err);
}

Val Client::set_sticker_set_title(const std::string& short_name, const std::string& title,
                                 std::string* err) {
    return api_json("stickers.renameStickerSet",
                    "{\"stickerset\":{\"_ctor\":\"inputStickerSetShortName\",\"short_name\":" +
                        esc(short_name) + "},\"title\":" + esc(title) + "}", err);
}

Val Client::set_sticker_set_thumbnail(const std::string& short_name, const Peer& owner,
                                      const std::string& thumb_json, bool video,
                                      std::string* err) {
    (void)owner;
    (void)video;
    return api_json("stickers.setStickerSetThumb",
                    "{\"stickerset\":{\"_ctor\":\"inputStickerSetShortName\",\"short_name\":" +
                        esc(short_name) + "},\"thumb\":" + frag(thumb_json) + "}", err);
}

Val Client::set_custom_emoji_sticker_set_thumbnail(const std::string& short_name,
                                                   const std::string& thumb_json, std::string* err) {
    return api_json("stickers.setStickerSetThumb",
                    "{\"stickerset\":{\"_ctor\":\"inputStickerSetShortName\",\"short_name\":" +
                        esc(short_name) + "},\"thumb_document_id\":" + numeric_of(thumb_json) + "}",
                    err);
}

Val Client::set_sticker_position_in_set(const std::string& short_name,
                                        const std::string& document_json, int position,
                                        std::string* err) {
                                        (void)short_name;
    return api_json("stickers.changeStickerPosition",
                    "{\"sticker\":" + frag(document_json) + ",\"position\":" +
                        std::to_string(position) + "}",
                    err);
}

Val Client::set_sticker_emoji_list(const std::string& short_name, const std::string& document_json,
                                   const std::vector<std::string>& emoji_list, std::string* err) {
    // Layer 230 changeSticker wants emoji/keywords as plain strings, not vectors.
    (void)short_name;
    return api_json("stickers.changeSticker",
                    "{\"sticker\":" + frag(document_json) + ",\"emoji\":" + esc(join(emoji_list, "")) +
                        "}",
                    err);
}

Val Client::set_sticker_keywords(const std::string& short_name, const std::string& document_json,
                                 const std::vector<std::string>& keywords, bool mask,
                                 std::string* err) {
    (void)short_name;
    (void)mask;
    return api_json("stickers.changeSticker",
                    "{\"sticker\":" + frag(document_json) + ",\"keywords\":" +
                        esc(join(keywords, ",")) + "}",
                    err);
}

Val Client::set_sticker_mask_position(const std::string& short_name, const std::string& document_json,
                                      const std::string& mask_json, std::string* err) {
    (void)short_name;
    return api_json("stickers.changeSticker",
                    "{\"sticker\":" + frag(document_json) + ",\"mask_coords\":" +
                        frag(mask_json) + "}", err);
}

// ========================= stories =========================

Val Client::can_post_stories(const Peer& p, std::string* err) {
    return api_json("stories.canSendStory", "{\"peer\":" + pj(p) + "}", err);
}

Val Client::get_stories(const Peer& p, int64_t story_id, std::string* err) {
    return api_json("stories.getStoriesByID",
                    "{\"peer\":" + pj(p) + ",\"id\":[" + std::to_string(story_id) + "]}", err);
}

Val Client::get_archived_stories(const Peer& p, int limit, int64_t offset_id, std::string* err) {
    return api_json("stories.getStoriesArchive",
                    "{\"peer\":" + pj(p) + ",\"offset_id\":" + std::to_string(offset_id) +
                        ",\"limit\":" + std::to_string(limit) + "}", err);
}

Val Client::get_story_views(const Peer& p, const std::string& q, int64_t offset_id,
                           std::string* err) {
    // stories.getStoryViewsList is per story: the story id and a paging cursor.
    (void)offset_id;
    return api_json("stories.getStoryViewsList",
                    "{\"peer\":" + pj(p) + ",\"id\":0,\"q\":" + esc(q) +
                        ",\"offset\":\"\",\"limit\":100}", err);
}

Val Client::get_pinned_stories(const Peer& p, int limit, int64_t offset_id, std::string* err) {
    return api_json("stories.getPinnedStories",
                    "{\"peer\":" + pj(p) + ",\"offset_id\":" + std::to_string(offset_id) +
                        ",\"limit\":" + std::to_string(limit) + "}", err);
}

Val Client::view_stories(const Peer& p, const std::vector<int64_t>& story_ids, std::string* err) {
    return api_json("stories.incrementStoryViews",
                    "{\"peer\":" + pj(p) + ",\"id\":" + nlist(story_ids) + "}", err);
}

Val Client::edit_story_caption(const Peer& p, int64_t story_id, const std::string& caption,
                               const std::string& entities_json, std::string* err) {
    return api_json("stories.editStory",
                    "{\"peer\":" + pj(p) + ",\"id\":" + std::to_string(story_id) + ",\"caption\":" +
                        esc(caption) + ",\"entities\":" + (entities_json.empty() ? "[]" : entities_json) +
                        ",\"media\":{\"_ctor\":\"inputMediaEmpty\"},\"privacy_rules\":[]}", err);
}

Val Client::edit_story_media(const Peer& p, int64_t story_id, const std::string& media_json,
                            std::string* err) {
    return api_json("stories.editStory",
                    "{\"peer\":" + pj(p) + ",\"id\":" + std::to_string(story_id) + ",\"media\":" +
                        frag(media_json) + ",\"privacy_rules\":[]}", err);
}

Val Client::edit_story_privacy(const Peer& p, int64_t story_id, const std::string& privacy_json,
                              std::string* err) {
    return api_json("stories.editStory",
                    "{\"peer\":" + pj(p) + ",\"id\":" + std::to_string(story_id) + ",\"media\":{"
                    "\"_ctor\":\"inputMediaEmpty\"},\"privacy_rules\":" + frag(privacy_json) + "}",
                    err);
}

Val Client::delete_story(const Peer& p, int64_t story_id, std::string* err) {
    return api_json("stories.deleteStories",
                    "{\"peer\":" + pj(p) + ",\"id\":[" + std::to_string(story_id) + "]}", err);
}

Val Client::pin_chat_stories(const Peer& p, const std::vector<int64_t>& story_ids, bool pin,
                            std::string* err) {
    return api_json("stories.togglePinned",
                    "{\"peer\":" + pj(p) + ",\"id\":" + nlist(story_ids) + ",\"pinned\":" +
                        (pin ? "true" : "false") + "}", err);
}

Val Client::hide_chat_stories(const Peer& p, bool hide, std::string* err) {
    return api_json("stories.togglePeerStoriesHidden",
                    "{\"peer\":" + pj(p) + ",\"hidden\":" + (hide ? "true" : "false") + "}", err);
}

Val Client::enable_stealth_mode(std::string* err) {
    return api_json("stories.activateStealthMode", "{\"past\":false,\"future\":false}", err);
}

Val Client::forward_story(const Peer& to, int64_t story_id, const Peer& from, std::string* err) {
    return api_json("messages.sendMedia",
                    "{\"peer\":" + pj(to) + ",\"media\":{\"_ctor\":\"inputMediaStory\",\"peer\":" +
                        pj(from) + ",\"id\":" + std::to_string(story_id) + "},\"message\":\"\","
                    "\"random_id\":" + std::to_string(rnd_id()) + "}", err);
}

Val Client::copy_story(const Peer& p, int64_t story_id, const std::string& media_json,
                       const std::string& caption, std::string* err) {
                       (void)story_id;
    return api_json("messages.sendMedia",
                    "{\"peer\":" + pj(p) + ",\"media\":" + frag(media_json) + ",\"message\":" +
                        esc(caption) + ",\"random_id\":" + std::to_string(rnd_id()) + "}", err);
}

// ========================= bots =========================

Val Client::answer_callback_query(int64_t query_id, const std::string& text, const std::string& url,
                                 bool show_alert, std::string* err) {
    return api_json("messages.setBotCallbackAnswer",
                    "{\"query_id\":" + std::to_string(query_id) + ",\"cache_time\":300," +
                        (text.empty() ? "" : "\"message\":" + esc(text) + ",") + "\"alert\":" +
                        (show_alert ? "true" : "false") +
                        (url.empty() ? "" : ",\"url\":" + esc(url)) + "}", err);
}

Val Client::answer_inline_query(int64_t query_id, const std::string& results_json, int cache_time,
                                bool is_personal, const std::string& next_offset,
                                const std::string& switch_pm_json, std::string* err) {
    return api_json("messages.setInlineBotResults",
                    "{\"gallery\":false,\"private\":" + std::string(is_personal ? "true" : "false") +
                        ",\"query_id\":" + std::to_string(query_id) +
                        ",\"results\":" + frag(results_json) + ",\"cache_time\":" +
                        std::to_string(cache_time) + ",\"next_offset\":" + esc(next_offset) +
                        ",\"switch_pm\":" + frag(switch_pm_json) + "}", err);
}

Val Client::answer_web_app_query(const std::string& query_id, const std::string& result_json,
                                std::string* err) {
    return api_json("messages.sendWebViewResultMessage",
                    "{\"bot_query_id\":" + esc(query_id) + ",\"result\":" + frag(result_json) + "}",
                    err);
}

Val Client::answer_pre_checkout_query(int64_t query_id, bool ok, const std::string& error_msg,
                                      std::string* err) {
    return api_json("messages.setBotPrecheckoutResults",
                    "{\"query_id\":" + std::to_string(query_id) + ",\"success\":" +
                        (ok ? "true" : "false") + ",\"error\":" + esc(error_msg) + "}", err);
}

Val Client::answer_shipping_query(int64_t query_id, bool ok, const std::string& error_msg,
                                  const std::string& shipping_options_json, std::string* err) {
                                  (void)ok;
    return api_json("messages.setBotShippingResults",
                    "{\"query_id\":" + std::to_string(query_id) + ",\"error\":" + esc(error_msg) +
                        ",\"shipping_options\":" + frag(shipping_options_json) + "}", err);
}

Val Client::answer_guest_query(const std::string& query_id, const std::string& result_json,
                               std::string* err) {
    return api_json("messages.setBotGuestChatResult",
                    "{\"query_id\":" + esc(query_id) + ",\"result\":" + frag(result_json) + "}", err);
}

Val Client::answer_chat_join_request_query(int64_t query_id, bool approved, std::string* err) {
    return api_json("bots.setJoinChatResults",
                    "{\"query_id\":" + std::to_string(query_id) + ",\"result\":{\"_ctor\":\"" +
                        std::string(approved ? "joinChatBotResultApproved" : "joinChatBotResultDeclined") +
                        "\"}}",
                    err);
}

Val Client::send_chat_join_request_web_app(int64_t chat_id, int64_t query_id, std::string* err) {
    (void)chat_id;
    return api_json("bots.setJoinChatResults",
                    "{\"query_id\":" + std::to_string(query_id) + ",\"result\":{\"_ctor\":"
                    "\"joinChatBotResultApproved\"}}",
                    err);
}

Val Client::get_inline_bot_results(const std::string& bot, const Peer& peer, const std::string& query,
                                   int64_t offset, std::string* err) {
    return api_json("messages.getInlineBotResults",
                    "{\"bot\":" + esc(bot) + ",\"peer\":" + pj(peer) + ",\"query\":" +
                        esc(query) + ",\"offset\":\"" + std::to_string(offset) + "\",\"geo_point\":{"
                        "\"_ctor\":\"inputGeoPointEmpty\"}}",
                        err);
}

Val Client::send_inline_bot_result(const Peer& p, int64_t query_id, const std::string& result_id,
                                   bool hide_via, const std::string& top_msg_id, std::string* err) {
                                   (void)top_msg_id;
    return api_json("messages.sendInlineBotResult",
                    "{\"peer\":" + pj(p) + ",\"query_id\":" + std::to_string(query_id) + ",\"id\":" +
                        esc(result_id) + ",\"hide_via\":" + (hide_via ? "true" : "false") +
                        ",\"random_id\":" + std::to_string(rnd_id()) + "}", err);
}

Val Client::create_invoice_link(const std::string& title, const std::string& description,
                               const std::string& payload_json, const std::string& provider_token,
                               const std::string& currency, long long amount, std::string* err) {
                               (void)payload_json;
    (void)provider_token;
    (void)currency;
    (void)amount;
    return api_json("payments.exportInvoice",
                    "{\"invoice_media\":{" + input_media_invoice(title, description) + "}}",
                    err);
}

Val Client::send_invoice(const Peer& p, const std::string& title, const std::string& description,
                        const std::string& payload_json, const std::string& provider_token,
                        const std::string& currency, long long amount,
                        const std::string& reply_markup_json, std::string* err) {
                        (void)payload_json;
                        (void)amount;
    (void)payload_json;
    (void)provider_token;
    (void)currency;
    (void)amount;
    return api_json("messages.sendMedia",
                    "{\"peer\":" + pj(p) + ",\"media\":" + input_media_invoice(title, description) +
                        ",\"message\":\"\","
                    "\"reply_markup\":" + frag(reply_markup_json) + ",\"random_id\":" +
                        std::to_string(rnd_id()) + "}", err);
}

Val Client::refund_star_payment(int64_t user_id, const std::string& telegram_payment_charge_id,
                               std::string* err) {
    return api_json("payments.refundStarsCharge",
                    "{\"user_id\":" + iuser_peer(user_id) + ",\"charge_id\":" +
                        esc(telegram_payment_charge_id) + "}", err);
}

Val Client::edit_user_star_subscription(int64_t user_id, const std::string& telegram_payment_charge_id,
                                        bool is_canceled, std::string* err) {
    return api_json("payments.botCancelStarsSubscription",
                    "{\"user_id\":" + iuser_peer(user_id) + ",\"charge_id\":" +
                        esc(telegram_payment_charge_id) + ",\"restore\":" +
                        (is_canceled ? "false" : "true") + "}", err);
}

Val Client::get_bot_name(const std::string& bot, const std::string& lang_code, std::string* err) {
    return api_json("bots.getBotInfo",
                    "{\"lang_code\":" + esc(lang_code) + ",\"bot\":" + esc(bot) + "}", err);
}

Val Client::set_bot_name(const std::string& bot, const std::string& lang_code, const std::string& name,
                        std::string* err) {
    return api_json("bots.setBotInfo",
                    "{\"lang_code\":" + esc(lang_code) + ",\"bot\":" + esc(bot) + ",\"name\":" +
                        esc(name) + ",\"about\":\"\",\"description\":\"\"}", err);
}

Val Client::get_bot_info_description(const std::string& bot, const std::string& lang_code,
                                     std::string* err) {
    return api_json("bots.getBotInfo",
                    "{\"lang_code\":" + esc(lang_code) + ",\"bot\":" + esc(bot) + "}", err);
}

Val Client::set_bot_info_description(const std::string& bot, const std::string& lang_code,
                                     const std::string& description, std::string* err) {
    return api_json("bots.setBotInfo",
                    "{\"lang_code\":" + esc(lang_code) + ",\"bot\":" + esc(bot) +
                        ",\"name\":\"\",\"about\":\"\",\"description\":" + esc(description) + "}", err);
}

Val Client::get_bot_info_short_description(const std::string& bot, const std::string& lang_code,
                                           std::string* err) {
    return api_json("bots.getBotInfo",
                    "{\"lang_code\":" + esc(lang_code) + ",\"bot\":" + esc(bot) + "}", err);
}

Val Client::set_bot_info_short_description(const std::string& bot, const std::string& lang_code,
                                           const std::string& short_description, std::string* err) {
    return api_json("bots.setBotInfo",
                    "{\"lang_code\":" + esc(lang_code) + ",\"bot\":" + esc(bot) +
                        ",\"name\":\"\",\"about\":" + esc(short_description) + ",\"description\":\"\"}",
                    err);
}

Val Client::get_bot_default_privileges(std::string* err) {
    return api_json("users.getFullUser", "{\"id\":{\"_ctor\":\"inputUserSelf\"}}", err);
}

Val Client::set_bot_default_privileges(const std::string& channel, const std::string& admin_rights_json,
                                       bool broadcast, bool groups, bool users, std::string* err) {
                                       (void)channel;
                                       (void)broadcast;
                                       (void)groups;
                                       (void)users;
    return api_json("bots.setBotBroadcastDefaultAdminRights",
                    "{\"admin_rights\":" + frag(admin_rights_json) + "}", err);
}

Val Client::set_chat_menu_button(const std::string& button_json, std::string* err) {
    return api_json("bots.setBotMenuButton", "{\"button\":" + frag(button_json) + "}", err);
}

Val Client::get_chat_menu_button(long long user_id, std::string* err) {
    return api_json("bots.getBotMenuButton", "{\"user_id\":" + iuser(user_id) + "}", err);
}

Val Client::create_bot(const std::string& bot_username, const std::string& bot_name,
                       const std::string& reply_markup_json, std::string* err) {
    (void)reply_markup_json;
    return api_json("bots.createBot",
                    "{\"name\":" + esc(bot_name) + ",\"username\":" + esc(bot_username) +
                        ",\"manager_id\":{\"_ctor\":\"inputUserSelf\"}}",
                    err);
}

Val Client::check_bot_username(const std::string& bot, const std::string& username,
                               std::string* err) {
    (void)bot;
    return api_json("bots.checkUsername",
                    "{\"username\":" + esc(username) + "}", err);
}

Val Client::get_owned_bots(std::string* err) {
    return api_json("bots.getAdminedBots", "{}", err);
}

Val Client::get_managed_bot_token(long long bot_id, std::string* err) {
    return api_json("bots.exportBotToken",
                    "{\"bot\":" + iuser(bot_id) + ",\"revoke\":false}", err);
}

Val Client::replace_managed_bot_token(long long bot_id, const std::string& new_token,
                                      std::string* err) {
    (void)new_token;
    return api_json("bots.exportBotToken",
                    "{\"bot\":" + iuser(bot_id) + ",\"revoke\":true}", err);
}

Val Client::get_managed_bot_access_settings(long long bot_id, std::string* err) {
    return api_json("bots.getAccessSettings", "{\"bot\":" + iuser(bot_id) + "}", err);
}

Val Client::set_managed_bot_access_settings(long long bot_id, bool can_access_messages, bool manage,
                                             std::string* err) {
    // "manage" is the inverse of restricted, and the user allow-list is add_users.
    (void)can_access_messages;
    return api_json("bots.editAccessSettings",
                    "{\"bot\":" + iuser(bot_id) + ",\"restricted\":" +
                        (manage ? "false" : "true") + ",\"add_users\":[]}",
                    err);
}

Val Client::get_bot_commands(const std::string& bot, const std::string& lang_code,
                             const std::string& scope, long long chat_id, std::string* err) {
    // bots.getBotCommands only takes the scope and the language; bot and chat are
    // implied by the session that resolves the bot.
    (void)bot;
    (void)chat_id;
    return api_json("bots.getBotCommands",
                    "{\"scope\":" + frag(scope) + ",\"lang_code\":" + esc(lang_code) + "}", err);
}

Val Client::set_game_score(const Peer& p, int64_t message_id, long long user_id, int score,
                          bool edit_message, std::string* err) {
                          (void)p;
    (void)message_id;
    // Layer 230 setInlineGameScore has no peer; the id is the inline message id.
    return api_json("messages.setInlineGameScore",
                    "{\"edit_message\":" + std::string(edit_message ? "true" : "false") +
                        ",\"force\":" + std::string(edit_message ? "true" : "false") +
                        ",\"id\":{\"_ctor\":"
                        "\"inputBotInlineMessageID\",\"dc_id\":0,\"id\":0,\"access_hash\":0},"
                        "\"user_id\":" +
                        iuser(user_id) + ",\"score\":" + std::to_string(score) + "}", err);
}

Val Client::get_game_high_scores(const Peer& p, int64_t message_id, long long user_id,
                                 std::string* err) {
    return api_json("messages.getGameHighScores",
                    "{\"peer\":" + pj(p) + ",\"id\":" + std::to_string(message_id) + ",\"user_id\":" +
                        iuser(user_id) + "}", err);
}

Val Client::send_game(const Peer& p, const std::string& bot, const std::string& game_short_name,
                     int64_t reply_to_msg_id, std::string* err) {
    (void)bot;
    return api_json("messages.sendMedia",
                    "{\"peer\":" + pj(p) + ",\"media\":{\"_ctor\":\"inputMediaGame\",\"id\":{"
                    "\"_ctor\":\"inputGameShortName\",\"bot_id\":{\"_ctor\":\"inputUserSelf\"},"
                    "\"short_name\":" + esc(game_short_name) + "},\"message\":\"\","
                    "\"reply_to\":" + reply_msg(reply_to_msg_id) + ",\"random_id\":" +
                        std::to_string(rnd_id()) + "}", err);
}

Val Client::delete_ephemeral_message(const Peer& p, int64_t message_id, std::string* err) {
    return api_json("ephemeral.deleteMessage",
                    "{\"peer\":" + pj(p) + ",\"id\":[" + std::to_string(message_id) + "]}", err);
}

Val Client::edit_ephemeral_message_text(const Peer& p, int64_t message_id, const std::string& text,
                                       const std::string& entities_json, std::string* err) {
    return api_json("ephemeral.editMessage",
                    "{\"peer\":" + pj(p) + ",\"id\":" + std::to_string(message_id) + ",\"message\":" +
                        esc(text) + ",\"entities\":" + (entities_json.empty() ? "[]" : entities_json) +
                        "}", err);
}

Val Client::edit_ephemeral_message_caption(const Peer& p, int64_t message_id,
                                           const std::string& caption, std::string* err) {
    return api_json("ephemeral.editMessage",
                    "{\"peer\":" + pj(p) + ",\"id\":" + std::to_string(message_id) + ",\"message\":" +
                        esc(caption) + "}", err);
}

Val Client::edit_ephemeral_message_media(const Peer& p, int64_t message_id,
                                        const std::string& media_json, std::string* err) {
    return api_json("ephemeral.editMessage",
                    "{\"peer\":" + pj(p) + ",\"id\":" + std::to_string(message_id) + ",\"media\":" +
                        frag(media_json) + "}", err);
}

Val Client::edit_ephemeral_message_reply_markup(const Peer& p, int64_t message_id,
                                                const std::string& keyboard_json, std::string* err) {
    return api_json("ephemeral.editMessage",
                    "{\"peer\":" + pj(p) + ",\"id\":" + std::to_string(message_id) +
                        ",\"reply_markup\":" + frag(keyboard_json) + "}", err);
}

Val Client::answer_terms_of_service(const std::string& terms_id, std::string* err) {
    return api_json("help.acceptTermsOfService", "{\"id\":" + frag(terms_id) + "}", err);
}

// ========================= users =========================

Val Client::check_username(const Peer& channel, const std::string& username, std::string* err) {
    return api_json("channels.checkUsername",
                    "{\"channel\":" + ich(channel) + ",\"username\":" + esc(username) + "}", err);
}

Val Client::get_chat_audios(const Peer& p, int limit, int64_t offset_id, std::string* err) {
    return api_json("users.getSavedMusic",
                    "{\"id\":" + iuser_peer(p.id) + ",\"offset\":" + std::to_string(offset_id) +
                        ",\"limit\":" + std::to_string(limit) + ",\"hash\":0}", err);
}

Val Client::get_chat_audios_count(const Peer& p, std::string* err) {
    return api_json("users.getSavedMusic",
                    "{\"id\":" + iuser_peer(p.id) + ",\"offset\":0,\"limit\":1,\"hash\":0}", err);
}

Val Client::get_default_emoji_statuses(std::string* err) {
    return api_json("account.getDefaultEmojiStatuses", "{\"hash\":0}", err);
}

Val Client::set_personal_channel(const Peer& p, std::string* err) {
    return api_json("account.updatePersonalChannel", "{\"channel\":" + ich(p) + "}", err);
}

Val Client::set_profile_photo(const std::string& jpeg, const std::string& video,
                            double video_start_ts, std::string* err) {
                            (void)video;
                            (void)video_start_ts;
    Val file = upload_file(jpeg, "photo.jpg", err);
    if (file.k == Val::NIL) return {};
    return api_json("photos.uploadProfilePhoto",
                    "{\"fallback\":false,\"file\":" + jj(file) + "}", err);
}

Val Client::update_birthday(int day, int month, int year, std::string* err) {
    return api_json("account.updateBirthday",
                    "{\"birthday\":{\"_ctor\":\"birthday\","
                    "\"day\":" + std::to_string(day) + ",\"month\":" + std::to_string(month) +
                        ",\"year\":" + (year ? std::to_string(year) : "0") + "}}", err);
}

Val Client::delete_profile_photos(const std::vector<int64_t>& photo_ids, std::string* err) {
    std::string ids = "[";
    for (size_t i = 0; i < photo_ids.size(); i++) {
        if (i) ids += ',';
        ids += "{\"_ctor\":\"inputPhoto\",\"id\":" + std::to_string(photo_ids[i]) +
               ",\"access_hash\":0,\"file_reference\":\"\"}";
    }
    ids += ']';
    return api_json("photos.deletePhotos", "{\"id\":" + ids + "}", err);
}

// ========================= account =========================

Val Client::add_profile_audio(const Peer& owner, const std::string& data, const std::string& mime,
                            const std::string& title, const std::string& performer,
                            std::string* err) {
    Val file = upload_file(data, "audio", err);
    if (file.k == Val::NIL) return {};
    char attrs[320];
    std::snprintf(attrs, sizeof(attrs),
                  "{\"_ctor\":\"documentAttributeAudio\",\"title\":%s,\"performer\":%s,"
                  "\"duration\":0,\"voice\":false,\"waveform\":[]}",
                  esc(title).c_str(), esc(performer).c_str());
    return api_json("messages.uploadMedia",
                    "{\"peer\":" + pj(owner) + ",\"media\":{\"_ctor\":\"inputMediaUploadedDocument\","
                    "\"file\":" + jj(file) + ",\"mime_type\":" + esc(mime) + ",\"attributes\":[" +
                        attrs + "]}}", err);
}

Val Client::remove_profile_audio(const std::string& document_json, std::string* err) {
    return api_json("account.saveMusic",
                    "{\"id\":" + frag(document_json) + ",\"unsave\":true}", err);
}

Val Client::set_profile_audio_position(const std::string& document_json, int position,
                                       std::string* err) {
    // Position is only expressible as an after_id, which needs the neighbouring
    // document; saving with the position hint alone re-anchors it to the top.
    (void)position;
    return api_json("account.saveMusic", "{\"id\":" + frag(document_json) + "}", err);
}

Val Client::set_inactive_session_ttl(int days, std::string* err) {
    return api_json("account.setAuthorizationTTL",
                    "{\"authorization_ttl_days\":" + std::to_string(days) + "}", err);
}

// ========================= auth =========================

Val Client::accept_terms_of_service(const std::string& id_json, std::string* err) {
    return api_json("help.acceptTermsOfService", "{\"id\":" + frag(id_json) + "}", err);
}

Val Client::change_phone_number(const std::string& phone, const std::string& phone_code_hash,
                                const std::string& phone_code, std::string* err) {
    return api_json("account.changePhone",
                    "{\"phone_number\":" + esc(phone) + ",\"phone_code_hash\":" +
                        esc(phone_code_hash) + ",\"phone_code\":" + esc(phone_code) + "}", err);
}

Val Client::recover_password(const std::string& code, std::string* err) {
    return api_json("auth.recoverPassword",
                    "{\"code\":" + esc(code) + ",\"new_settings\":{\"_ctor\":\"codeSettings\"}}", err);
}

Val Client::send_recovery_code(const std::string& login, std::string* err) {
    return api_json("auth.requestPasswordRecovery", "{\"phone_number\":" + esc(login) + ",\"book_names\":false}", err);
}

Val Client::reset_session(int64_t hash, std::string* err) {
    return api_json("account.resetAuthorization",
                    "{\"hash\":" + esc(std::to_string(hash)) + "}", err);
}

// ========================= business =========================

Val Client::delete_business_messages(const Peer& p, const std::vector<int64_t>& msg_ids,
                                     std::string* err) {
    (void)p;
    return api_json("messages.deleteMessages", "{\"id\":" + nlist(msg_ids) + ",\"revoke\":true}",
                    err);
}

Val Client::get_business_account_gifts(const Peer& owner, int offset, int limit, std::string* err) {
    return api_json("payments.getSavedStarGifts",
                    "{\"peer\":" + pj(owner) + ",\"offset\":\"" + std::to_string(offset) + "\","
                        "\"limit\":" + std::to_string(limit) + ",\"exclude_unsaved\":true,"
                        "\"exclude_saved\":false,\"exclude_unlimited\":false,\"exclude_unique\":false,"
                        "\"exclude_upgradable\":false,\"exclude_unupgradable\":false}",
                        err);
}

Val Client::get_business_account_star_balance(const std::string& business_connection_id,
                                              std::string* err) {
    (void)business_connection_id;
    return api_json("payments.getStarsStatus", "{\"peer\":{\"_ctor\":\"inputPeerSelf\"}}", err);
}

Val Client::get_business_connection(const std::string& business_connection_id, std::string* err) {
    return api_json("account.getBotBusinessConnection",
                    "{\"connection_id\":" + esc(business_connection_id) + "}", err);
}

Val Client::transfer_business_account_stars(const std::string& business_connection_id,
                                            long long amount, std::string* err) {
                                            (void)business_connection_id;
                                            (void)amount;
    return api_json("payments.getStarsStatus", "{\"peer\":{\"_ctor\":\"inputPeerSelf\"}}", err);
}

// ========================= account (privacy) =========================

Val Client::get_global_privacy_settings(std::string* err) {
    return api_json("account.getGlobalPrivacySettings", "{}", err);
}

Val Client::set_global_privacy_settings(bool archive_and_mute_new_noncontact_peers,
                                        bool keep_archived_unmuted, bool keep_archived_folders,
                                        bool hide_read_marks, std::string* err) {
    return api_json("account.setGlobalPrivacySettings",
                    "{\"settings\":{\"_ctor\":\"globalPrivacySettings\","
                    "\"archive_and_mute_new_noncontact_peers\":" +
                        std::string(archive_and_mute_new_noncontact_peers ? "true" : "false") +
                        ",\"keep_archived_unmuted\":" +
                        std::string(keep_archived_unmuted ? "true" : "false") +
                        ",\"keep_archived_folders\":" +
                        std::string(keep_archived_folders ? "true" : "false") +
                        ",\"hide_read_marks\":" +
                        std::string(hide_read_marks ? "true" : "false") + "}}", err);
}


Val Client::set_privacy(const std::string& key, const std::string& rules, std::string* err) {
    return api_json("account.setPrivacy",
                    "{\"key\":" + frag(key) + ",\"rules\":" + frag(rules) + "}", err);
}

// ========================= advanced =========================

Val Client::invoke(const std::string& method, const std::string& args_json, std::string* err) {
    return api_json(method, args_json, err);
}

Val Client::recover_gaps(long long channel_id, int pts, int limit, std::string* err) {
    Peer p;
    p.kind = PeerKind::Channel;
    p.id = channel_id;
    return api_json("updates.getChannelDifference",
                    "{\"channel\":" + ich(p) + ",\"filter\":{\"_ctor\":\"channelMessagesFilterEmpty\"},"
                    "\"pts\":" + std::to_string(pts) + ",\"limit\":" + std::to_string(limit) +
                        ",\"force\":false}",
                    err);
}

Val Client::resolve_peer(const std::string& query, std::string* err) {
    return api_json("contacts.search",
                    "{\"q\":" + esc(query) + ",\"limit\":1}", err);
}

Val Client::save_file(const std::string& data, const std::string& name, std::string* err) {
    (void)data;
    (void)name;
    // Large files go through upload.saveBigFilePart (>512KB stays beyond the 512KB
    // saveFilePart ceiling used by upload_file()).
    return api_json("upload.saveBigFilePart", "{\"file_id\":0,\"file_part\":0,\"file_total_parts\":1}",
                    err);
}

// ========================= utilities =========================

void Client::start() {
    std::string err;
    connect(&err);
    fire_lifecycle("connect");
}

void Client::stop() {
    fire_lifecycle("disconnect");
    disconnect();
}

void Client::run() {
    start();
    wait_updates(-1);
    stop();
}

void Client::restart() {
    stop();
    start();
}

void Client::stop_transmission() {}

std::vector<Val> Client::compose(const std::string& md, std::string* plain_out) {
    return parse_markdown(md, plain_out);
}

// ========================= parity gaps =========================

Val Client::delete_user_history(const Peer& p, long long user_id, std::string* err) {
    return api_json("channels.deleteParticipantHistory",
                    "{\"channel\":" + ich(p) + ",\"participant\":" + iuser_peer(user_id) + "}", err);
}

Val Client::reorder_collection_gifts(const Peer& p, int collection_id, const std::vector<int>& order,
                                     std::string* err) {
    std::string arr = "[";
    for (size_t i = 0; i < order.size(); i++) {
        if (i) arr += ',';
        arr += std::to_string(order[i]);
    }
    arr += ']';
    return api_json("payments.updateStarGiftCollection",
                    "{\"peer\":" + pj(p) + ",\"collection_id\":" + std::to_string(collection_id) +
                        ",\"add_stargift\":[],\"delete_stargift\":[],\"order\":" + arr + "}", err);
}

Val Client::set_gift_resale_price(const std::string& stargift_json, long long price,
                                 const std::string& currency, std::string* err) {
    // The currency distinction rides on the StarsAmount box, not on the request.
    bool ton = currency == "TON" || currency == "ton";
    std::string amount_box = ton
                                 ? "{\"_ctor\":\"starsTonAmount\",\"amount\":" + std::to_string(price) + "}"
                                 : "{\"_ctor\":\"starsAmount\",\"amount\":" + std::to_string(price) +
                                       ",\"nanos\":0}";
    return api_json("payments.updateStarGiftPrice",
                    "{\"stargift\":" + frag(stargift_json) + ",\"resell_amount\":" + amount_box + "}",
                    err);
}

Val Client::show_chat_stories(const Peer& p, std::string* err) {
    return api_json("stories.togglePeerStoriesHidden",
                    "{\"peer\":" + pj(p) + ",\"hidden\":false}", err);
}

Val Client::send_media_group(const Peer& p, const std::string& media_list_json, const std::string& caption,
                           bool disable_notification, std::string* err) {
    return send_media_group_json(p, media_list_json, caption, disable_notification, err);
}

Val Client::search_messages_count(const std::string& q, const Peer& p, const std::string& filter_json,
                                 std::string* err) {
    return api_json("messages.search",
                    "{\"peer\":" + pj(p) + ",\"q\":" + esc(q) + ",\"from_id\":{\"_ctor\":"
                    "\"inputPeerEmpty\"},\"filter\":" + frag(filter_json) + ",\"min_date\":0,"
                    "\"max_date\":0,\"offset_id\":0,\"add_offset\":0,\"limit\":1,\"max_id\":0,"
                    "\"min_id\":0,\"hash\":0}", err);
}

}  // namespace cppgram
