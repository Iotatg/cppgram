// pyrogram/filters.py parity: named filter factory functions + combinators.
//
// Mirrors kurigram's `filters` module (76 named filters, 3 combinators) one-to-one.
// Every factory returns a MessageFilter whose corresponding field is set; combine
// them with operator&& / operator|| / operator! exactly like pyrogram.
//
//   cg.on_message(filters::photo & filters::private, handler);
//   cg.on_message(filters::command("start"), handler);
//   cg.on_message(filters::regex(r"(?i)^hi") & filters::incoming, handler);

#pragma once

#include "cppgram/client.hpp"

#include <functional>
#include <string>
#include <vector>

namespace cppgram::filters {

// ---------------- direction / sender ----------------
inline MessageFilter incoming() { MessageFilter f; f.incoming_only = true; return f; }
inline MessageFilter outgoing() { MessageFilter f; f.outgoing_only = true; return f; }

// ---------------- scoped sets ----------------
inline MessageFilter me() { MessageFilter f; f.me_only = true; return f; }
inline MessageFilter bot() { MessageFilter f; f.via_bot_only = true; return f; }
inline MessageFilter sender_chat() { MessageFilter f; f.channel_only = true; return f; }

inline MessageFilter chat(std::vector<int64_t> ids) {
    MessageFilter f;
    f.chat_ids = std::move(ids);
    return f;
}
inline MessageFilter user(std::vector<int64_t> ids) {
    MessageFilter f;
    f.user_ids = std::move(ids);
    return f;
}

// ---------------- text ----------------
inline MessageFilter all() { return MessageFilter{}; }
inline MessageFilter text()      { MessageFilter f; f.text_only = true; return f; }
inline MessageFilter reply()     { MessageFilter f; f.reply_only = true; return f; }
inline MessageFilter forward()   { MessageFilter f; f.forwarded_only = true; return f; }
inline MessageFilter caption()   { MessageFilter f; f.caption_only = true; return f; }
inline MessageFilter quote()     { MessageFilter f; f.quote_only = true; return f; }
inline MessageFilter media_spoiler() { MessageFilter f; f.media_spoiler_only = true; return f; }
inline MessageFilter self_destruction() { MessageFilter f; f.self_destructing_only = true; return f; }
inline MessageFilter media_group() { MessageFilter f; f.media_group_only = true; return f; }
inline MessageFilter scheduled() { MessageFilter f; f.scheduled_only = true; return f; }
inline MessageFilter from_scheduled() { return scheduled(); }
inline MessageFilter paid_message() { MessageFilter f; f.paid_message_only = true; return f; }
inline MessageFilter business()   { MessageFilter f; f.business_only = true; return f; }
inline MessageFilter linked_channel() { MessageFilter f; f.linked_channel_only = true; return f; }
inline MessageFilter via_bot()    { MessageFilter f; f.via_bot_only = true; return f; }
inline MessageFilter mentioned()  { MessageFilter f; f.mentioned_only = true; return f; }
inline MessageFilter service()    { MessageFilter f; f.service_only = true; return f; }

// ---------------- media ----------------
inline MessageFilter media()     { MessageFilter f; f.media_only = true; return f; }
inline MessageFilter photo()     { MessageFilter f; f.photo_only = true; return f; }
inline MessageFilter sticker()   { MessageFilter f; f.sticker_only = true; return f; }
inline MessageFilter animation() { MessageFilter f; f.animation_only = true; return f; }
inline MessageFilter video()     { MessageFilter f; f.video_only = true; return f; }
inline MessageFilter video_note(){ MessageFilter f; f.video_note_only = true; return f; }
inline MessageFilter voice()     { MessageFilter f; f.voice_only = true; return f; }
inline MessageFilter audio()     { MessageFilter f; f.audio_only = true; return f; }
inline MessageFilter document()  { MessageFilter f; f.document_only = true; return f; }
inline MessageFilter contact()   { MessageFilter f; f.contact_only = true; return f; }
inline MessageFilter location()  { MessageFilter f; f.location_only = true; return f; }
inline MessageFilter live_location() { MessageFilter f; f.live_location_only = true; return f; }
inline MessageFilter venue()     { MessageFilter f; f.venue_only = true; return f; }
inline MessageFilter web_page()  { MessageFilter f; f.web_page_only = true; return f; }
inline MessageFilter poll()      { MessageFilter f; f.poll_only = true; return f; }
inline MessageFilter dice()      { MessageFilter f; f.dice_only = true; return f; }
inline MessageFilter game()      { MessageFilter f; f.game_only = true; return f; }
inline MessageFilter story()     { MessageFilter f; f.story_only = true; return f; }
inline MessageFilter giveaway()  { MessageFilter f; f.giveaway_only = true; return f; }
inline MessageFilter giveaway_winners() { MessageFilter f; f.giveaway_winners_only = true; return f; }
inline MessageFilter gift_code() { MessageFilter f; f.gift_code_only = true; return f; }
inline MessageFilter gift()      { MessageFilter f; f.gift_only = true; return f; }
inline MessageFilter users_shared() { MessageFilter f; f.users_shared_only = true; return f; }
inline MessageFilter chat_shared()  { MessageFilter f; f.chat_shared_only = true; return f; }
inline MessageFilter successful_payment() { MessageFilter f; f.invoice_success_only = true; return f; }

// ---------------- chat type ----------------
inline MessageFilter private_() { MessageFilter f; f.private_only = true; return f; }
inline MessageFilter group()   { MessageFilter f; f.group_only = true; return f; }
inline MessageFilter channel() { MessageFilter f; f.channel_only = true; return f; }
inline MessageFilter direct()  { MessageFilter f; f.direct_only = true; return f; }
inline MessageFilter forum()   { MessageFilter f; f.forum_only = true; return f; }

// ---------------- service kinds ----------------
inline MessageFilter new_chat_members()   { MessageFilter f; f.new_chat_members_only = true; return f; }
inline MessageFilter left_chat_member()   { MessageFilter f; f.left_chat_member_only = true; return f; }
inline MessageFilter new_chat_title()     { MessageFilter f; f.new_chat_title_only = true; return f; }
inline MessageFilter new_chat_photo()     { MessageFilter f; f.new_chat_photo_only = true; return f; }
inline MessageFilter delete_chat_photo()  { MessageFilter f; f.delete_chat_photo_only = true; return f; }
inline MessageFilter group_chat_created()    { MessageFilter f; f.group_created_only = true; return f; }
inline MessageFilter supergroup_chat_created(){ MessageFilter f; f.supergroup_created_only = true; return f; }
inline MessageFilter channel_chat_created()  { MessageFilter f; f.channel_created_only = true; return f; }
inline MessageFilter migrate_to_chat_id()   { MessageFilter f; f.migrate_to_chat_only = true; return f; }
inline MessageFilter migrate_from_chat_id() { MessageFilter f; f.migrate_from_chat_only = true; return f; }
inline MessageFilter pinned_message()   { MessageFilter f; f.pinned_message_only = true; return f; }
inline MessageFilter game_high_score()  { MessageFilter f; f.game_high_score_only = true; return f; }
inline MessageFilter video_chat_started(){ MessageFilter f; f.video_chat_started_only = true; return f; }
inline MessageFilter video_chat_ended()  { MessageFilter f; f.video_chat_ended_only = true; return f; }
inline MessageFilter video_chat_members_invited() {
    MessageFilter f;
    f.video_chat_members_invited_only = true;
    return f;
}

// ---------------- keyboards / admin ----------------
inline MessageFilter reply_keyboard() { MessageFilter f; f.reply_keyboard_only = true; return f; }
inline MessageFilter inline_keyboard(){ MessageFilter f; f.inline_keyboard_only = true; return f; }
inline MessageFilter admin() { MessageFilter f; f.admin_only = true; return f; }

// ---------------- custom / combinators ----------------
inline MessageFilter custom(std::function<bool(const Message&)> fn) {
    MessageFilter f;
    f.pred = std::move(fn);
    return f;
}

// filters.command("start", "/") — also accepts "/start@botname".
inline MessageFilter command(const std::string& name, const std::string& pfx = "/") {
    MessageFilter f;
    f.command = name;
    f.command_prefix = pfx;
    return f;
}

// filters.regex(r"(?i)pattern")
inline MessageFilter regex(const std::string& pattern) {
    MessageFilter f;
    f.regex_pattern = pattern;
    return f;
}

// pyrogram create(): run an arbitrary predicate over the decoded message.
inline MessageFilter create(std::function<bool(const Message&)> fn) { return custom(std::move(fn)); }

}  // namespace cppgram::filters

// ---------------- pyrogram-style logical operators ----------------
namespace cppgram {

inline MessageFilter operator&&(const MessageFilter& a, const MessageFilter& b) {
    MessageFilter f;
    f.all_of = {a, b};
    return f;
}

inline MessageFilter operator||(const MessageFilter& a, const MessageFilter& b) {
    MessageFilter f;
    f.any_of = {a, b};
    return f;
}

inline MessageFilter operator!(const MessageFilter& a) {
    MessageFilter f;
    f.negate = true;
    f.all_of = {a};
    return f;
}

}  // namespace cppgram
