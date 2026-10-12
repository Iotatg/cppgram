#pragma once

#include <chrono>
#include <cstdint>
#include <functional>

#include "cppgram/args.hpp"
#include "cppgram/handlers.hpp"

#include <vector>
#include "cppgram/types.hpp"
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cppgram/tl.hpp"

namespace cppgram {

inline int64_t unix_now() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}
inline int64_t mono_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// Authorized session obtained from a userbot string session (pyrogram or gogram format).
struct Session {
    int dc_id{0};
    std::string ip;  // literal address, no port
    int port{443};
    std::string auth_key;  // 256 raw bytes
    int32_t api_id{0};
    int64_t user_id{0};
};

// Server-side updates that the assistant engine cares about (decoded from RPC + push updates).
struct UpdateEvent {
    std::string kind;  // "signaling", "connection_state", "participants", ...
    int64_t group_call_id{0};
    int64_t chat_id{0};
    std::string payload;  // kind-specific (base64 / json / raw bytes)
};

// A newly-arrived message, decoded with the full schema engine.
struct Message {
    int64_t id{0};
    int64_t chat_id{0};   // marked peer id (channels negative)
    int64_t from_id{0};
    std::string text;
    std::string from_name;
    int64_t date{0};
    bool incoming{false};
    Val raw;              // full decoded message object
};

// Filter for Message handlers (pyrogram filters.py parity).
//
// The field set mirrors pyrogram's 76 named filters one-to-one: every field maps to a
// "message.<field>" truthiness test or a chat-type / direction check. Empty/zero fields
// are "match all", matching pyrogram's Filter.__call__ semantics.
//
// The original chat_id / from_user_id / prefix / contains stay on the left so existing
// call sites compile unchanged. Media tests look at the decoded `media` constructor
// (messageMediaPhoto, messageMediaSticker, ...) in Message::raw; service tests look at
// the message `action` constructor (messageActionChatAddUser, ...).
struct MessageFilter {
    int64_t chat_id{0};        // 0 = any chat (note: never matches broadcast 0)
    int64_t from_user_id{0};   // 0 = any sender
    std::string prefix;        // must start with text
    std::string contains;      // must contain substring
    bool incoming_only{false};
    bool outgoing_only{false};
    bool outgoing{false};      // required state when incoming_only/outgoing set

    // pyrogram.filters: scoped sets of chats / senders
    std::vector<int64_t> chat_ids;        // filters.chat(a, b, ...)
    std::vector<int64_t> user_ids;        // filters.user(a, b, ...) / me / self
    std::vector<std::string> chat_usernames;
    std::vector<std::string> user_usernames;
    bool me_only{false};
    int64_t me_id{0};                     // set by the client after users.getUsers

    // pyrogram.filters: text side
    bool text_only{false};         // message.text non-empty
    bool reply_only{false};        // reply_to_message_id or reply_to_story_id
    bool caption_only{false};      // message.caption non-empty
    bool forwarded_only{false};    // forward_origin present
    bool via_bot_only{false};      // via_bot_id present
    bool mentioned_only{false};    // mentioned flag
    bool quote_only{false};        // quote non-empty
    bool media_spoiler_only{false};
    bool media_group_only{false};  // groups album messages
    bool self_destructing_only{false};
    bool scheduled_only{false};    // from_scheduled flag
    bool paid_message_only{false};

    // pyrogram.filters: media presence
    bool photo_only{false};
    bool sticker_only{false};
    bool animation_only{false};
    bool audio_only{false};
    bool document_only{false};
    bool video_only{false};
    bool voice_only{false};
    bool video_note_only{false};
    bool contact_only{false};
    bool location_only{false};
    bool live_location_only{false};
    bool venue_only{false};
    bool web_page_only{false};
    bool poll_only{false};
    bool dice_only{false};
    bool game_only{false};
    bool story_only{false};
    bool giveaway_only{false};
    bool giveaway_winners_only{false};
    bool gift_code_only{false};
    bool gift_only{false};
    bool users_shared_only{false};
    bool chat_shared_only{false};
    bool invoice_success_only{false};   // successful_payment
    bool media_only{false};             // any media
    bool service_only{false};           // action present
    bool story_sticker_only{false};

    // pyrogram.filters: chat type
    bool private_only{false};
    bool group_only{false};
    bool channel_only{false};
    bool direct_only{false};
    bool forum_only{false};

    // pyrogram.filters: service-message kinds
    bool new_chat_members_only{false};
    bool left_chat_member_only{false};
    bool new_chat_title_only{false};
    bool new_chat_photo_only{false};
    bool delete_chat_photo_only{false};
    bool group_created_only{false};
    bool supergroup_created_only{false};
    bool channel_created_only{false};
    bool migrate_to_chat_only{false};
    bool migrate_from_chat_only{false};
    bool pinned_message_only{false};
    bool game_high_score_only{false};
    bool video_chat_started_only{false};
    bool video_chat_ended_only{false};
    bool video_chat_members_invited_only{false};
    bool business_only{false};
    bool linked_channel_only{false};
    bool gift_offer_only{false};
    bool gift_offer_accepted_only{false};
    bool gift_offer_rejected_only{false};

    // pyrogram.filters: keyboard markup
    bool reply_keyboard_only{false};
    bool inline_keyboard_only{false};

    // pyrogram.filters: admin / command / regex
    bool admin_only{false};
    std::string command;           // filters.command("start") — bot-prefix match
    std::string command_prefix;    // default "/"
    std::string regex_pattern;    // filters.regex(r"...")

    // pyrogram.filters: combinators (custom / all_of / any_of / invert)
    std::function<bool(const Message&)> pred;
    std::vector<MessageFilter> all_of;
    std::vector<MessageFilter> any_of;
    bool negate{false};

    // Positive evaluation of every field (no inversion). Public `matches()` applies
    // the pyrogram `operator!` semantics on top of this.
    bool matches_base(const Message& m) const;

    bool matches(const Message& m) const { return negate ? !matches_base(m) : matches_base(m); }
};

struct ClientConfig {
    std::string app_version{"AdvaitMusic"};
    std::string device_model{"Linux x86_64"};
    std::string system_version{"Linux"};
    std::string lang_code{"en"};
};

// Minimal MTProto user client: enough to authenticate with an imported auth key,
// stay initialized, and run group-call phone.* methods.
class Client {
public:
    Client(Session session, ClientConfig cfg);
    ~Client();
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    // TCP connect + initConnection handshake. Idempotent.
    bool connect(std::string* err);
    void disconnect();
    bool connected() const { return fd_ >= 0 && initialized_; }

    // Serialized TL request -> serialized TL response object (rpc_result body).
    std::string call_rpc(const std::string& serialized_request, std::string* err, bool wrap_init = false,
                         bool match_pong = false);

    // Non-blocking: drains socket, dispatches pushed updates to the handler.
    void poll_updates();
    void set_update_handler(std::function<void(const UpdateEvent&)> h);

