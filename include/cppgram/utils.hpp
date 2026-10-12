// kurigram `pyrogram/utils` parity: free helper functions shared by the raw and
// high-level layers (peers, amounts, datetimes, text, TL json values, crypto
// primitives, base64/RLE and inline message ids).
//
// Every function mirrors the same-named one in kurigram, same semantics:
//   cppgram::utils::get_peer_type(-102853048978) == "channel"
//   cppgram::utils::to_nano(1.5) == 1500000000
//   cppgram::utils::split_text(text, 4096) -> vector<string>
#pragma once

#include "cppgram/args.hpp"
#include "cppgram/types.hpp"

#include <cstdint>
#include <ctime>
#include <optional>
#include <string>
#include <vector>

namespace cppgram::utils {

namespace peers {

constexpr int64_t ZERO_SECRET_CHAT_ID = -2000000000000;
constexpr int64_t ZERO_CHANNEL_ID = -1000000000000;
constexpr int64_t MAX_CHANNEL_ID = 1000000000000 - (1LL << 31);
constexpr int64_t MIN_MONOFORUM_CHANNEL_ID = 1000000000000 + (1LL << 31) + 1;
constexpr int64_t MAX_MONOFORUM_CHANNEL_ID = 3000000000000;
constexpr int64_t MAX_USER_ID = (1LL << 40) - 1;
constexpr int64_t MAX_CHAT_ID = 999999999999;

// Raw id out of a marked (Bot API style) peer id. Returns nullopt for invalid ids,
// mirroring kurigram's `get_raw_peer_id` returning None.
std::optional<int64_t> get_raw_peer_id(int64_t peer);

// Non-raw (marked) peer id out of a Peer/InputPeer/RequestedPeer object tree.
// The tree may be any of the constructors kurigram accepts; the id attribute is
// read from the TL constructor's field names.
std::optional<int64_t> get_peer_id(const Val& peer);

std::string get_peer_type(int64_t peer_id);

inline int64_t get_channel_id(int64_t peer_id) { return ZERO_CHANNEL_ID - peer_id; }

}  // namespace peers

namespace amounts {

inline double from_nano(int64_t nano) { return static_cast<double>(nano) / 1e9; }
inline int64_t to_nano(double amount) { return static_cast<int64_t>(amount * 1e9); }
inline int get_premium_duration_month_count(int day_count) {
    const int v = day_count / 30;
    return v > 1 ? v : 1;
}
int get_premium_duration_day_count(int month_count);

}  // namespace amounts

namespace datetimes {

inline std::time_t zero_datetime() { return 0; }
inline std::time_t max_datetime() { return (1LL << 31) - 1; }
std::optional<std::time_t> timestamp_to_datetime(int64_t ts);
std::optional<int64_t> datetime_to_timestamp(std::optional<std::time_t> dt);
// Timedelta flavour: now + seconds.
std::optional<int64_t> datetime_to_timestamp(int64_t from_now_seconds);

}  // namespace datetimes

// ---------------- text ----------------
std::vector<std::string> split_text(const std::string& text, size_t max_length = 4096);
std::optional<std::string> get_first_url(const std::string& text);

// ---------------- TL JSON values ----------------
// obj_to_jsonvalue returns the raw.types.Json* constructor tree for a Val
// holding a json value (int/double/bool/string/list/object); jsonvalue_to_obj
// does the opposite direction. Null has no Val, so it is represented by a
// JsonNull ctor node with no fields.
Val obj_to_jsonvalue(const Val& obj);
Val jsonvalue_to_obj(const Val& obj);

// ---------------- crypto ----------------
std::string sha256(const std::string& data);
int64_t btoi(const std::string& b);
std::string itob(int64_t i);
std::string bytes_xor(const std::string& a, const std::string& b);
std::string pbkdf2_sha512(const std::string& password, const std::string& salt, int iterations);
std::string compute_password_hash(const Val& algo, const std::string& password);

// ---------------- base64 / RLE ----------------
std::string b64_encode(const std::string& s);
std::string b64_decode(const std::string& s);
std::string rle_encode(const std::string& s);
std::string rle_decode(const std::string& s);

// ---------------- inline message ids ----------------
// Takes/returns InputBotInlineMessageID / InputBotInlineMessageID64 trees.
std::string pack_inline_message_id(const Val& msg_id);
Val unpack_inline_message_id(const std::string& inline_message_id);

// ---------------- file ids ----------------
int file_type_of(const std::string& media_ctor_or_file_type_name);

}  // namespace cppgram::utils
