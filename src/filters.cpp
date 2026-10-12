// pyrogram/filters.py parity layer: MessageFilter::matches() plus the `filters::`
// factory helpers. Every named filter in kurigram maps to one field in the struct
// in client.hpp; `matches()` evaluates them in a single pass over the decoded message.

#include "cppgram/client.hpp"

#include <algorithm>
#include <regex>
#include <string>
#include <vector>

namespace cppgram {

namespace {

std::string media_ctor(const Message& m) {
    return m.raw.has("media") ? m.raw.at("media").name : std::string();
}

std::string action_ctor(const Message& m) {
    return m.raw.has("action") ? m.raw.at("action").name : std::string();
}

bool truthy(const Message& m, const char* key) {
    return m.raw.has(key) && m.raw.num(key) != 0;
}

bool has_key(const Message& m, const char* key) { return m.raw.has(key); }

// pyrogram.classifiers.document_document_attribute / media type of a
// messageMediaDocument: "document", "sticker", "animation", "voice", "audio",
// "video" or "video_note" (walk the decoded `attributes` vector).
std::string document_kind(const Message& m) {
    if (!m.raw.has("media")) return "document";
    const Val& media = m.raw.at("media");
    const Val& doc = media.has("document") ? media.at("document") : media;
    for (const auto& a : doc.list("attributes")) {
        const std::string& n = a.name;
        if (n == "documentAttributeSticker") return "sticker";
        if (n == "documentAttributeVideo") {
            if (a.num("round_message")) return "video_note";
            if (a.num("nosound")) return "animation";
            return "video";
        }
        if (n == "documentAttributeAudio") return a.num("voice") ? "voice" : "audio";
        if (n == "documentAttributeAnimated") return "animation";
    }
    return "document";
}

// pyrogram.chat_type_of(): marked peer id -> enum name.
//   > 0                 user (private or bot)
//   <= -1e12            channel / supergroup / forum
//   otherwise           basic group
std::string chat_type_of(const Message& m) {
    const int64_t id = m.chat_id;
    if (id > 0) return truthy(m, "via_bot_id") ? "bot" : "private";
    if (id <= -1000000000000LL) return "channel";
    return "group";
}

bool starts_with(const std::string& text, const std::string& p) {
    return p.empty() || text.compare(0, p.size(), p) == 0;
}

bool has_substr(const std::string& text, const std::string& s) {
    return s.empty() || text.find(s) != std::string::npos;
}

// pyrogram.filters.command: text is "/cmd", "/cmd@bot" or "/cmd <args>".
bool matches_command(const Message& m, const std::string& cmd, const std::string& pfx) {
    const std::string body = pfx + cmd;
    if (!starts_with(m.text, body)) return false;
    const size_t i = body.size();
    if (i >= m.text.size()) return true;
    const char next = m.text[i];
    return next == ' ' || next == '@' || next == '\n';
}

}  // namespace

bool MessageFilter::matches_base(const Message& m) const {
    const std::string media = media_ctor(m);
    const std::string action = action_ctor(m);

    // ---------------- scoping ----------------
    if (chat_id && m.chat_id != chat_id) return false;
    if (!chat_ids.empty() && std::find(chat_ids.begin(), chat_ids.end(), m.chat_id) == chat_ids.end())
        return false;
    if (from_user_id && m.from_id != from_user_id) return false;
    if (!user_ids.empty() && std::find(user_ids.begin(), user_ids.end(), m.from_id) == user_ids.end())
        return false;
    if (me_only && m.from_id != me_id) return false;

    // ---------------- text ----------------
    if (!prefix.empty() && !starts_with(m.text, prefix)) return false;
    if (!contains.empty() && !has_substr(m.text, contains)) return false;
    if (text_only && m.text.empty()) return false;
    if (reply_only && !(has_key(m, "reply_to") || truthy(m, "reply_to"))) return false;
    if (caption_only && !(has_key(m, "media") && !m.raw.at("media").text("caption").empty()))
        return false;
    if (forwarded_only && !has_key(m, "fwd_from")) return false;
    if (via_bot_only && !truthy(m, "via_bot_id")) return false;
    if (mentioned_only && !truthy(m, "mentioned")) return false;
    if (quote_only && !has_key(m, "quote")) return false;
    if (media_spoiler_only && !truthy(m, "media_spoiler")) return false;
    if (self_destructing_only && !truthy(m, "ttl_period")) return false;
    if (scheduled_only && !truthy(m, "from_scheduled")) return false;
    if (paid_message_only && !truthy(m, "paid_messages_stars")) return false;
    if (reply_keyboard_only && !has_key(m, "reply_markup")) return false;
    if (inline_keyboard_only && !has_key(m, "reply_markup")) return false;

    // ---------------- media ----------------
    const bool has_media = !media.empty() && media != "messageMediaEmpty";
    if (media_only && !has_media) return false;
    if (media_group_only && !truthy(m, "grouped_id")) return false;
    if (story_sticker_only && media != "messageMediaUnsupported") return false;
    if (photo_only && media != "messageMediaPhoto") return false;
    if (web_page_only && media != "messageMediaWebPage") return false;
    if (contact_only && media != "messageMediaContact") return false;
    if (venue_only && media != "messageMediaVenue") return false;
    if (poll_only && media != "messageMediaPoll") return false;
    if (dice_only && media != "messageMediaDice") return false;
    if (game_only && media != "messageMediaGame") return false;
    if (giveaway_only && media != "messageMediaGiveaway") return false;
    if (giveaway_winners_only && media != "messageMediaGiveawayResults") return false;
    if (gift_code_only && media != "messageMediaGiveaway") return false;
    if (gift_only && (media != "messageMediaGift" && media != "messageMediaGiftBox")) return false;
    if (location_only && media != "messageMediaGeo" && media != "messageMediaGeoLive") return false;
    if (live_location_only && media != "messageMediaGeoLive") return false;
    if (story_only && media != "messageMediaStory") return false;
    if (invoice_success_only && !has_key(m, "action")) return false;
    if (users_shared_only && action != "messageActionRequestedPeerSentMe") return false;
    if (chat_shared_only && action != "messageActionRequestedPeer") return false;
    if (media == "messageMediaDocument") {
        const std::string kind = document_kind(m);
        if (sticker_only && kind != "sticker") return false;
        if (animation_only && kind != "animation") return false;
        if (video_only && kind != "video") return false;
        if (video_note_only && kind != "video_note") return false;
        if (voice_only && kind != "voice") return false;
        if (audio_only && kind != "audio") return false;
        if (document_only && kind != "document") return false;
    } else {
        if (sticker_only || animation_only || video_only || video_note_only || voice_only ||
            audio_only || document_only)
            return false;
    }

    // ---------------- service messages ----------------
    const bool is_service = !action.empty();
    if (service_only && !is_service) return false;
    if (new_chat_members_only && action != "messageActionChatAddUser") return false;
    if (left_chat_member_only && action != "messageActionChatDeleteUser") return false;
    if (new_chat_title_only && action != "messageActionChatEditTitle") return false;
    if (new_chat_photo_only && action != "messageActionChatEditPhoto") return false;
    if (delete_chat_photo_only && action != "messageActionChatDeletePhoto") return false;
    if (group_created_only && action != "messageActionChatCreate") return false;
    if (supergroup_created_only &&
        (action != "messageActionChatCreate" && action != "messageActionChatMigrateTo"))
        return false;
    if (channel_created_only && action != "messageActionChannelCreate") return false;
    if (migrate_to_chat_only && action != "messageActionChatMigrateTo") return false;
    if (migrate_from_chat_only && action != "messageActionChannelMigrateFrom") return false;
    if (pinned_message_only && action != "messageActionPinMessage") return false;
    if (game_high_score_only && action != "messageActionGameScore") return false;
    if (video_chat_started_only && action != "messageActionGroupCallScheduled") return false;
    if (video_chat_ended_only && action != "messageActionGroupCallEnded") return false;
    if (video_chat_members_invited_only && action != "messageActionInviteToGroupCall") return false;
    if (business_only && !has_key(m, "business_connection_id")) return false;
    if (linked_channel_only && !has_key(m, "replies")) return false;
    if (gift_offer_only && action != "messageActionStarGift") return false;
    if (gift_offer_accepted_only && action != "messageActionStarGift") return false;
    if (gift_offer_rejected_only && action != "messageActionStarGiftUnique") return false;

    // ---------------- chat type ----------------
    const std::string ctype = chat_type_of(m);
    if (private_only && !(ctype == "private" || ctype == "bot")) return false;
    if (group_only && !(ctype == "group" || ctype == "supergroup")) return false;
    if (channel_only && !(ctype == "channel" || ctype == "supergroup")) return false;
    if (forum_only && ctype != "supergroup") return false;
    if (direct_only && ctype != "channel") return false;

    // ---------------- direction ----------------
    if (incoming_only && m.incoming != true) return false;
    if (outgoing_only && m.incoming != false) return false;
    if (outgoing && m.incoming) return false;

    // ---------------- command / regex ----------------
    if (!command.empty() &&
        !matches_command(m, command, command_prefix.empty() ? "/" : command_prefix))
        return false;
    if (!regex_pattern.empty()) {
        try {
            const std::regex re(regex_pattern, std::regex::ECMAScript | std::regex::icase);
            if (!std::regex_search(m.text, re)) return false;
        } catch (const std::regex_error&) {
            return false;
        }
    }

    // ---------------- custom / combinators ----------------
    if (pred && !pred(m)) return false;
    for (const auto& f : all_of)
        if (!f.matches(m)) return false;
    bool any_match = any_of.empty();
    for (const auto& f : any_of)
        if (f.matches(m)) {
            any_match = true;
            break;
        }
    if (!any_match) return false;
    return true;
}

}  // namespace cppgram