    // --- pyrogram-style message handlers ---
    using MessageHandler = std::function<void(const Message&)>;
    // Register a handler; returns a handle you can pass to off_message().
    int on_message(MessageFilter f, MessageHandler fn);
    void off_message(int handle);
    // Pump updates for up to `seconds` (drains pushed updates + difference),
    // dispatching Message events to registered handlers. Returns count.
    int wait_updates(int seconds, bool with_difference = true);
    // Low-level: dispatch one UpdateEvent through message handlers.
    void dispatch_message_event(const UpdateEvent& ev);

    // --- pyrogram-style typed update handlers (all optional; 0 = all) ---
    using EditedMessageHandler = std::function<void(const EditedMessageEvent&)>;
    using DeletedMessagesHandler = std::function<void(const DeletedMessagesEvent&)>;
    using CallbackQueryHandler = std::function<void(const CallbackQuery&)>;
    using MessageReactionHandler = std::function<void(const MessageReactionEvent&)>;
    using MessageReactionCountHandler = std::function<void(const MessageReactionCountUpdate&)>;
    using ChatMemberHandler = std::function<void(const ChatMemberUpdated&)>;
    using ChatJoinRequestHandler = std::function<void(const ChatJoinRequest&)>;
    using PollHandler = std::function<void(const PollUpdate&)>;
    using UserStatusHandler = std::function<void(const UserStatusEvent&)>;
    using TypingHandler = std::function<void(const TypingEvent&)>;
    using StoryHandler = std::function<void(const StoryUpdate&)>;
    using InlineQueryHandler = std::function<void(const InlineQueryEvent&)>;
    using ChosenInlineResultHandler = std::function<void(const ChosenInlineResultEvent&)>;
    using RawUpdateHandler = std::function<void(const RawUpdateEvent&)>;
    using VoidHandler = std::function<void()>;
    using ErrorHandler = std::function<void(int code, const std::string& message)>;
    using PreCheckoutQueryHandler = std::function<void(const cppgram::PreCheckoutQuery&)>;
    using ShippingQueryHandler = std::function<void(const cppgram::ShippingQuery&)>;
    using PurchasedPaidMediaHandler = std::function<void(const cppgram::PurchasedPaidMediaEvent&)>;
    using BusinessConnectionHandler = std::function<void(const cppgram::BusinessConnectionEvent&)>;
    using ManagedBotHandler = std::function<void(const cppgram::ManagedBotEvent&)>;
    using StoppedMessageGenerationHandler =
        std::function<void(const cppgram::StoppedMessageGenerationEvent&)>;

    int on_edited_message(MessageFilter f, EditedMessageHandler fn);
    int on_deleted_messages(DeletedMessagesHandler fn);
    int on_callback_query(CallbackQueryHandler fn);
    int on_message_reaction(MessageReactionHandler fn);
    int on_chat_member_updated(ChatMemberHandler fn);
    int on_chat_join_request(ChatJoinRequestHandler fn);
    int on_poll(PollHandler fn);
    int on_user_status(UserStatusHandler fn);
    int on_typing(TypingHandler fn);
    int on_story(StoryHandler fn);
    int on_inline_query(InlineQueryHandler fn);
    int on_chosen_inline_result(ChosenInlineResultHandler fn);
    int on_raw_update(RawUpdateHandler fn);
    int on_error(ErrorHandler fn);
    int on_connect(VoidHandler fn);
    int on_disconnect(VoidHandler fn);

    int on_chat_boost(RawUpdateHandler fn);
    int on_message_reaction_count(MessageReactionCountHandler fn);
    int on_pre_checkout_query(PreCheckoutQueryHandler fn);
    int on_shipping_query(ShippingQueryHandler fn);
    int on_purchased_paid_media(PurchasedPaidMediaHandler fn);
    int on_business_connection(BusinessConnectionHandler fn);
    int on_deleted_business_messages(RawUpdateHandler fn);
    int on_edited_business_message(RawUpdateHandler fn);
    int on_managed_bot(ManagedBotHandler fn);
    int on_guest_message(MessageHandler fn);
    int on_stopped_message_generation(StoppedMessageGenerationHandler fn);
    int on_business_message(RawUpdateHandler fn);
    int on_start(VoidHandler fn);
    int on_stop(VoidHandler fn);
    void off_handler(const std::string& kind, int handle);
    // Feed one decoded Updates object (or a bare Update) into every typed handler.
    void dispatch_update_obj(const Val& updates_root);
    // True once at least one handler of the given kind is registered.
    bool has_handler(const std::string& kind) const;


    // ---- group voice calls ----
    struct GroupCallRef {
        int64_t id{0};
        int64_t access_hash{0};
    };
    // Resolve the currently active group call of a superchannel (via channels.getFullChannel).
    bool get_group_call(int64_t channel_id, GroupCallRef* out, std::string* err);
    // Access hash of our own account (users.getUsers(inputUserSelf)), cached after first call.
    bool self_access_hash(int64_t* out, std::string* err);
    // Join an active group call; on success *server_params receives updateGroupCallConnection params JSON.
    bool join_group_call(const GroupCallRef&, const std::string& params_json, std::string* server_params,
                         std::string* err);

    // ---- high-level messaging (user account) ----
    // Peer cache is filled by refresh_dialog_peers()/get_dialogs() and by any
    // resolved channel. Users need a cached access hash (dialog refresh covers
    // everything the account has ever talked to).
    void cache_peer(const Peer& p);
    bool peer_for(int64_t marked_chat_id, Peer* out, std::string* err);
    bool refresh_dialog_peers(int limit, std::string* err);
    std::vector<DialogInfo> get_dialogs(int limit, std::string* err);
    // Sends plain text; returns sent message id (0 = unknown but sent, -1 = error).
    int64_t send_message_text(const Peer&, const std::string& text, int64_t reply_to_msg_id, std::string* err);
    bool edit_message_text(const Peer&, int64_t message_id, const std::string& text, std::string* err);
    bool delete_messages(const Peer&, const std::vector<int64_t>& message_ids, bool revoke, std::string* err);
    bool set_typing(const Peer&, std::string* err);
    bool read_history(const Peer&, int64_t max_id, std::string* err);
    std::vector<MessageInfo> get_history(const Peer&, int limit, std::string* err);
    // One non-blocking difference round; new/edited/deleted messages are pushed
    // through the update handler as UpdateEvent{kind="message"} payload json.
    void poll_difference();

    // --- authentication (auth.* flows on a freshly generated key) ---
    // auth.sendCode; returns the auth.sentCode tree (phone_code_hash inside).
    Val auth_send_code(const std::string& phone, int32_t api_id, const std::string& api_hash,
                       std::string* err);
    // auth.signIn / auth.signUp; fills user_id in session on success.
    bool auth_sign_in(const std::string& phone, const std::string& code_hash,
                      const std::string& code, std::string* err);
    bool auth_sign_up(const std::string& phone, const std::string& code_hash,
                      const std::string& first_name, const std::string& last_name, std::string* err);
    bool auth_resend_code(const std::string& phone, const std::string& code_hash, std::string* err);
    bool auth_cancel_code(const std::string& phone, std::string* err);
    // 2FA: SRP check via account.getPassword (kurigram compute_password_check).
    bool auth_check_password(const std::string& password, std::string* err);
    bool auth_log_out(std::string* err);

