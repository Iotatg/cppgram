// Typed update events for the high-level handler system (on_edited_message, on_callback_query, ...).
// Every struct also carries the full decoded `Val` of the originating Update so nothing is lost.
#pragma once

#include "cppgram/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace cppgram {

// updateEditMessage / updateEditChannelMessage / updateNewScheduledMessage
struct EditedMessageEvent {
    int64_t id{0};
    int64_t chat_id{0};
    std::string text;
    int64_t date{0};
    Val raw;
};

// updateDeleteMessages / updateDeleteChannelMessages / updateDeleteScheduledMessages
struct DeletedMessagesEvent {
    int64_t chat_id{0};              // 0 when not channel-scoped
    std::vector<int64_t> message_ids;
    Val raw;
};

// updateBotCallbackQuery / updateInlineBotCallbackQuery
struct CallbackQuery {
    int64_t query_id{0};
    int64_t user_id{0};
    int64_t msg_id{0};
    int64_t chat_id{0};
    int64_t chat_instance{0};
    std::string data;
    std::string game_short_name;
    Val message;  // decoded message the button belongs to (may be empty)
    Val raw;
};

// updateMessageReactions (bulk reaction state of one message)
struct MessageReactionEvent {
    int64_t chat_id{0};
    int64_t msg_id{0};
    std::vector<std::string> reactions;         // emojis/%custom-emoji ids%
    std::vector<int32_t> counts;
    Val reactions_blob;  // full MessageReactions object
    Val raw;
};

// updateChatParticipant / updateChannelParticipant (+ Add/Delete/Admin variants)
struct ChatMemberUpdated {
    int64_t chat_id{0};
    int64_t date{0};
    int64_t actor_id{0};
    int64_t user_id{0};
    std::string kind;               // "join", "leave", "promote", "demote", "restricted", "rights"
    std::string prev_status;        // raw participant constructor name
    std::string new_status;         // raw participant constructor name
    Val prev_participant;
    Val new_participant;
    Val raw;
};

// updateBotChatInviteRequester
struct ChatJoinRequest {
    int64_t chat_id{0};
    int64_t user_id{0};
    int64_t date{0};
    std::string about;
    Val invite;
    int64_t qts{0};
    Val raw;
};

// updateMessagePoll / updateMessagePollVote
struct PollUpdate {
    int64_t chat_id{0};
    int64_t msg_id{0};
    int64_t poll_id{0};
    Val poll;       // may be empty for vote updates
    Val results;
    bool voted{false};
    std::vector<std::string> options;
    Val raw;
};

// updateUserStatus
struct UserStatusEvent {
    int64_t user_id{0};
    std::string status;   // "online", "offline", "recently", "last_week", "last_month"
    int64_t was_online{0};
    Val raw;
};

// updateUserTyping / updateChatUserTyping / updateChannelUserTyping
struct TypingEvent {
    int64_t chat_id{0};
    int64_t user_id{0};
    std::string action;   // "typing", "upload_photo", "record_video", ...
    Val raw;
};

// updateStory / updateStoryID
struct StoryUpdate {
    int64_t chat_id{0};       // peer the story belongs to
    int64_t story_id{0};
    Val story;
    Val raw;
};

// updateBotInlineQuery
struct InlineQueryEvent {
    int64_t query_id{0};
    int64_t user_id{0};
    std::string query;
    std::string offset;
    Val raw;
};

// updateBotInlineSend
struct ChosenInlineResultEvent {
    int64_t user_id{0};
    std::string query;
    std::string result_id;
    int64_t msg_id{0};
    Val raw;
};

// updateBotChatBoost
struct ChatBoostEvent {
    int64_t chat_id{0};
    int64_t qts{0};
    Val boost;
    Val raw;
};

// updateBotShippingQuery / updateBotPrecheckoutQuery / updateWebViewResultSent /
// updateBotNewBusinessMessage / updateBotStopped / updatePendingJoinRequests — opaque passthroughs.
struct RawUpdateEvent {
    Val update;   // decoded Update object
};

// updateBotPrecheckoutQuery
struct PreCheckoutQuery {
    int64_t query_id{0};
    int64_t user_id{0};
    std::string currency;
    long long total_amount{0};
    std::string invoice_payload;
    std::string shipping_option_id;
    std::string order_name;
    std::string order_email;
    std::string order_phone;
    std::string shipping_city;
    std::string shipping_country_code;
    std::string shipping_post_code;
    std::string shipping_state;
    std::string shipping_street_line1;
    std::string shipping_street_line2;
    Val raw;
};

// updateBotShippingQuery
struct ShippingQuery {
    int64_t query_id{0};
    int64_t user_id{0};
    std::string invoice_payload;
    std::string shipping_city;
    std::string shipping_country_code;
    std::string shipping_post_code;
    std::string shipping_state;
    std::string shipping_street_line1;
    std::string shipping_street_line2;
    Val raw;
};

// updateBotBusinessConnection
struct BusinessConnectionEvent {
    std::string connection_id;
    int64_t user_id{0};
    int dc_id{0};
    int64_t date{0};
    bool can_reply{false};
    bool is_enabled{false};
    Val raw;
};

// updateBotPurchasedPaidMedia
struct PurchasedPaidMediaEvent {
    int64_t query_id{0};
    int64_t user_id{0};
    std::string payload;
    Val raw;
};

// updateBotStopped / updateStoppedMessageGeneration
struct StoppedMessageGenerationEvent {
    int64_t chat_id{0};
    int64_t message_thread_id{0};
    std::string draft_id;
    Val raw;
};

// updateBotManagedBot
struct ManagedBotEvent {
    int64_t bot_id{0};
    int64_t user_id{0};
    Val raw;
};

// updateBotMessageReactions (per-message reaction count update)
struct MessageReactionCountUpdate {
    int64_t chat_id{0};
    int64_t message_id{0};
    int64_t date{0};
    std::vector<std::string> reactions;
    std::vector<int32_t> counts;
    Val raw;
};

// LabeledPrice / StarAmount / ShippingAddress value types used by payments flows.
struct LabeledPrice {
    std::string label;
    long long amount{0};
};

struct StarAmount {
    long long star_count{0};
    long long nanostar_count{0};
};

struct ShippingAddress {
    std::string country_code;
    std::string state;
    std::string city;
    std::string street_line1;
    std::string street_line2;
    std::string post_code;
};

}  // namespace cppgram
