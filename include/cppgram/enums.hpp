// Pyrogram/kurigram enum parity: every enumerated constant from pyrogram/enums/*.py
// (44 enums / 401 members). Each member carries both the friendly enum value and
// the TL constructor it maps to, so callers may pass either a slider name or the
// raw TL ctor to the wrapper methods.
//
// auto() members (kurigram AutoName) have value == name.lower(); the rest map 1:1
// to a raw.types constructor (TL ctors are lowerCamelCase: sendMessageTypingAction).
//
//   enums::ChatAction::TYPING                              -> "typing"
//   enums::ChatAction::tl_name(enums::ChatAction::TYPING)  -> "sendMessageTypingAction"
//   enums::ChatAction::from_tl("sendMessageTypingAction")  -> TYPING
//
// Table-driven, constexpr, zero init code.
#pragma once

#include <string_view>
#include <string>
#include <utility>   // std::pair

namespace cppgram::enums {

// Reverse lookup shared by every enum.
template <class E, const std::pair<E, std::string_view>* Table, std::size_t N>
E from_tl_impl(std::string_view s, E fallback) {
    for (std::size_t i = 0; i < N; i++)
        if (s == Table[i].second) return Table[i].first;
    return fallback;
}

// ---- BlockList (2 members) ----
struct BlockList {
    enum E : int {
        MAIN = 0,
        STORIES = 1,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { BlockList::MAIN, "main" },
        { BlockList::STORIES, "stories" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- BusinessSchedule (3 members) ----
struct BusinessSchedule {
    enum E : int {
        ALWAYS = 0,
        OUTSIDE_WORK_HOURS = 1,
        CUSTOM = 2,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { BusinessSchedule::ALWAYS, "businessAwayMessageScheduleAlways" },
        { BusinessSchedule::OUTSIDE_WORK_HOURS, "businessAwayMessageScheduleOutsideWorkHours" },
        { BusinessSchedule::CUSTOM, "businessAwayMessageScheduleCustom" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- ButtonStyle (5 members) ----
struct ButtonStyle {
    enum E : int {
        DEFAULT = 0,
        PRIMARY = 1,
        DANGER = 2,
        SUCCESS = 3,
        LINK = 4,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { ButtonStyle::DEFAULT, "default" },
        { ButtonStyle::PRIMARY, "primary" },
        { ButtonStyle::DANGER, "danger" },
        { ButtonStyle::SUCCESS, "success" },
        { ButtonStyle::LINK, "link" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- ChatAction (16 members) ----
struct ChatAction {
    enum E : int {
        TYPING = 0,
        UPLOAD_PHOTO = 1,
        RECORD_VIDEO = 2,
        UPLOAD_VIDEO = 3,
        RECORD_AUDIO = 4,
        UPLOAD_AUDIO = 5,
        UPLOAD_DOCUMENT = 6,
        FIND_LOCATION = 7,
        RECORD_VIDEO_NOTE = 8,
        UPLOAD_VIDEO_NOTE = 9,
        PLAYING = 10,
        CHOOSE_CONTACT = 11,
        SPEAKING = 12,
        IMPORT_HISTORY = 13,
        CHOOSE_STICKER = 14,
        CANCEL = 15,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { ChatAction::TYPING, "sendMessageTypingAction" },
        { ChatAction::UPLOAD_PHOTO, "sendMessageUploadPhotoAction" },
        { ChatAction::RECORD_VIDEO, "sendMessageRecordVideoAction" },
        { ChatAction::UPLOAD_VIDEO, "sendMessageUploadVideoAction" },
        { ChatAction::RECORD_AUDIO, "sendMessageRecordAudioAction" },
        { ChatAction::UPLOAD_AUDIO, "sendMessageUploadAudioAction" },
        { ChatAction::UPLOAD_DOCUMENT, "sendMessageUploadDocumentAction" },
        { ChatAction::FIND_LOCATION, "sendMessageGeoLocationAction" },
        { ChatAction::RECORD_VIDEO_NOTE, "sendMessageRecordRoundAction" },
        { ChatAction::UPLOAD_VIDEO_NOTE, "sendMessageUploadRoundAction" },
        { ChatAction::PLAYING, "sendMessageGamePlayAction" },
        { ChatAction::CHOOSE_CONTACT, "sendMessageChooseContactAction" },
        { ChatAction::SPEAKING, "speakingInGroupCallAction" },
        { ChatAction::IMPORT_HISTORY, "sendMessageHistoryImportAction" },
        { ChatAction::CHOOSE_STICKER, "sendMessageChooseStickerAction" },
        { ChatAction::CANCEL, "sendMessageCancelAction" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- ChatEventActionType (54 members) ----
struct ChatEventActionType {
    enum E : int {
        UNSUPPORTED = 0,
        MESSAGE_EDITED = 1,
        MESSAGE_DELETED = 2,
        MESSAGE_PINNED = 3,
        MESSAGE_UNPINNED = 4,
        POLL_STOPPED = 5,
        MEMBER_JOINED = 6,
        MEMBER_JOINED_BY_INVITE_LINK = 7,
        MEMBER_JOINED_BY_REQUEST = 8,
        MEMBER_INVITED = 9,
        MEMBER_LEFT = 10,
        MEMBER_PROMOTED = 11,
        MEMBER_RESTRICTED = 12,
        MEMBER_TAG_CHANGED = 13,
        MEMBER_SUBSCRIPTION_EXTENDED = 14,
        AVAILABLE_REACTIONS_CHANGED = 15,
        BACKGROUND_CHANGED = 16,
        DESCRIPTION_CHANGED = 17,
        EMOJI_STATUS_CHANGED = 18,
        LINKED_CHAT_CHANGED = 19,
        LOCATION_CHANGED = 20,
        MESSAGE_AUTO_DELETE_TIME_CHANGED = 21,
        PERMISSIONS_CHANGED = 22,
        PHOTO_CHANGED = 23,
        SLOW_MODE_DELAY_CHANGED = 24,
        STICKER_SET_CHANGED = 25,
        EMOJI_STICKER_SET_CHANGED = 26,
        TITLE_CHANGED = 27,
        USERNAME_CHANGED = 28,
        ACTIVE_USERNAMES_CHANGED = 29,
        ACCENT_COLOR_CHANGED = 30,
        PROFILE_ACCENT_COLOR_CHANGED = 31,
        HAS_PROTECTED_CONTENT_TOGGLED = 32,
        INVITES_TOGGLED = 33,
        IS_ALL_HISTORY_AVAILABLE_TOGGLED = 34,
        HAS_AGGRESSIVE_ANTI_SPAM_ENABLED_TOGGLED = 35,
        SIGN_MESSAGES_TOGGLED = 36,
        SHOW_MESSAGE_SENDER_TOGGLED = 37,
        AUTOMATIC_TRANSLATION_TOGGLED = 38,
        INVITE_LINK_EDITED = 39,
        INVITE_LINK_REVOKED = 40,
        INVITE_LINK_DELETED = 41,
        VIDEO_CHAT_CREATED = 42,
        VIDEO_CHAT_ENDED = 43,
        VIDEO_CHAT_MUTE_NEW_PARTICIPANTS_TOGGLED = 44,
        VIDEO_CHAT_PARTICIPANT_IS_MUTED_TOGGLED = 45,
        VIDEO_CHAT_PARTICIPANT_VOLUME_LEVEL_CHANGED = 46,
        IS_FORUM_TOGGLED = 47,
        FORUM_TOPIC_CREATED = 48,
        FORUM_TOPIC_EDITED = 49,
        FORUM_TOPIC_TOGGLE_IS_CLOSED = 50,
        FORUM_TOPIC_TOGGLE_IS_HIDDEN = 51,
        FORUM_TOPIC_DELETED = 52,
        FORUM_TOPIC_PINNED = 53,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { ChatEventActionType::UNSUPPORTED, "unsupported" },
        { ChatEventActionType::MESSAGE_EDITED, "message_edited" },
        { ChatEventActionType::MESSAGE_DELETED, "message_deleted" },
        { ChatEventActionType::MESSAGE_PINNED, "message_pinned" },
        { ChatEventActionType::MESSAGE_UNPINNED, "message_unpinned" },
        { ChatEventActionType::POLL_STOPPED, "poll_stopped" },
        { ChatEventActionType::MEMBER_JOINED, "member_joined" },
        { ChatEventActionType::MEMBER_JOINED_BY_INVITE_LINK, "member_joined_by_invite_link" },
        { ChatEventActionType::MEMBER_JOINED_BY_REQUEST, "member_joined_by_request" },
        { ChatEventActionType::MEMBER_INVITED, "member_invited" },
        { ChatEventActionType::MEMBER_LEFT, "member_left" },
        { ChatEventActionType::MEMBER_PROMOTED, "member_promoted" },
        { ChatEventActionType::MEMBER_RESTRICTED, "member_restricted" },
        { ChatEventActionType::MEMBER_TAG_CHANGED, "member_tag_changed" },
        { ChatEventActionType::MEMBER_SUBSCRIPTION_EXTENDED, "member_subscription_extended" },
        { ChatEventActionType::AVAILABLE_REACTIONS_CHANGED, "available_reactions_changed" },
        { ChatEventActionType::BACKGROUND_CHANGED, "background_changed" },
        { ChatEventActionType::DESCRIPTION_CHANGED, "description_changed" },
        { ChatEventActionType::EMOJI_STATUS_CHANGED, "emoji_status_changed" },
        { ChatEventActionType::LINKED_CHAT_CHANGED, "linked_chat_changed" },
        { ChatEventActionType::LOCATION_CHANGED, "location_changed" },
        { ChatEventActionType::MESSAGE_AUTO_DELETE_TIME_CHANGED, "message_auto_delete_time_changed" },
        { ChatEventActionType::PERMISSIONS_CHANGED, "permissions_changed" },
        { ChatEventActionType::PHOTO_CHANGED, "photo_changed" },
        { ChatEventActionType::SLOW_MODE_DELAY_CHANGED, "slow_mode_delay_changed" },
        { ChatEventActionType::STICKER_SET_CHANGED, "sticker_set_changed" },
        { ChatEventActionType::EMOJI_STICKER_SET_CHANGED, "emoji_sticker_set_changed" },
        { ChatEventActionType::TITLE_CHANGED, "title_changed" },
        { ChatEventActionType::USERNAME_CHANGED, "username_changed" },
        { ChatEventActionType::ACTIVE_USERNAMES_CHANGED, "active_usernames_changed" },
        { ChatEventActionType::ACCENT_COLOR_CHANGED, "accent_color_changed" },
        { ChatEventActionType::PROFILE_ACCENT_COLOR_CHANGED, "profile_accent_color_changed" },
        { ChatEventActionType::HAS_PROTECTED_CONTENT_TOGGLED, "has_protected_content_toggled" },
        { ChatEventActionType::INVITES_TOGGLED, "invites_toggled" },
        { ChatEventActionType::IS_ALL_HISTORY_AVAILABLE_TOGGLED, "is_all_history_available_toggled" },
        { ChatEventActionType::HAS_AGGRESSIVE_ANTI_SPAM_ENABLED_TOGGLED, "has_aggressive_anti_spam_enabled_toggled" },
        { ChatEventActionType::SIGN_MESSAGES_TOGGLED, "sign_messages_toggled" },
        { ChatEventActionType::SHOW_MESSAGE_SENDER_TOGGLED, "show_message_sender_toggled" },
        { ChatEventActionType::AUTOMATIC_TRANSLATION_TOGGLED, "automatic_translation_toggled" },
        { ChatEventActionType::INVITE_LINK_EDITED, "invite_link_edited" },
        { ChatEventActionType::INVITE_LINK_REVOKED, "invite_link_revoked" },
        { ChatEventActionType::INVITE_LINK_DELETED, "invite_link_deleted" },
        { ChatEventActionType::VIDEO_CHAT_CREATED, "video_chat_created" },
        { ChatEventActionType::VIDEO_CHAT_ENDED, "video_chat_ended" },
        { ChatEventActionType::VIDEO_CHAT_MUTE_NEW_PARTICIPANTS_TOGGLED, "video_chat_mute_new_participants_toggled" },
        { ChatEventActionType::VIDEO_CHAT_PARTICIPANT_IS_MUTED_TOGGLED, "video_chat_participant_is_muted_toggled" },
        { ChatEventActionType::VIDEO_CHAT_PARTICIPANT_VOLUME_LEVEL_CHANGED, "video_chat_participant_volume_level_changed" },
        { ChatEventActionType::IS_FORUM_TOGGLED, "is_forum_toggled" },
        { ChatEventActionType::FORUM_TOPIC_CREATED, "forum_topic_created" },
        { ChatEventActionType::FORUM_TOPIC_EDITED, "forum_topic_edited" },
        { ChatEventActionType::FORUM_TOPIC_TOGGLE_IS_CLOSED, "forum_topic_toggle_is_closed" },
        { ChatEventActionType::FORUM_TOPIC_TOGGLE_IS_HIDDEN, "forum_topic_toggle_is_hidden" },
        { ChatEventActionType::FORUM_TOPIC_DELETED, "forum_topic_deleted" },
        { ChatEventActionType::FORUM_TOPIC_PINNED, "forum_topic_pinned" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- ChatJoinRequestQueryResult (3 members) ----
struct ChatJoinRequestQueryResult {
    enum E : int {
        APPROVE = 0,
        DECLINE = 1,
        QUEUE = 2,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { ChatJoinRequestQueryResult::APPROVE, "joinChatBotResultApproved" },
        { ChatJoinRequestQueryResult::DECLINE, "joinChatBotResultDeclined" },
        { ChatJoinRequestQueryResult::QUEUE, "joinChatBotResultQueued" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- ChatJoinType (3 members) ----
struct ChatJoinType {
    enum E : int {
        BY_ADD = 0,
        BY_LINK = 1,
        BY_REQUEST = 2,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { ChatJoinType::BY_ADD, "by_add" },
        { ChatJoinType::BY_LINK, "by_link" },
        { ChatJoinType::BY_REQUEST, "by_request" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- ChatMemberStatus (6 members) ----
struct ChatMemberStatus {
    enum E : int {
        OWNER = 0,
        ADMINISTRATOR = 1,
        MEMBER = 2,
        RESTRICTED = 3,
        LEFT = 4,
        BANNED = 5,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { ChatMemberStatus::OWNER, "owner" },
        { ChatMemberStatus::ADMINISTRATOR, "administrator" },
        { ChatMemberStatus::MEMBER, "member" },
        { ChatMemberStatus::RESTRICTED, "restricted" },
        { ChatMemberStatus::LEFT, "left" },
        { ChatMemberStatus::BANNED, "banned" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- ChatMembersFilter (6 members) ----
struct ChatMembersFilter {
    enum E : int {
        SEARCH = 0,
        BANNED = 1,
        RESTRICTED = 2,
        BOTS = 3,
        RECENT = 4,
        ADMINISTRATORS = 5,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { ChatMembersFilter::SEARCH, "channelParticipantsSearch" },
        { ChatMembersFilter::BANNED, "channelParticipantsKicked" },
        { ChatMembersFilter::RESTRICTED, "channelParticipantsBanned" },
        { ChatMembersFilter::BOTS, "channelParticipantsBots" },
        { ChatMembersFilter::RECENT, "channelParticipantsRecent" },
        { ChatMembersFilter::ADMINISTRATORS, "channelParticipantsAdmins" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- ChatPhotoStickerType (2 members) ----
struct ChatPhotoStickerType {
    enum E : int {
        REGULAR_OR_MASK = 0,
        CUSTOM_EMOJI = 1,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { ChatPhotoStickerType::REGULAR_OR_MASK, "regular_or_mask" },
        { ChatPhotoStickerType::CUSTOM_EMOJI, "custom_emoji" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- ChatType (7 members) ----
struct ChatType {
    enum E : int {
        PRIVATE = 0,
        BOT = 1,
        GROUP = 2,
        SUPERGROUP = 3,
        CHANNEL = 4,
        FORUM = 5,
        DIRECT = 6,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { ChatType::PRIVATE, "private" },
        { ChatType::BOT, "bot" },
        { ChatType::GROUP, "group" },
        { ChatType::SUPERGROUP, "supergroup" },
        { ChatType::CHANNEL, "channel" },
        { ChatType::FORUM, "forum" },
        { ChatType::DIRECT, "direct" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- ClientPlatform (8 members) ----
struct ClientPlatform {
    enum E : int {
        ANDROID = 0,
        IOS = 1,
        WP = 2,
        BB = 3,
        DESKTOP = 4,
        WEB = 5,
        UBP = 6,
        OTHER = 7,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { ClientPlatform::ANDROID, "android" },
        { ClientPlatform::IOS, "ios" },
        { ClientPlatform::WP, "wp" },
        { ClientPlatform::BB, "bb" },
        { ClientPlatform::DESKTOP, "desktop" },
        { ClientPlatform::WEB, "web" },
        { ClientPlatform::UBP, "ubp" },
        { ClientPlatform::OTHER, "other" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- FolderColor (8 members) ----
struct FolderColor {
    enum E : int {
        NO_COLOR = 0,
        RED = 1,
        ORANGE = 2,
        VIOLET = 3,
        GREEN = 4,
        CYAN = 5,
        BLUE = 6,
        PINK = 7,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { FolderColor::NO_COLOR, "no_color" },
        { FolderColor::RED, "red" },
        { FolderColor::ORANGE, "orange" },
        { FolderColor::VIOLET, "violet" },
        { FolderColor::GREEN, "green" },
        { FolderColor::CYAN, "cyan" },
        { FolderColor::BLUE, "blue" },
        { FolderColor::PINK, "pink" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- GiftAttributeType (4 members) ----
struct GiftAttributeType {
    enum E : int {
        MODEL = 0,
        SYMBOL = 1,
        BACKDROP = 2,
        ORIGINAL_DETAILS = 3,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { GiftAttributeType::MODEL, "starGiftAttributeModel" },
        { GiftAttributeType::SYMBOL, "starGiftAttributePattern" },
        { GiftAttributeType::BACKDROP, "starGiftAttributeBackdrop" },
        { GiftAttributeType::ORIGINAL_DETAILS, "starGiftAttributeOriginalDetails" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- GiftForResaleOrder (3 members) ----
struct GiftForResaleOrder {
    enum E : int {
        PRICE = 0,
        CHANGE_DATE = 1,
        NUMBER = 2,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { GiftForResaleOrder::PRICE, "price" },
        { GiftForResaleOrder::CHANGE_DATE, "change_date" },
        { GiftForResaleOrder::NUMBER, "number" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- GiftPurchaseOfferState (3 members) ----
struct GiftPurchaseOfferState {
    enum E : int {
        PENDING = 0,
        ACCEPTED = 1,
        REJECTED = 2,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { GiftPurchaseOfferState::PENDING, "pending" },
        { GiftPurchaseOfferState::ACCEPTED, "accepted" },
        { GiftPurchaseOfferState::REJECTED, "rejected" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- GiftType (2 members) ----
struct GiftType {
    enum E : int {
        REGULAR = 0,
        UPGRADED = 1,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { GiftType::REGULAR, "regular" },
        { GiftType::UPGRADED, "upgraded" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- MaskPointType (4 members) ----
struct MaskPointType {
    enum E : int {
        FOREHEAD = 0,
        EYES = 1,
        MOUTH = 2,
        CHIN = 3,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { MaskPointType::FOREHEAD, "forehead" },
        { MaskPointType::EYES, "eyes" },
        { MaskPointType::MOUTH, "mouth" },
        { MaskPointType::CHIN, "chin" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- MediaAreaType (7 members) ----
struct MediaAreaType {
    enum E : int {
        POST = 0,
        LOCATION = 1,
        REACTION = 2,
        URL = 3,
        VENUE = 4,
        WEATHER = 5,
        GIFT = 6,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { MediaAreaType::POST, "mediaAreaChannelPost" },
        { MediaAreaType::LOCATION, "mediaAreaGeoPoint" },
        { MediaAreaType::REACTION, "mediaAreaSuggestedReaction" },
        { MediaAreaType::URL, "mediaAreaUrl" },
        { MediaAreaType::VENUE, "mediaAreaVenue" },
        { MediaAreaType::WEATHER, "mediaAreaWeather" },
        { MediaAreaType::GIFT, "mediaAreaStarGift" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- MessageEntityType (22 members) ----
struct MessageEntityType {
    enum E : int {
        MENTION = 0,
        HASHTAG = 1,
        CASHTAG = 2,
        BOT_COMMAND = 3,
        URL = 4,
        EMAIL = 5,
        PHONE_NUMBER = 6,
        BOLD = 7,
        ITALIC = 8,
        UNDERLINE = 9,
        STRIKETHROUGH = 10,
        SPOILER = 11,
        CODE = 12,
        PRE = 13,
        BLOCKQUOTE = 14,
        TEXT_LINK = 15,
        TEXT_MENTION = 16,
        BANK_CARD = 17,
        CUSTOM_EMOJI = 18,
        DATE_TIME = 19,
        TON_ADDRESS = 20,
        UNKNOWN = 21,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { MessageEntityType::MENTION, "messageEntityMention" },
        { MessageEntityType::HASHTAG, "messageEntityHashtag" },
        { MessageEntityType::CASHTAG, "messageEntityCashtag" },
        { MessageEntityType::BOT_COMMAND, "messageEntityBotCommand" },
        { MessageEntityType::URL, "messageEntityUrl" },
        { MessageEntityType::EMAIL, "messageEntityEmail" },
        { MessageEntityType::PHONE_NUMBER, "messageEntityPhone" },
        { MessageEntityType::BOLD, "messageEntityBold" },
        { MessageEntityType::ITALIC, "messageEntityItalic" },
        { MessageEntityType::UNDERLINE, "messageEntityUnderline" },
        { MessageEntityType::STRIKETHROUGH, "messageEntityStrike" },
        { MessageEntityType::SPOILER, "messageEntitySpoiler" },
        { MessageEntityType::CODE, "messageEntityCode" },
        { MessageEntityType::PRE, "messageEntityPre" },
        { MessageEntityType::BLOCKQUOTE, "messageEntityBlockquote" },
        { MessageEntityType::TEXT_LINK, "messageEntityTextUrl" },
        { MessageEntityType::TEXT_MENTION, "messageEntityMentionName" },
        { MessageEntityType::BANK_CARD, "messageEntityBankCard" },
        { MessageEntityType::CUSTOM_EMOJI, "messageEntityCustomEmoji" },
        { MessageEntityType::DATE_TIME, "messageEntityFormattedDate" },
        { MessageEntityType::TON_ADDRESS, "messageEntityTonAddress" },
        { MessageEntityType::UNKNOWN, "messageEntityUnknown" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- MessageMediaType (23 members) ----
struct MessageMediaType {
    enum E : int {
        UNSUPPORTED = 0,
        AUDIO = 1,
        DOCUMENT = 2,
        PHOTO = 3,
        LIVE_PHOTO = 4,
        STICKER = 5,
        VIDEO = 6,
        ANIMATION = 7,
        VOICE = 8,
        VIDEO_NOTE = 9,
        CONTACT = 10,
        LOCATION = 11,
        VENUE = 12,
        POLL = 13,
        WEB_PAGE = 14,
        DICE = 15,
        GAME = 16,
        GIVEAWAY = 17,
        GIVEAWAY_WINNERS = 18,
        STORY = 19,
        INVOICE = 20,
        PAID_MEDIA = 21,
        CHECKLIST = 22,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { MessageMediaType::UNSUPPORTED, "unsupported" },
        { MessageMediaType::AUDIO, "audio" },
        { MessageMediaType::DOCUMENT, "document" },
        { MessageMediaType::PHOTO, "photo" },
        { MessageMediaType::LIVE_PHOTO, "live_photo" },
        { MessageMediaType::STICKER, "sticker" },
        { MessageMediaType::VIDEO, "video" },
        { MessageMediaType::ANIMATION, "animation" },
        { MessageMediaType::VOICE, "voice" },
        { MessageMediaType::VIDEO_NOTE, "video_note" },
        { MessageMediaType::CONTACT, "contact" },
        { MessageMediaType::LOCATION, "location" },
        { MessageMediaType::VENUE, "venue" },
        { MessageMediaType::POLL, "poll" },
        { MessageMediaType::WEB_PAGE, "web_page" },
        { MessageMediaType::DICE, "dice" },
        { MessageMediaType::GAME, "game" },
        { MessageMediaType::GIVEAWAY, "giveaway" },
        { MessageMediaType::GIVEAWAY_WINNERS, "giveaway_winners" },
        { MessageMediaType::STORY, "story" },
        { MessageMediaType::INVOICE, "invoice" },
        { MessageMediaType::PAID_MEDIA, "paid_media" },
        { MessageMediaType::CHECKLIST, "checklist" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- MessageOriginType (5 members) ----
struct MessageOriginType {
    enum E : int {
        CHANNEL = 0,
        CHAT = 1,
        HIDDEN_USER = 2,
        IMPORT = 3,
        USER = 4,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { MessageOriginType::CHANNEL, "channel" },
        { MessageOriginType::CHAT, "chat" },
        { MessageOriginType::HIDDEN_USER, "hidden_user" },
        { MessageOriginType::IMPORT, "import" },
        { MessageOriginType::USER, "user" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- MessageServiceType (75 members) ----
struct MessageServiceType {
    enum E : int {
        UNSUPPORTED = 0,
        CUSTOM_ACTION = 1,
        NEW_CHAT_MEMBERS = 2,
        LEFT_CHAT_MEMBER = 3,
        CHAT_OWNER_LEFT = 4,
        CHAT_OWNER_CHANGED = 5,
        NEW_CHAT_TITLE = 6,
        NEW_CHAT_PHOTO = 7,
        DELETE_CHAT_PHOTO = 8,
        FORUM_TOPIC_CREATED = 9,
        FORUM_TOPIC_CLOSED = 10,
        FORUM_TOPIC_REOPENED = 11,
        FORUM_TOPIC_EDITED = 12,
        GENERAL_FORUM_TOPIC_HIDDEN = 13,
        GENERAL_FORUM_TOPIC_UNHIDDEN = 14,
        GROUP_CHAT_CREATED = 15,
        CHANNEL_CHAT_CREATED = 16,
        SUPERGROUP_CHAT_CREATED = 17,
        MIGRATE_TO_CHAT_ID = 18,
        MIGRATE_FROM_CHAT_ID = 19,
        PINNED_MESSAGE = 20,
        GAME_HIGH_SCORE = 21,
        GIVEAWAY_CREATED = 22,
        GIVEAWAY_COMPLETED = 23,
        MANAGED_BOT_CREATED = 24,
        POLL_OPTION_ADDED = 25,
        POLL_OPTION_DELETED = 26,
        PREMIUM_GIFT_CODE = 27,
        GIFTED_PREMIUM = 28,
        GIFTED_STARS = 29,
        GIFTED_GRAMS = 30,
        VIDEO_CHAT_STARTED = 31,
        VIDEO_CHAT_ENDED = 32,
        VIDEO_CHAT_SCHEDULED = 33,
        VIDEO_CHAT_MEMBERS_INVITED = 34,
        PHONE_CALL_STARTED = 35,
        PHONE_CALL_ENDED = 36,
        WEB_APP_DATA = 37,
        USERS_SHARED = 38,
        CHAT_SHARED = 39,
        SUCCESSFUL_PAYMENT = 40,
        REFUNDED_PAYMENT = 41,
        SUGGESTED_POST_APPROVAL_FAILED = 42,
        SUGGESTED_POST_APPROVED = 43,
        SUGGESTED_POST_DECLINED = 44,
        SUGGESTED_POST_PAID = 45,
        SUGGESTED_POST_REFUNDED = 46,
        SET_MESSAGE_AUTO_DELETE_TIME = 47,
        CHAT_BOOST = 48,
        GIFT = 49,
        CONNECTED_WEBSITE = 50,
        WRITE_ACCESS_ALLOWED = 51,
        SCREENSHOT_TAKEN = 52,
        CONTACT_REGISTERED = 53,
        PROXIMITY_ALERT_TRIGGERED = 54,
        HISTORY_CLEARED = 55,
        SUGGEST_PROFILE_PHOTO = 56,
        SUGGEST_BIRTHDAY = 57,
        CHAT_SET_BACKGROUND = 58,
        CHAT_SET_THEME = 59,
        GIVEAWAY_PRIZE_STARS = 60,
        PAID_MESSAGES_REFUNDED = 61,
        PAID_MESSAGES_PRICE_CHANGED = 62,
        DIRECT_MESSAGE_PRICE_CHANGED = 63,
        CHECKLIST_TASKS_DONE = 64,
        CHECKLIST_TASKS_ADDED = 65,
        COMMUNITY_CHAT_ADDED = 66,
        COMMUNITY_CHAT_REMOVED = 67,
        COMMUNITY_CHAT_JOINED = 68,
        UPGRADED_GIFT_PURCHASE_OFFER = 69,
        UPGRADED_GIFT_PURCHASE_OFFER_REJECTED = 70,
        CHAT_HAS_PROTECTED_CONTENT_TOGGLED = 71,
        CHAT_HAS_PROTECTED_CONTENT_DISABLE_REQUESTED = 72,
        TON_WALLET_TRANSFER = 73,
        TON_CONNECT_REQUEST = 74,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { MessageServiceType::UNSUPPORTED, "unsupported" },
        { MessageServiceType::CUSTOM_ACTION, "custom_action" },
        { MessageServiceType::NEW_CHAT_MEMBERS, "new_chat_members" },
        { MessageServiceType::LEFT_CHAT_MEMBER, "left_chat_member" },
        { MessageServiceType::CHAT_OWNER_LEFT, "chat_owner_left" },
        { MessageServiceType::CHAT_OWNER_CHANGED, "chat_owner_changed" },
        { MessageServiceType::NEW_CHAT_TITLE, "new_chat_title" },
        { MessageServiceType::NEW_CHAT_PHOTO, "new_chat_photo" },
        { MessageServiceType::DELETE_CHAT_PHOTO, "delete_chat_photo" },
        { MessageServiceType::FORUM_TOPIC_CREATED, "forum_topic_created" },
        { MessageServiceType::FORUM_TOPIC_CLOSED, "forum_topic_closed" },
        { MessageServiceType::FORUM_TOPIC_REOPENED, "forum_topic_reopened" },
        { MessageServiceType::FORUM_TOPIC_EDITED, "forum_topic_edited" },
        { MessageServiceType::GENERAL_FORUM_TOPIC_HIDDEN, "general_forum_topic_hidden" },
        { MessageServiceType::GENERAL_FORUM_TOPIC_UNHIDDEN, "general_forum_topic_unhidden" },
        { MessageServiceType::GROUP_CHAT_CREATED, "group_chat_created" },
        { MessageServiceType::CHANNEL_CHAT_CREATED, "channel_chat_created" },
        { MessageServiceType::SUPERGROUP_CHAT_CREATED, "supergroup_chat_created" },
        { MessageServiceType::MIGRATE_TO_CHAT_ID, "migrate_to_chat_id" },
        { MessageServiceType::MIGRATE_FROM_CHAT_ID, "migrate_from_chat_id" },
        { MessageServiceType::PINNED_MESSAGE, "pinned_message" },
        { MessageServiceType::GAME_HIGH_SCORE, "game_high_score" },
        { MessageServiceType::GIVEAWAY_CREATED, "giveaway_created" },
        { MessageServiceType::GIVEAWAY_COMPLETED, "giveaway_completed" },
        { MessageServiceType::MANAGED_BOT_CREATED, "managed_bot_created" },
        { MessageServiceType::POLL_OPTION_ADDED, "poll_option_added" },
        { MessageServiceType::POLL_OPTION_DELETED, "poll_option_deleted" },
        { MessageServiceType::PREMIUM_GIFT_CODE, "premium_gift_code" },
        { MessageServiceType::GIFTED_PREMIUM, "gifted_premium" },
        { MessageServiceType::GIFTED_STARS, "gifted_stars" },
        { MessageServiceType::GIFTED_GRAMS, "gifted_grams" },
        { MessageServiceType::VIDEO_CHAT_STARTED, "video_chat_started" },
        { MessageServiceType::VIDEO_CHAT_ENDED, "video_chat_ended" },
        { MessageServiceType::VIDEO_CHAT_SCHEDULED, "video_chat_scheduled" },
        { MessageServiceType::VIDEO_CHAT_MEMBERS_INVITED, "video_chat_members_invited" },
        { MessageServiceType::PHONE_CALL_STARTED, "phone_call_started" },
        { MessageServiceType::PHONE_CALL_ENDED, "phone_call_ended" },
        { MessageServiceType::WEB_APP_DATA, "web_app_data" },
        { MessageServiceType::USERS_SHARED, "users_shared" },
        { MessageServiceType::CHAT_SHARED, "chat_shared" },
        { MessageServiceType::SUCCESSFUL_PAYMENT, "successful_payment" },
        { MessageServiceType::REFUNDED_PAYMENT, "refunded_payment" },
        { MessageServiceType::SUGGESTED_POST_APPROVAL_FAILED, "suggested_post_approval_failed" },
        { MessageServiceType::SUGGESTED_POST_APPROVED, "suggested_post_approved" },
        { MessageServiceType::SUGGESTED_POST_DECLINED, "suggested_post_declined" },
        { MessageServiceType::SUGGESTED_POST_PAID, "suggested_post_paid" },
        { MessageServiceType::SUGGESTED_POST_REFUNDED, "suggested_post_refunded" },
        { MessageServiceType::SET_MESSAGE_AUTO_DELETE_TIME, "set_message_auto_delete_time" },
        { MessageServiceType::CHAT_BOOST, "chat_boost" },
        { MessageServiceType::GIFT, "gift" },
        { MessageServiceType::CONNECTED_WEBSITE, "connected_website" },
        { MessageServiceType::WRITE_ACCESS_ALLOWED, "write_access_allowed" },
        { MessageServiceType::SCREENSHOT_TAKEN, "screenshot_taken" },
        { MessageServiceType::CONTACT_REGISTERED, "contact_registered" },
        { MessageServiceType::PROXIMITY_ALERT_TRIGGERED, "proximity_alert_triggered" },
        { MessageServiceType::HISTORY_CLEARED, "history_cleared" },
        { MessageServiceType::SUGGEST_PROFILE_PHOTO, "suggest_profile_photo" },
        { MessageServiceType::SUGGEST_BIRTHDAY, "suggest_birthday" },
        { MessageServiceType::CHAT_SET_BACKGROUND, "chat_set_background" },
        { MessageServiceType::CHAT_SET_THEME, "chat_set_theme" },
        { MessageServiceType::GIVEAWAY_PRIZE_STARS, "giveaway_prize_stars" },
        { MessageServiceType::PAID_MESSAGES_REFUNDED, "paid_messages_refunded" },
        { MessageServiceType::PAID_MESSAGES_PRICE_CHANGED, "paid_messages_price_changed" },
        { MessageServiceType::DIRECT_MESSAGE_PRICE_CHANGED, "direct_message_price_changed" },
        { MessageServiceType::CHECKLIST_TASKS_DONE, "checklist_tasks_done" },
        { MessageServiceType::CHECKLIST_TASKS_ADDED, "checklist_tasks_added" },
        { MessageServiceType::COMMUNITY_CHAT_ADDED, "community_chat_added" },
        { MessageServiceType::COMMUNITY_CHAT_REMOVED, "community_chat_removed" },
        { MessageServiceType::COMMUNITY_CHAT_JOINED, "community_chat_joined" },
        { MessageServiceType::UPGRADED_GIFT_PURCHASE_OFFER, "upgraded_gift_purchase_offer" },
        { MessageServiceType::UPGRADED_GIFT_PURCHASE_OFFER_REJECTED, "upgraded_gift_purchase_offer_rejected" },
        { MessageServiceType::CHAT_HAS_PROTECTED_CONTENT_TOGGLED, "chat_has_protected_content_toggled" },
        { MessageServiceType::CHAT_HAS_PROTECTED_CONTENT_DISABLE_REQUESTED, "chat_has_protected_content_disable_requested" },
        { MessageServiceType::TON_WALLET_TRANSFER, "ton_wallet_transfer" },
        { MessageServiceType::TON_CONNECT_REQUEST, "ton_connect_request" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- MessagesFilter (18 members) ----
struct MessagesFilter {
    enum E : int {
        EMPTY = 0,
        PHOTO = 1,
        VIDEO = 2,
        PHOTO_VIDEO = 3,
        DOCUMENT = 4,
        URL = 5,
        ANIMATION = 6,
        VOICE_NOTE = 7,
        VIDEO_NOTE = 8,
        AUDIO_VIDEO_NOTE = 9,
        AUDIO = 10,
        CHAT_PHOTO = 11,
        PHONE_CALL = 12,
        MENTION = 13,
        LOCATION = 14,
        CONTACT = 15,
        PINNED = 16,
        POLL = 17,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { MessagesFilter::EMPTY, "inputMessagesFilterEmpty" },
        { MessagesFilter::PHOTO, "inputMessagesFilterPhotos" },
        { MessagesFilter::VIDEO, "inputMessagesFilterVideo" },
        { MessagesFilter::PHOTO_VIDEO, "inputMessagesFilterPhotoVideo" },
        { MessagesFilter::DOCUMENT, "inputMessagesFilterDocument" },
        { MessagesFilter::URL, "inputMessagesFilterUrl" },
        { MessagesFilter::ANIMATION, "inputMessagesFilterGif" },
        { MessagesFilter::VOICE_NOTE, "inputMessagesFilterVoice" },
        { MessagesFilter::VIDEO_NOTE, "inputMessagesFilterRoundVideo" },
        { MessagesFilter::AUDIO_VIDEO_NOTE, "inputMessagesFilterRoundVoice" },
        { MessagesFilter::AUDIO, "inputMessagesFilterMusic" },
        { MessagesFilter::CHAT_PHOTO, "inputMessagesFilterChatPhotos" },
        { MessagesFilter::PHONE_CALL, "inputMessagesFilterPhoneCalls" },
        { MessagesFilter::MENTION, "inputMessagesFilterMyMentions" },
        { MessagesFilter::LOCATION, "inputMessagesFilterGeo" },
        { MessagesFilter::CONTACT, "inputMessagesFilterContacts" },
        { MessagesFilter::PINNED, "inputMessagesFilterPinned" },
        { MessagesFilter::POLL, "inputMessagesFilterPoll" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- NextCodeType (5 members) ----
struct NextCodeType {
    enum E : int {
        CALL = 0,
        FLASH_CALL = 1,
        MISSED_CALL = 2,
        SMS = 3,
        FRAGMENT_SMS = 4,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { NextCodeType::CALL, "codeTypeCall" },
        { NextCodeType::FLASH_CALL, "codeTypeFlashCall" },
        { NextCodeType::MISSED_CALL, "codeTypeMissedCall" },
        { NextCodeType::SMS, "codeTypeSms" },
        { NextCodeType::FRAGMENT_SMS, "codeTypeFragmentSms" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- PaidReactionPrivacy (3 members) ----
struct PaidReactionPrivacy {
    enum E : int {
        DEFAULT = 0,
        ANONYMOUS = 1,
        CHAT = 2,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { PaidReactionPrivacy::DEFAULT, "paidReactionPrivacyDefault" },
        { PaidReactionPrivacy::ANONYMOUS, "paidReactionPrivacyAnonymous" },
        { PaidReactionPrivacy::CHAT, "paidReactionPrivacyPeer" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- ParseMode (4 members) ----
struct ParseMode {
    enum E : int {
        DEFAULT = 0,
        MARKDOWN = 1,
        HTML = 2,
        DISABLED = 3,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { ParseMode::DEFAULT, "default" },
        { ParseMode::MARKDOWN, "markdown" },
        { ParseMode::HTML, "html" },
        { ParseMode::DISABLED, "disabled" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- PaymentFormType (3 members) ----
struct PaymentFormType {
    enum E : int {
        REGULAR = 0,
        STARS = 1,
        STAR_SUBSCRIPTION = 2,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { PaymentFormType::REGULAR, "regular" },
        { PaymentFormType::STARS, "stars" },
        { PaymentFormType::STAR_SUBSCRIPTION, "star_subscription" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- PhoneCallDiscardReason (5 members) ----
struct PhoneCallDiscardReason {
    enum E : int {
        MISSED = 0,
        DECLINED = 1,
        DISCONNECTED = 2,
        HUNG_UP = 3,
        UPGRADE_TO_CONFERENCE_CALL = 4,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { PhoneCallDiscardReason::MISSED, "phoneCallDiscardReasonMissed" },
        { PhoneCallDiscardReason::DECLINED, "phoneCallDiscardReasonBusy" },
        { PhoneCallDiscardReason::DISCONNECTED, "phoneCallDiscardReasonDisconnect" },
        { PhoneCallDiscardReason::HUNG_UP, "phoneCallDiscardReasonHangup" },
        { PhoneCallDiscardReason::UPGRADE_TO_CONFERENCE_CALL, "phoneCallDiscardReasonMigrateConferenceCall" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- PhoneNumberCodeType (3 members) ----
struct PhoneNumberCodeType {
    enum E : int {
        AUTHENTICATION = 0,
        CHANGE = 1,
        VERIFY = 2,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { PhoneNumberCodeType::AUTHENTICATION, "authentication" },
        { PhoneNumberCodeType::CHANGE, "change" },
        { PhoneNumberCodeType::VERIFY, "verify" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- PollType (2 members) ----
struct PollType {
    enum E : int {
        QUIZ = 0,
        REGULAR = 1,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { PollType::QUIZ, "quiz" },
        { PollType::REGULAR, "regular" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- PrivacyKey (14 members) ----
struct PrivacyKey {
    enum E : int {
        ABOUT = 0,
        ADDED_BY_PHONE = 1,
        BIRTHDAY = 2,
        CHAT_INVITE = 3,
        FORWARDS = 4,
        PHONE_CALL = 5,
        PHONE_NUMBER = 6,
        PHONE_P2P = 7,
        PROFILE_PHOTO = 8,
        STATUS = 9,
        VOICE_MESSAGES = 10,
        GIFTS = 11,
        NO_PAID_MESSAGES = 12,
        SAVED_MUSIC = 13,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { PrivacyKey::ABOUT, "inputPrivacyKeyAbout" },
        { PrivacyKey::ADDED_BY_PHONE, "inputPrivacyKeyAddedByPhone" },
        { PrivacyKey::BIRTHDAY, "inputPrivacyKeyBirthday" },
        { PrivacyKey::CHAT_INVITE, "inputPrivacyKeyChatInvite" },
        { PrivacyKey::FORWARDS, "inputPrivacyKeyForwards" },
        { PrivacyKey::PHONE_CALL, "inputPrivacyKeyPhoneCall" },
        { PrivacyKey::PHONE_NUMBER, "inputPrivacyKeyPhoneNumber" },
        { PrivacyKey::PHONE_P2P, "inputPrivacyKeyPhoneP2P" },
        { PrivacyKey::PROFILE_PHOTO, "inputPrivacyKeyProfilePhoto" },
        { PrivacyKey::STATUS, "inputPrivacyKeyStatusTimestamp" },
        { PrivacyKey::VOICE_MESSAGES, "inputPrivacyKeyVoiceMessages" },
        { PrivacyKey::GIFTS, "inputPrivacyKeyStarGiftsAutoSave" },
        { PrivacyKey::NO_PAID_MESSAGES, "inputPrivacyKeyNoPaidMessages" },
        { PrivacyKey::SAVED_MUSIC, "inputPrivacyKeySavedMusic" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- PrivacyRuleType (12 members) ----
struct PrivacyRuleType {
    enum E : int {
        ALLOW_ALL = 0,
        ALLOW_BOTS = 1,
        ALLOW_CHAT_PARTICIPANTS = 2,
        ALLOW_CLOSE_FRIENDS = 3,
        ALLOW_CONTACTS = 4,
        ALLOW_PREMIUM = 5,
        ALLOW_USERS = 6,
        DISALLOW_ALL = 7,
        DISALLOW_BOTS = 8,
        DISALLOW_CHAT_PARTICIPANTS = 9,
        DISALLOW_CONTACTS = 10,
        DISALLOW_USERS = 11,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { PrivacyRuleType::ALLOW_ALL, "privacyValueAllowAll" },
        { PrivacyRuleType::ALLOW_BOTS, "privacyValueAllowBots" },
        { PrivacyRuleType::ALLOW_CHAT_PARTICIPANTS, "privacyValueAllowChatParticipants" },
        { PrivacyRuleType::ALLOW_CLOSE_FRIENDS, "privacyValueAllowCloseFriends" },
        { PrivacyRuleType::ALLOW_CONTACTS, "privacyValueAllowContacts" },
        { PrivacyRuleType::ALLOW_PREMIUM, "privacyValueAllowPremium" },
        { PrivacyRuleType::ALLOW_USERS, "privacyValueAllowUsers" },
        { PrivacyRuleType::DISALLOW_ALL, "privacyValueDisallowAll" },
        { PrivacyRuleType::DISALLOW_BOTS, "privacyValueDisallowBots" },
        { PrivacyRuleType::DISALLOW_CHAT_PARTICIPANTS, "privacyValueDisallowChatParticipants" },
        { PrivacyRuleType::DISALLOW_CONTACTS, "privacyValueDisallowContacts" },
        { PrivacyRuleType::DISALLOW_USERS, "privacyValueDisallowUsers" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- ProfileTab (8 members) ----
struct ProfileTab {
    enum E : int {
        POSTS = 0,
        GIFTS = 1,
        MEDIA = 2,
        FILES = 3,
        LINKS = 4,
        MUSIC = 5,
        VOICE = 6,
        GIFS = 7,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { ProfileTab::POSTS, "profileTabPosts" },
        { ProfileTab::GIFTS, "profileTabGifts" },
        { ProfileTab::MEDIA, "profileTabMedia" },
        { ProfileTab::FILES, "profileTabFiles" },
        { ProfileTab::LINKS, "profileTabLinks" },
        { ProfileTab::MUSIC, "profileTabMusic" },
        { ProfileTab::VOICE, "profileTabVoice" },
        { ProfileTab::GIFS, "profileTabGifs" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- ProxyScheme (5 members) ----
struct ProxyScheme {
    enum E : int {
        SOCKS4 = 0,
        SOCKS5 = 1,
        HTTP = 2,
        MTPROXY = 3,
        WEB = 4,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { ProxyScheme::SOCKS4, "socks4" },
        { ProxyScheme::SOCKS5, "socks5" },
        { ProxyScheme::HTTP, "http" },
        { ProxyScheme::MTPROXY, "mtproxy" },
        { ProxyScheme::WEB, "web" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- SentCodeType (11 members) ----
struct SentCodeType {
    enum E : int {
        APP = 0,
        CALL = 1,
        FLASH_CALL = 2,
        MISSED_CALL = 3,
        SMS = 4,
        FRAGMENT_SMS = 5,
        EMAIL_CODE = 6,
        FIREBASE_SMS = 7,
        SETUP_EMAIL_REQUIRED = 8,
        SMS_PHRASE = 9,
        SMS_WORD = 10,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { SentCodeType::APP, "sentCodeTypeApp" },
        { SentCodeType::CALL, "sentCodeTypeCall" },
        { SentCodeType::FLASH_CALL, "sentCodeTypeFlashCall" },
        { SentCodeType::MISSED_CALL, "sentCodeTypeMissedCall" },
        { SentCodeType::SMS, "sentCodeTypeSms" },
        { SentCodeType::FRAGMENT_SMS, "sentCodeTypeFragmentSms" },
        { SentCodeType::EMAIL_CODE, "sentCodeTypeEmailCode" },
        { SentCodeType::FIREBASE_SMS, "sentCodeTypeFirebaseSms" },
        { SentCodeType::SETUP_EMAIL_REQUIRED, "sentCodeTypeSetUpEmailRequired" },
        { SentCodeType::SMS_PHRASE, "sentCodeTypeSmsPhrase" },
        { SentCodeType::SMS_WORD, "sentCodeTypeSmsWord" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- StickerFormat (3 members) ----
struct StickerFormat {
    enum E : int {
        STATIC = 0,
        ANIMATED = 1,
        VIDEO = 2,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { StickerFormat::STATIC, "static" },
        { StickerFormat::ANIMATED, "animated" },
        { StickerFormat::VIDEO, "video" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- StickerType (3 members) ----
struct StickerType {
    enum E : int {
        REGULAR = 0,
        MASK = 1,
        CUSTOM_EMOJI = 2,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { StickerType::REGULAR, "regular" },
        { StickerType::MASK, "mask" },
        { StickerType::CUSTOM_EMOJI, "custom_emoji" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- StoriesPrivacyRules (4 members) ----
struct StoriesPrivacyRules {
    enum E : int {
        PUBLIC = 0,
        CONTACTS = 1,
        CLOSE_FRIENDS = 2,
        SELECTED_USERS = 3,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { StoriesPrivacyRules::PUBLIC, "public" },
        { StoriesPrivacyRules::CONTACTS, "contacts" },
        { StoriesPrivacyRules::CLOSE_FRIENDS, "close_friends" },
        { StoriesPrivacyRules::SELECTED_USERS, "selected_users" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- SuggestedPostRefundReason (2 members) ----
struct SuggestedPostRefundReason {
    enum E : int {
        POST_DELETED = 0,
        PAYMENT_REFUNDED = 1,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { SuggestedPostRefundReason::POST_DELETED, "post_deleted" },
        { SuggestedPostRefundReason::PAYMENT_REFUNDED, "payment_refunded" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- SuggestedPostState (3 members) ----
struct SuggestedPostState {
    enum E : int {
        PENDING = 0,
        APPROVED = 1,
        DECLINED = 2,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { SuggestedPostState::PENDING, "pending" },
        { SuggestedPostState::APPROVED, "approved" },
        { SuggestedPostState::DECLINED, "declined" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- TopChatCategory (9 members) ----
struct TopChatCategory {
    enum E : int {
        USERS = 0,
        BOTS = 1,
        GROUPS = 2,
        CHANNELS = 3,
        INLINE_BOTS = 4,
        GUEST_BOTS = 5,
        WEB_APP_BOTS = 6,
        CALLS = 7,
        FORWARD_CHATS = 8,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { TopChatCategory::USERS, "users" },
        { TopChatCategory::BOTS, "bots" },
        { TopChatCategory::GROUPS, "groups" },
        { TopChatCategory::CHANNELS, "channels" },
        { TopChatCategory::INLINE_BOTS, "inline_bots" },
        { TopChatCategory::GUEST_BOTS, "guest_bots" },
        { TopChatCategory::WEB_APP_BOTS, "web_app_bots" },
        { TopChatCategory::CALLS, "calls" },
        { TopChatCategory::FORWARD_CHATS, "forward_chats" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- UpgradedGiftOrigin (7 members) ----
struct UpgradedGiftOrigin {
    enum E : int {
        UPGRADE = 0,
        TRANSFER = 1,
        RESALE = 2,
        BLOCKCHAIN = 3,
        GIFTED_UPGRADE = 4,
        OFFER = 5,
        CRAFT = 6,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { UpgradedGiftOrigin::UPGRADE, "upgrade" },
        { UpgradedGiftOrigin::TRANSFER, "transfer" },
        { UpgradedGiftOrigin::RESALE, "resale" },
        { UpgradedGiftOrigin::BLOCKCHAIN, "blockchain" },
        { UpgradedGiftOrigin::GIFTED_UPGRADE, "gifted_upgrade" },
        { UpgradedGiftOrigin::OFFER, "offer" },
        { UpgradedGiftOrigin::CRAFT, "craft" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

// ---- UserStatus (6 members) ----
struct UserStatus {
    enum E : int {
        ONLINE = 0,
        OFFLINE = 1,
        RECENTLY = 2,
        LAST_WEEK = 3,
        LAST_MONTH = 4,
        LONG_AGO = 5,
    };

    static constexpr std::pair<E, std::string_view> table[] = {
        { UserStatus::ONLINE, "online" },
        { UserStatus::OFFLINE, "offline" },
        { UserStatus::RECENTLY, "recently" },
        { UserStatus::LAST_WEEK, "last_week" },
        { UserStatus::LAST_MONTH, "last_month" },
        { UserStatus::LONG_AGO, "long_ago" },
    };

    static constexpr E kUnset = static_cast<E>(-1);

    // Enum -> TL constructor name.
    static std::string tl_name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return std::string(tl);
        return {};
    }

    // Enum -> friendly (lowercase) name.
    static constexpr std::string_view name(E v) {
        for (const auto& [e, tl] : table)
            if (e == v) return tl;
        return {};
    }

    // TL constructor or friendly name -> enum, kUnset when unknown.
    static E from_tl(std::string_view s) {
        return from_tl_impl<E, table, sizeof(table) / sizeof(table[0])>(s, kUnset);
    }

    static constexpr int count() { return int(sizeof(table) / sizeof(table[0])); }
};

}  // namespace cppgram::enums