    // --- pyrogram-style high-level API (thin sugar over api()) ---
    Val get_me(std::string* err);
    Val get_users(const std::vector<long long>& ids, std::string* err);
    Val resolve_username(const std::string& username, std::string* err);
    Val get_dialogs_full(int limit, std::string* err);
    Val get_chat(const Peer& p, bool full, std::string* err);
    bool join_chat(const Peer& p, std::string* err);
    bool leave_chat(const Peer& p, std::string* err);
    Val get_chat_members(const Peer& p, int offset, int limit, std::string* err);
    bool ban_chat_member(const Peer& chat, long long user_id, bool revoke, std::string* err);
    bool unban_chat_member(const Peer& chat, long long user_id, std::string* err);
    bool promote_chat_member(const Peer& chat, long long user_id, bool admin, std::string* err);
    Val fetch_messages(const Peer& p, const std::vector<long long>& ids, std::string* err);
    Val fetch_history_full(const Peer& p, int limit, long long offset_id, std::string* err);
    bool forward_messages(const Peer& from, const std::vector<long long>& ids, const Peer& to,
                          bool silent, std::string* err);
    bool pin_message(const Peer& p, long long msg_id, bool unpin, std::string* err);
    bool set_reaction(const Peer& p, long long msg_id, const std::string& emoji, std::string* err);

    // --- media transfer ---
    // Upload raw bytes in 512KB parts (upload.saveFilePart) and return an
    // InputFile node ready for inputMedia* wrappers.
    Val upload_file(const std::string& data, const std::string& name, std::string* err);
    // Upload a JPEG and send it as a photo message; returns Updates tree.
    Val send_photo_message(const Peer& p, const std::string& jpeg, const std::string& name,
                           const std::string& caption, std::string* err);
    // Generic upload.getFile download loop into `path`.
    bool download_location(const Val& input_file_location, const std::string& path,
                           std::string* err);
    // Download first (largest) profile photo of a user into `path`.
    bool download_user_photo(long long user_id, const std::string& path, std::string* err);
    // Upload bytes as a document and send it as a message; returns Updates tree.
    Val send_document_message(const Peer& p, const std::string& data, const std::string& file_name,
                              const std::string& caption, bool force_file, std::string* err);
    // Generic: upload + send with explicit mime and DocumentAttribute list.
    Val send_document_media(const Peer& p, const std::string& data, const std::string& name,
                            const std::string& mime, std::vector<Val> attributes, bool force_file,
                            const std::string& caption, std::string* err);
    // Voice note (OGG/Opus) as a voice message.
    Val send_voice_message(const Peer& p, const std::string& ogg, double duration_sec,
                           const std::string& caption, std::string* err);
    // MP4 with video attributes (rendered as a video message, streams ready).
    Val send_video_message(const Peer& p, const std::string& mp4, const std::string& file_name,
                           const std::string& caption, double duration, int w, int h,
                           std::string* err);
    // Download the document attached to a message into `path`.
    bool download_document_msg(const Peer& p, long long msg_id, const std::string& path,
                               std::string* err);
    Val invoke_raw(const std::string& method, const std::string& args_json, std::string* err);
    std::string input_channel_json(const Peer& p);
    void cache_peer_from_node(const Val& node);

    // --- markdown / media extras ---
    // Parse a markdown subset (**bold**, *italic*, __underline__, ~~strike~~,
    // `code`, ```pre```, [text](url)) into plain text + messageEntity list.
    // Sends an entity-annotated message.
    Val send_markdown(const Peer& p, const std::string& md, int64_t reply_to_msg_id,
                      std::string* err);
    // Download the largest photo/document/video attached to a message into path.
    Val download_media_msg(const Peer& p, int64_t msg_id, const std::string& path,
                           std::string* err);
    // messages.searchGlobal wrapper.
    Val search_messages(const std::string& query, int limit, std::string* err);
    // Peer cache (access_hash store) persistence; JSON {marked_id: inputPeer}.
    bool save_peer_cache(const std::string& path, std::string* err);
    bool load_peer_cache(const std::string& path, std::string* err);
    // Session string serialization (gogram json format / pyrogram 271-byte).
    std::string session_string(bool pyrogram_format = false) const;
    // Encodes just the 271-byte pyrogram layout; split out so the codec can be
    // tested without a live connection.
    static std::string pyrogram_session_string(const Session& ses);
    void set_user_id(int64_t uid) { session_.user_id = uid; }

    // --- kurigram-parity wrappers (thin api() sugar) ---
    Val set_chat_title(const Peer& p, const std::string& title, std::string* err);
    Val set_chat_description(const Peer& p, const std::string& about, std::string* err);
    Val set_chat_username(const Peer& p, const std::string& username, std::string* err);
    Val set_slow_mode(const Peer& p, int seconds, std::string* err);
    Val set_chat_ttl(const Peer& p, int period, std::string* err);
    Val set_chat_protected_content(const Peer& p, bool enabled, std::string* err);
    Val archive_chats(const std::vector<Peer>& chats, bool archive, std::string* err);
    Val mark_chat_unread(const Peer& p, bool unread, std::string* err);
    Val create_channel(const std::string& title, const std::string& about, bool megagroup, std::string* err);
    Val create_group(const std::string& title, const std::vector<long long>& user_ids, std::string* err);
    Val delete_channel(const Peer& p, std::string* err);
    Val get_chat_member(const Peer& chat, long long user_id, std::string* err);
    Val get_chat_members_count(const Peer& p, std::string* err);
    Val get_dialogs_count(std::string* err);
    Val restrict_chat_member(const Peer& chat, long long user_id, bool view_messages, std::string* err);
    Val unpin_all_chat_messages(const Peer& p, std::string* err);
    Val set_administrator_title(const Peer& chat, long long user_id, const std::string& title, std::string* err);
    Val get_chat_event_log(const Peer& p, int limit, std::string* err);
    Val create_forum_topic(const Peer& p, const std::string& title, std::string* err);
    Val edit_forum_topic(const Peer& p, int topic_id, const std::string& title, std::string* err);
    Val close_forum_topic(const Peer& p, int topic_id, bool closed, std::string* err);
    Val delete_forum_topic(const Peer& p, int topic_id, std::string* err);
    Val get_forum_topics(const Peer& p, int limit, std::string* err);
    Val transfer_chat_ownership(const Peer& chat, long long user_id, const std::string& password, std::string* err);

    Val send_poll(const Peer& p, const std::string& question, const std::vector<std::string>& options,
                  bool quiz, int correct_option, std::string* err);
    Val stop_poll(const Peer& p, long long msg_id, std::string* err);
    Val vote_poll(const Peer& p, long long msg_id, const std::vector<int>& option_ids, std::string* err);
    Val retract_vote(const Peer& p, long long msg_id, std::string* err);
    Val send_dice(const Peer& p, const std::string& emoji, std::string* err);
    Val send_contact(const Peer& p, const std::string& phone, const std::string& first, const std::string& last,
                     std::string* err);
    Val send_location(const Peer& p, double lat, double lon, std::string* err);
    Val send_venue(const Peer& p, double lat, double lon, const std::string& title, const std::string& address,
                   std::string* err);
    Val send_sticker(const Peer& p, long long file_id, long long access_hash, const std::string& file_ref_b64,
                     std::string* err);
    Val copy_message(const Peer& from, long long msg_id, const Peer& to, std::string* err);
    Val get_scheduled_messages(const Peer& p, std::string* err);
    Val read_mentions(const Peer& p, std::string* err);
    Val read_reactions(const Peer& p, std::string* err);
    Val view_messages(const Peer& p, const std::vector<long long>& ids, std::string* err);
    Val get_discussion_message(const Peer& p, long long msg_id, std::string* err);
    Val get_discussion_replies(const Peer& p, long long msg_id, int limit, std::string* err);
    Val delete_chat_history(const Peer& p, bool revoke, std::string* err);
    Val send_chat_action(const Peer& p, const std::string& action, std::string* err);

    Val get_contacts(std::string* err);
    Val add_contact(long long user_id, const std::string& first, const std::string& last, const std::string& phone,
                    std::string* err);
    Val delete_contacts(const std::vector<long long>& user_ids, std::string* err);
    Val search_contacts(const std::string& query, int limit, std::string* err);
    Val import_contacts(const std::string& phone, const std::string& first, const std::string& last, std::string* err);
    Val block_user(long long user_id, std::string* err);
    Val unblock_user(long long user_id, std::string* err);

    Val update_profile(const std::string& first, const std::string& last, const std::string& about, std::string* err);
    Val set_username(const std::string& username, std::string* err);
    Val update_status(bool offline, std::string* err);
    Val get_common_chats(long long user_id, int limit, std::string* err);
    Val get_chat_photos(const Peer& p, int limit, std::string* err);
    Val set_emoji_status(long long document_id, std::string* err);

    Val get_password_hint(std::string* err);
    Val get_active_sessions(std::string* err);
    Val reset_sessions(std::string* err);
    Val sign_in_bot(const std::string& bot_token, std::string* err);

    Val export_chat_invite_link(const Peer& p, std::string* err);
    Val create_chat_invite_link(const Peer& p, int expire_date, int usage_limit, std::string* err);
    Val revoke_chat_invite_link(const Peer& p, const std::string& link, std::string* err);
    Val approve_chat_join_request(const Peer& p, long long user_id, std::string* err);
    Val decline_chat_join_request(const Peer& p, long long user_id, std::string* err);
    Val get_chat_join_requests(const Peer& p, int limit, std::string* err);

    Val get_folders(std::string* err);
    Val delete_folder(int folder_id, std::string* err);
    Val join_folder(const std::string& slug, std::string* err);
    Val leave_folder(int folder_id, std::string* err);

    Val get_all_stories(std::string* err);
    Val get_chat_stories(const Peer& p, std::string* err);
    Val read_chat_stories(const Peer& p, int max_id, std::string* err);
    Val delete_stories(const Peer& p, const std::vector<int>& ids, std::string* err);

    Val get_sticker_set(const std::string& short_name, std::string* err);
    Val search_stickers(const std::string& q, int limit, std::string* err);
    Val get_favorite_stickers(std::string* err);


    Val get_privacy(const std::string& key, std::string* err);
    Val get_account_ttl(std::string* err);
    Val set_account_ttl(int days, std::string* err);

    Val apply_boost(const Peer& p, std::string* err);
    Val get_boosts(const Peer& p, std::string* err);
    Val get_boosts_status(const Peer& p, std::string* err);

    Val get_stars_balance(std::string* err);
    Val get_available_gifts(std::string* err);
    Val send_gift(long long user_id, long long gift_id, const std::string& text, std::string* err);

    Val get_app_config(std::string* err);
    Val get_ton_balance(std::string* err);
    Val send_rich_message(const Peer& p, const std::string& md, int64_t reply_to_msg_id, std::string* err);
    Val set_bot_profile_photo(long long bot_id, const std::string& jpeg, std::string* err);
    Val edit_folder_invite_link(int folder_id, const std::string& slug, const std::string& title, std::string* err);
    Val get_call_members(int64_t call_id, int64_t access_hash, std::string* err);
    Val enable_cloud_password(const std::string& new_password, const std::string& hint, std::string* err);
    Val change_cloud_password(const std::string& current, const std::string& next, const std::string& hint, std::string* err);
    Val remove_cloud_password(const std::string& current, std::string* err);
    Val set_bot_commands(const std::vector<std::pair<std::string, std::string>>& cmds, std::string* err);
    Val send_story(const Peer& p, const std::string& jpeg, const std::string& caption, std::string* err);

    // --- full-kurigram-parity wrappers (src/parity_wrappers.cpp) ---
    // chats
    Val add_chat_members(const Peer& chat, const std::vector<long long>& user_ids, int forward_limit, std::string* err);
    Val create_supergroup(const std::string& title, const std::string& about, bool forum, std::string* err);
    Val delete_supergroup(const Peer& p, std::string* err);
    Val delete_chat_photo(const Peer& p, std::string* err);
    Val set_chat_photo(const Peer& p, const std::string& jpeg, std::string* err);
    Val set_chat_permissions(const Peer& p, const std::string& perms_json, std::string* err);
    Val set_chat_discussion_group(const Peer& p, const Peer& discussion, std::string* err);
    Val set_chat_direct_messages_group(const Peer& p, const Peer& group, std::string* err);
    Val set_send_as_chat(const Peer& p, const Peer& send_as, std::string* err);
    Val get_send_as_chats(const Peer& p, std::string* err);
    Val set_chat_member_tag(int64_t chat_id, long long user_id, const std::string& tag, std::string* err);
    Val set_chat_accent_color(const Peer& p, int accent_color_id, long long bg_emoji_id, std::string* err);
    Val set_chat_profile_accent_color(const Peer& p, int accent_color_id, long long bg_emoji_id, std::string* err);
    Val set_upgraded_gift_colors(const Peer& p, long long collectible_id, std::string* err);
    Val toggle_folder_tags(bool enabled, std::string* err);
    Val toggle_join_to_send(const Peer& p, bool enabled, std::string* err);
    Val process_chat_has_protected_content_disable_request(const Peer& p, int64_t request_msg_id, bool approve, std::string* err);
    Val get_chat_settings(const Peer& p, std::string* err);
    Val get_chat_online_count(const Peer& p, std::string* err);
    Val get_similar_channels(const Peer& p, std::string* err);
    Val get_suitable_discussion_chats(std::string* err);
    Val get_top_chats(int limit, std::string* err);
    Val delete_chat_admin_invite_links(const Peer& p, long long admin_id, std::string* err);
    Val get_chat_admin_invite_links(const Peer& p, long long admin_id, int limit, bool revoked, std::string* err);
    Val get_chat_admin_invite_links_count(const Peer& p, long long admin_id, bool revoked, std::string* err);
    Val get_chat_admins_with_invite_links(const Peer& p, std::string* err);
    Val update_chat_notifications(const Peer& p, bool mute, int mute_until, std::string* err);
    Val get_chat_photos_count(const Peer& p, std::string* err);
    Val get_personal_channels(const Peer& p, std::string* err);
    Val get_direct_messages_topics(const Peer& p, std::string* err);
    Val get_direct_messages_topics_by_id(const Peer& p, const std::vector<int>& topic_ids, std::string* err);
    Val set_main_profile_tab(std::string* err);
    // forum topics
    Val hide_general_forum_topic(const Peer& p, std::string* err);
    Val unhide_general_forum_topic(const Peer& p, std::string* err);
    Val toggle_forum_topics(const Peer& p, bool enabled, bool tabs, std::string* err);
    Val unpin_all_forum_topic_messages(const Peer& p, int64_t message_thread_id, std::string* err);
    // invite links
    Val get_chat_invite_link(const Peer& p, const std::string& link, std::string* err);
    Val edit_chat_invite_link(const Peer& p, const std::string& link, const std::string& title, int64_t expire_date, int usage_limit, bool request_needed, std::string* err);
    Val delete_chat_invite_link(const Peer& p, const std::string& link, std::string* err);
    Val approve_all_chat_join_requests(const Peer& p, const std::string& link, std::string* err);
    Val decline_all_chat_join_requests(const Peer& p, const std::string& link, std::string* err);
    Val get_chat_invite_link_joiners(const Peer& p, const std::string& link, int limit, int64_t offset_date, std::string* err);
    Val get_chat_invite_link_joiners_count(const Peer& p, const std::string& link, std::string* err);
    Val check_chat_folder_invite_link(const std::string& slug, std::string* err);
    Val get_chats_for_folder_invite_link(long long folder_id, std::string* err);
    // folders
    Val reorder_folders(const std::vector<int>& order, std::string* err);
    // contacts
    Val get_contacts_count(std::string* err);
    Val get_blocked_message_senders(std::string* err);
    Val set_contact_note(long long user_id, const std::string& note, std::string* err);

    Val delete_user_history(const Peer& p, long long user_id, std::string* err);
    Val reorder_collection_gifts(const Peer& p, int collection_id, const std::vector<int>& order, std::string* err);
    Val set_gift_resale_price(const std::string& stargift_json, long long price, const std::string& currency, std::string* err);
    Val show_chat_stories(const Peer& p, std::string* err);

    // --- advanced transport helpers ---
    // Raw invoke of any TL method by its full name (e.g. "channels.editTitle").
    Val invoke(const std::string& method, const std::string& args_json, std::string* err);
    // updates.getChannelDifference with the given pts/limit.
    Val recover_gaps(long long channel_id, int pts, int limit, std::string* err);
    // Resolve any peer reference (username/id) to an inputPeer via search.
    Val resolve_peer(const std::string& query, std::string* err);
    // upload.saveBigFilePart chunk upload -> inputFile.
    Val save_file(const std::string& data, const std::string& name, std::string* err);

    // --- lifecycle utilities ---
    void start();
    void stop();
    void run();
    void restart();
    void stop_transmission();
    // Compose/parse helpers.
    std::vector<Val> compose(const std::string& md, std::string* plain_out);

    // messages
    Val add_checklist_tasks(const Peer& p, int64_t message_id, const std::string& tasks_json, std::string* err);
    Val mark_checklist_tasks_as_done(const Peer& p, int64_t message_id, const std::vector<int>& ids, std::string* err);
    Val add_poll_option(const Peer& p, int64_t message_id, const std::string& option, std::string* err);
    Val delete_poll_option(const Peer& p, int64_t message_id, const std::string& option, std::string* err);
    Val add_to_gifs(const std::string& input_document_json, bool save, std::string* err);
    Val approve_suggested_post(const Peer& p, int64_t message_id, const std::string& comment, std::string* err);
    Val decline_suggested_post(const Peer& p, int64_t message_id, const std::string& comment, std::string* err);
    Val compose_text_with_ai(const Peer& p, const std::string& prompt, std::string* err);
    Val fix_text_with_ai(const Peer& p, const std::string& text, std::string* err);
    Val summarize_message(const Peer& p, const std::vector<int>& message_ids, std::string* err);
    Val translate_message_text(const Peer& p, int64_t message_id, const std::string& to_lang, std::string* err);
    Val translate_text(const std::string& text, const std::string& to_lang, std::string* err);
    Val send_screenshot_notification(const Peer& p, const std::vector<int64_t>& msg_ids, const std::string& jpeg, std::string* err);
    Val forward_media_group(const Peer& from, const std::vector<int64_t>& msg_ids, const Peer& to, std::string* err);
    Val copy_media_group(const Peer& to, const std::string& multi_media_json, std::string* err);
    Val send_cached_media(const Peer& p, const std::string& media_json, const std::string& caption, int64_t reply_to_msg_id, std::string* err);
    Val get_media_group(const Peer& p, int64_t message_id, std::string* err);
    Val send_media_group(const Peer& p, const std::string& media_list_json, const std::string& caption, bool disable_notification, std::string* err);
    Val search_messages_count(const std::string& q, const Peer& p, const std::string& filter_json, std::string* err);
    Val send_media_group_json(const Peer& p, const std::string& media_list_json, const std::string& caption, bool disable_notification, std::string* err);
    Val send_checklist(const Peer& p, const std::string& checklist_json, const std::string& caption, int64_t reply_to_msg_id, std::string* err);
    Val send_live_photo(const Peer& p, const std::string& jpeg, const std::string& caption, std::string* err);
    Val send_video_note(const Peer& p, const std::string& data, int duration, int w, int h, std::string* err);
    Val send_paid_media(const Peer& p, const std::string& media_list_json, long long stars, std::string* err);
    Val send_paid_reaction(const Peer& p, int64_t message_id, int count, const std::string& reaction_json, std::string* err);
    Val send_message_draft(const Peer& p, const std::string& text, const std::string& entities_json, int64_t reply_to_msg_id, std::string* err);
    Val send_rich_message_draft(const Peer& p, const std::string& md, int64_t reply_to_msg_id, std::string* err);
    Val send_web_page(const Peer& p, const std::string& url, const std::string& text, std::string* err);
    Val edit_message_caption(const Peer& p, int64_t message_id, const std::string& caption, std::string* err);
    Val edit_message_media(const Peer& p, int64_t message_id, const std::string& media_json, std::string* err);
    Val edit_message_reply_markup(const Peer& p, int64_t message_id, const std::string& keyboard_json, std::string* err);
    Val edit_inline_text(const std::string& inline_message_id, const std::string& text, const std::string& entities_json, std::string* err);
    Val edit_inline_caption(const std::string& inline_message_id, const std::string& caption, std::string* err);
    Val edit_inline_media(const std::string& inline_message_id, const std::string& media_json, std::string* err);
    Val edit_inline_reply_markup(const std::string& inline_message_id, const std::string& keyboard_json, std::string* err);
    Val edit_message_checklist(const Peer& p, int64_t message_id, const std::string& tasks_json, const std::vector<int>& done_ids, std::string* err);
    Val delete_direct_messages_chat_topic_history(const Peer& p, std::string* err);
    Val get_direct_messages_chat_topic_history(const Peer& p, int limit, std::string* err);
    Val set_direct_messages_chat_topic_is_marked_as_unread(const Peer& p, bool unread, std::string* err);
    Val get_chat_history_count(const Peer& p, std::string* err);
    Val get_discussion_replies_count(const Peer& p, int64_t message_id, std::string* err);
    Val get_user_personal_chat_messages(long long user_id, std::string* err);
    Val get_available_effects(std::string* err);
    Val search_global(const std::string& q, int limit, bool broadcasts_only, bool groups_only, bool users_only, std::string* err);
    Val search_global_count(const std::string& q, std::string* err);
    Val search_posts(const std::string& q, int limit, bool hashtag, std::string* err);
    Val search_posts_count(const std::string& q, std::string* err);
    Val get_web_app_url(const std::string& bot, const std::string& short_name, const std::string& url, std::string* err);
    Val get_main_web_app(const std::string& bot, std::string* err);
    Val get_web_app_link_url(const std::string& bot, const std::string& short_name, std::string& url, std::string* err);
    Val open_web_app(const Peer& p, const std::string& bot, const std::string& short_name, std::string* err);
    Val start_bot(const Peer& p, const std::string& bot, const std::string& start_param, std::string* err);
    Val stream_media(const Peer& p, int64_t message_id, const std::string& path, std::string* err);

    // payments
    Val add_collection_gifts(const Peer& owner, int collection_id, const std::string& stargifts_json, std::string* err);
    Val remove_collection_gifts(const Peer& owner, int collection_id, const std::string& stargifts_json, std::string* err);
    Val create_gift_collection(const Peer& owner, const std::string& title, const std::string& gift_ids_json, std::string* err);
    Val delete_gift_collection(const Peer& owner, int collection_id, std::string* err);
    Val reorder_gift_collections(const Peer& owner, const std::vector<int>& order, std::string* err);
    Val set_gift_collection_name(const Peer& owner, int collection_id, const std::string& title, std::string* err);
    Val get_gift_collections(const Peer& owner, std::string* err);
    Val get_chat_gifts(const Peer& owner, int offset, int limit, bool exclude_unsaved, bool exclude_saved, bool exclude_unlimited, bool exclude_unique, bool exclude_upgradable, bool exclude_unupgradable, std::string* err);
    Val get_chat_gifts_count(const Peer& owner, std::string* err);
    Val apply_gift_code(const std::string& slug, std::string* err);
    Val check_gift_code(const std::string& slug, std::string* err);
    Val get_gift_upgrade_preview(const Peer& p, const std::string& gift_json, std::string* err);
    Val get_gift_upgrade_variants(const Peer& p, const std::string& gift_json, std::string* err);
    Val upgrade_gift(const Peer& owner, const std::string& gift_json, bool keep_original_details, std::string* err);
    Val buy_gift_upgrade(const Peer& owner, const std::string& gift_json, const std::string& invoice_json, std::string* err);
    Val transfer_gift(const Peer& owner, const std::string& gift_json, const Peer& to, std::string* err);
    Val convert_gift_to_stars(const Peer& owner, const std::string& gift_json, std::string* err);
    Val drop_gift_original_details(const Peer& owner, const std::string& gift_json, const std::string& invoice_json, std::string* err);
    Val craft_gift(const Peer& p, const std::string& gifts_json, const std::string& invoice_json, std::string* err);
    Val get_gifts_for_crafting(long long gift_id, int limit, std::string* err);
    Val get_upgraded_gift(const std::string& slug, std::string* err);
    Val get_upgraded_gift_value_info(const std::string& slug, std::string* err);
    Val get_gift_auction_state(const Peer& p, const std::string& gift_json, std::string* err);
    Val place_gift_auction_bid(const Peer& p, const std::string& gift_json, long long stars, const std::string& invoice_json, std::string* err);
    Val increase_gift_auction_bid(const Peer& p, const std::string& gift_json, long long stars, const std::string& invoice_json, std::string* err);
    Val send_gift_purchase_offer(const Peer& p, const std::string& gift_json, long long stars, long long duration, bool anonym, const std::string& invoice_json, std::string* err);
    Val process_gift_purchase_offer(const std::string& msg_id, bool approve, std::string* err);
    Val search_gifts_for_resale(long long gift_id, int order, bool for_crafting, bool for_stars,
                                 const std::string& attributes_json, int limit, const std::string& offset,
                                 std::string* err);
    Val send_resold_gift(const Peer& to, const std::string& slug, const std::string& invoice_json, std::string* err);
    Val get_payment_form(const std::string& invoice_json, std::string* err);
    Val send_payment_form(const Peer& p, int64_t form_id, const std::string& invoice_json, const std::string& payment_json, std::string* err);
    Val edit_star_subscription(const std::string& subscription_id, bool canceled, std::string* err);
    Val reuse_star_subscription(const std::string& subscription_id, const std::string& invoice_json, std::string* err);
    Val gift_premium_with_stars(const Peer& p, int months, long long stars, const std::string& invoice_json, std::string* err);
    Val suggest_birthday(int day, int month, int year, std::string* err);
    Val toggle_gift_pinned(const Peer& p, const std::string& stargifts_json, bool pinned, std::string* err);
    Val hide_gift(const Peer& p, const std::string& gift_json, bool hide, std::string* err);

    // stickers
    Val add_favorite_sticker(const std::string& document_json, std::string* err);
    Val remove_favorite_sticker(const std::string& document_json, std::string* err);
    Val add_recent_sticker(const std::string& document_json, std::string* err);
    Val remove_recent_sticker(const std::string& document_json, std::string* err);
    Val get_recent_stickers(std::string* err);
    Val clear_recent_stickers(std::string* err);
    Val get_owned_sticker_sets(std::string* err);
    Val add_sticker_to_set(const Peer& owner, const std::string& short_name, const std::string& sticker_json, std::string* err);
    Val delete_sticker_from_set(const std::string& short_name, const std::string& document_json, std::string* err);
    Val create_new_sticker_set(const Peer& owner, const std::string& title, const std::string& short_name, const std::string& stickers_json, bool masquerade, bool animated, bool video, bool text_color, bool thumb, std::string* err);
    Val delete_sticker_set(const std::string& short_name, std::string* err);
    Val change_sticker_set(const Peer& p, const std::string& short_name, bool install, bool archived, std::string* err);
    Val reorder_installed_sticker_sets(const std::vector<long long>& ids, bool masquerade, std::string* err);
    Val replace_sticker_in_set(const std::string& short_name, const std::string& old_doc_json, const std::string& new_sticker_json, std::string* err);
    Val get_custom_emoji_stickers(const std::vector<long long>& ids, std::string* err);
    Val search_sticker_sets(const std::string& q, bool exclude_featured, std::string* err);
    Val get_suggested_sticker_set_name(const std::string& title, std::string* err);
    Val upload_sticker_file(const Peer& owner, const std::string& data, const std::string& mime, const std::string& attrs_json, std::string* err);
    Val set_sticker_set_title(const std::string& short_name, const std::string& title, std::string* err);
    Val set_sticker_set_thumbnail(const std::string& short_name, const Peer& owner, const std::string& thumb_json, bool video, std::string* err);
    Val set_custom_emoji_sticker_set_thumbnail(const std::string& short_name, const std::string& thumb_json, std::string* err);
    Val set_sticker_position_in_set(const std::string& short_name, const std::string& document_json, int position, std::string* err);
    Val set_sticker_emoji_list(const std::string& short_name, const std::string& document_json, const std::vector<std::string>& emoji_list, std::string* err);
    Val set_sticker_keywords(const std::string& short_name, const std::string& document_json, const std::vector<std::string>& keywords, bool mask, std::string* err);
    Val set_sticker_mask_position(const std::string& short_name, const std::string& document_json, const std::string& mask_json, std::string* err);

    // stories
    Val can_post_stories(const Peer& p, std::string* err);
    Val get_stories(const Peer& p, int64_t story_id, std::string* err);
    Val get_archived_stories(const Peer& p, int limit, int64_t offset_id, std::string* err);
    Val get_story_views(const Peer& p, const std::string& q, int64_t offset_id, std::string* err);
    Val get_pinned_stories(const Peer& p, int limit, int64_t offset_id, std::string* err);
    Val view_stories(const Peer& p, const std::vector<int64_t>& story_ids, std::string* err);
    Val edit_story_caption(const Peer& p, int64_t story_id, const std::string& caption, const std::string& entities_json, std::string* err);
    Val edit_story_media(const Peer& p, int64_t story_id, const std::string& media_json, std::string* err);
    Val edit_story_privacy(const Peer& p, int64_t story_id, const std::string& privacy_json, std::string* err);
    Val delete_story(const Peer& p, int64_t story_id, std::string* err);
    Val pin_chat_stories(const Peer& p, const std::vector<int64_t>& story_ids, bool pin, std::string* err);
    Val hide_chat_stories(const Peer& p, bool hide, std::string* err);
    Val enable_stealth_mode(std::string* err);
    Val forward_story(const Peer& to, int64_t story_id, const Peer& from, std::string* err);
    Val copy_story(const Peer& p, int64_t story_id, const std::string& media_json, const std::string& caption, std::string* err);

    // bots
    Val answer_callback_query(int64_t query_id, const std::string& text, const std::string& url, bool show_alert, std::string* err);
    Val answer_inline_query(int64_t query_id, const std::string& results_json, int cache_time, bool is_personal, const std::string& next_offset, const std::string& switch_pm_json, std::string* err);
    Val answer_web_app_query(const std::string& query_id, const std::string& result_json, std::string* err);
    Val answer_pre_checkout_query(int64_t query_id, bool ok, const std::string& error_msg, std::string* err);
    Val answer_shipping_query(int64_t query_id, bool ok, const std::string& error_msg, const std::string& shipping_options_json, std::string* err);
    Val answer_guest_query(const std::string& query_id, const std::string& result_json, std::string* err);
    Val answer_chat_join_request_query(int64_t query_id, bool approved, std::string* err);
    Val send_chat_join_request_web_app(int64_t chat_id, int64_t query_id, std::string* err);
    Val get_inline_bot_results(const std::string& bot, const Peer& peer, const std::string& query, int64_t offset, std::string* err);
    Val send_inline_bot_result(const Peer& p, int64_t query_id, const std::string& result_id, bool hide_via, const std::string& top_msg_id, std::string* err);
    Val create_invoice_link(const std::string& title, const std::string& description, const std::string& payload_json, const std::string& provider_token, const std::string& currency, long long amount, std::string* err);
    Val send_invoice(const Peer& p, const std::string& title, const std::string& description, const std::string& payload_json, const std::string& provider_token, const std::string& currency, long long amount, const std::string& reply_markup_json, std::string* err);
    Val refund_star_payment(int64_t user_id, const std::string& telegram_payment_charge_id, std::string* err);
    Val edit_user_star_subscription(int64_t user_id, const std::string& telegram_payment_charge_id, bool is_canceled, std::string* err);
    Val get_bot_name(const std::string& bot, const std::string& lang_code, std::string* err);
    Val set_bot_name(const std::string& bot, const std::string& lang_code, const std::string& name, std::string* err);
    Val get_bot_info_description(const std::string& bot, const std::string& lang_code, std::string* err);
    Val set_bot_info_description(const std::string& bot, const std::string& lang_code, const std::string& description, std::string* err);
    Val get_bot_info_short_description(const std::string& bot, const std::string& lang_code, std::string* err);
    Val set_bot_info_short_description(const std::string& bot, const std::string& lang_code, const std::string& short_description, std::string* err);
    Val get_bot_default_privileges(std::string* err);
    Val set_bot_default_privileges(const std::string& channel, const std::string& admin_rights_json, bool broadcast, bool groups, bool users, std::string* err);
    Val set_chat_menu_button(const std::string& button_json, std::string* err);
    Val get_chat_menu_button(long long user_id, std::string* err);
    Val create_bot(const std::string& bot_username, const std::string& bot_name, const std::string& reply_markup_json, std::string* err);
    Val check_bot_username(const std::string& bot, const std::string& username, std::string* err);
    Val get_owned_bots(std::string* err);
    Val get_managed_bot_token(long long bot_id, std::string* err);
    Val replace_managed_bot_token(long long bot_id, const std::string& new_token, std::string* err);
    Val get_managed_bot_access_settings(long long bot_id, std::string* err);
    Val set_managed_bot_access_settings(long long bot_id, bool can_access_messages, bool manage, std::string* err);
    Val get_bot_commands(const std::string& bot, const std::string& lang_code, const std::string& scope, long long chat_id, std::string* err);
    Val set_game_score(const Peer& p, int64_t message_id, long long user_id, int score, bool edit_message, std::string* err);
    Val get_game_high_scores(const Peer& p, int64_t message_id, long long user_id, std::string* err);
    Val send_game(const Peer& p, const std::string& bot, const std::string& game_short_name, int64_t reply_to_msg_id, std::string* err);
    Val delete_ephemeral_message(const Peer& p, int64_t message_id, std::string* err);
    Val edit_ephemeral_message_text(const Peer& p, int64_t message_id, const std::string& text, const std::string& entities_json, std::string* err);
    Val edit_ephemeral_message_caption(const Peer& p, int64_t message_id, const std::string& caption, std::string* err);
    Val edit_ephemeral_message_media(const Peer& p, int64_t message_id, const std::string& media_json, std::string* err);
    Val edit_ephemeral_message_reply_markup(const Peer& p, int64_t message_id, const std::string& keyboard_json, std::string* err);
    Val answer_terms_of_service(const std::string& terms_id, std::string* err);

    // users
    Val check_username(const Peer& channel, const std::string& username, std::string* err);
    Val get_chat_audios(const Peer& p, int limit, int64_t offset_id, std::string* err);
    Val get_chat_audios_count(const Peer& p, std::string* err);
    Val get_default_emoji_statuses(std::string* err);
    Val set_personal_channel(const Peer& p, std::string* err);
    Val set_profile_photo(const std::string& jpeg, const std::string& video, double video_start_ts, std::string* err);
    Val update_birthday(int day, int month, int year, std::string* err);
    Val delete_profile_photos(const std::vector<int64_t>& photo_ids, std::string* err);

    // account
    Val add_profile_audio(const Peer& owner, const std::string& data, const std::string& mime, const std::string& title, const std::string& performer, std::string* err);
    Val remove_profile_audio(const std::string& document_json, std::string* err);
    Val set_profile_audio_position(const std::string& document_json, int position, std::string* err);
    Val get_global_privacy_settings(std::string* err);
    Val set_global_privacy_settings(bool archive_and_mute_new_noncontact_peers, bool keep_archived_unmuted, bool keep_archived_folders, bool hide_read_marks, std::string* err);
    Val set_inactive_session_ttl(int days, std::string* err);
    Val set_privacy(const std::string& privacy_key_json, const std::string& rules_json, std::string* err);

    // auth
    Val reset_session(int64_t hash, std::string* err);
    Val accept_terms_of_service(const std::string& id_json, std::string* err);
    Val change_phone_number(const std::string& phone, const std::string& phone_code_hash, const std::string& phone_code, std::string* err);
    Val recover_password(const std::string& code, std::string* err);
    Val send_recovery_code(const std::string& login, std::string* err);

    // business
    Val delete_business_messages(const Peer& p, const std::vector<int64_t>& msg_ids, std::string* err);
    Val get_business_account_gifts(const Peer& owner, int offset, int limit, std::string* err);
    Val get_business_account_star_balance(const std::string& business_connection_id, std::string* err);
    Val get_business_connection(const std::string& business_connection_id, std::string* err);
    Val transfer_business_account_stars(const std::string& business_connection_id, long long amount, std::string* err);

    // --- generic schema API (every Telegram RPC callable) ---
    // args built with Val::obj(...) or parse_json(); returns decoded tree.
    Val api(const std::string& method, const Val& args, std::string* err);
    Val api_json(const std::string& method, const std::string& args_json, std::string* err);
    int64_t difference_pts() const { return diff_pts_; }

    const Session& session() const { return session_; }

    static Session parse_session_string(const std::string& encoded);
    static std::string dc_ip(int dc_id);
private:
    Session session_;
    ClientConfig cfg_;
    int fd_{-1};
    bool initialized_{false};
    int64_t session_id_{0};
    int64_t last_msg_id_{0};
    int64_t seq_no_{0};
    int64_t server_salt_{0};
    bool salt_retry_{false};
    bool expect_pong_{false};
    bool rpc_ready_{false};
    bool init_active_{false};
    int64_t self_ah_{0};
    int64_t diff_pts_{0};
    std::vector<int64_t> pending_acks_;
    Val me_;
    int32_t diff_date_{0};
    int64_t diff_qts_{0};
    std::unordered_map<int64_t, Peer> peers_;
    std::mutex send_mu_;
    std::mutex updates_mu_;
    std::function<void(const UpdateEvent&)> update_handler_;
    std::vector<std::tuple<int, MessageFilter, MessageHandler>> msg_handlers_;
    int msg_handle_seq_{0};
    std::mutex msg_mu_;
    // Typed handler registry: kind -> (handle, closure). Kinds:
    // "edited_message", "deleted_messages", "callback_query", "message_reaction",
    // "chat_member_updated", "chat_join_request", "poll", "user_status", "typing",
    // "story", "inline_query", "chosen_inline_result", "raw_update", "error",
    // "connect", "disconnect".
    std::unordered_map<std::string, std::vector<std::pair<int, std::function<void(const Val&)>>>> typed_handlers_;
    mutable std::mutex typed_mu_;
    std::string recv_buf_;
    bool obfuscated_{false};

    bool socket_connect(std::string* err);
    bool send_all(const std::string& data);
    bool recv_chunk(int timeout_ms);
    bool write_msg(const std::string& msg);
    bool read_msg(std::string* msg, int first_wait_ms = 200);
    std::string encrypt_body(const std::string& body, int64_t* msg_id, bool content_related);
    // Decrypts one encrypted_message; returns plaintext inner body (after salt/session/msg_id/seq/len header).
    void flush_acks();
    void track_update_pts(const std::string& body);
    void emit_message_events(const std::string& body);
    bool decrypt_message(const std::string& blob, std::string* body, int64_t* msg_id_out);
    // Process one decrypted body; if it is rpc_result matching req_id, copies response into out.
    bool handle_plaintext(const std::string& body, int64_t req_id, std::string* out_result, bool* matched, int64_t msg_id = 0);
    bool init_connection(std::string* err);
    bool resolve_channel(int64_t chat_id, int64_t* channel_out, int64_t* ah, std::string* err);
    void dispatch_update(const std::string& obj);
    bool wait_socket_readable(int timeout_ms);
    void dispatch_typed_event(const UpdateEvent& ev);
    void fire_lifecycle(const std::string& kind);
    int register_typed(const std::string& kind, std::function<void(const Val&)> fn);
};

// --- MTProto auth-key generation (src/keygen.cpp) ---
// Full DH handshake over the abridged transport; writes the 256-byte key.
bool generate_auth_key(const std::string& ip, int port, int dc_id, std::string* out_key,
                       std::string* err);
// Builds the inputCheckPasswordSRP JSON args object for auth.checkPassword
// from the account.getPassword response tree (kurigram SRP algorithm).
std::string srp_password_check(const Val& password_obj, const std::string& password,
                               std::string* err);
// Same, but returns the ready-to-encode argument tree with A/M1 already turned
// from base64url into the raw bytes the TL encoder writes verbatim.
Val srp_check_password_args(const Val& password_obj, const std::string& password,
                            std::string* err);
// Builds the v = g^x mod p hash used by account.passwordInputSettings when a cloud
// password is (re)set, from the new_algo of the getPassword response tree. When
// algo_json_out is given it receives the salted algo JSON to send alongside the hash.
std::string srp_new_password_hash(const Val& password_obj, const std::string& new_password,
                                  std::string* algo_json_out, std::string* err);
// n pseudo-random bytes from the process-local CSPRNG.
std::string random_bytes(size_t n);
// Markdown subset -> plain text + messageEntity Vals (src/format.cpp).
std::vector<Val> parse_markdown(const std::string& md, std::string* plain_out);

}  // namespace cppgram
