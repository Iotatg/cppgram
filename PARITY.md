# cppgram API Parity

cppgram reaches gogram/pyrogram parity through two complementary layers:

1. **Schema engine (complete, universal coverage)** — `scripts/gen_schema.py` parses the
   official Telegram TL schema (layer 229, `schema/api.tl`) into a static table:
   **2542 constructors, 839 RPC functions, 681 types, 8139 fields**. Every request
   constructor and every response constructor in Telegram's API can be encoded/decoded
   and called:

   ```cpp
   Val r = client.api("messages.getDialogs", parse_json("{\"limit\":10}", nullptr), &err);
   ```

   This single entry point covers the entire surface of gogram's 1168 client methods
   and pyrogram's 163 (gogram methods are 1:1 wrappers over the same RPCs).

2. **High-level wrappers (pyrogram surface, mapped below)** — convenience methods on
   `Client` mirroring pyrogram/gogram ergonomics. Anything not wrapped is a direct
   `api()`/`api_json()` call with identical JSON arg shapes.

## High-level wrappers implemented (live-verified against production DCs)

| pyrogram / gogram | cppgram | status |
|---|---|---|
| Client.get_me | `get_me()` | live |
| Client.resolve_username | `resolve_username()` | live |
| Client.get_dialogs | `get_dialogs_full()` | live (full decode incl. big entities) |
| Client.get_chat | `get_chat(peer, full)` | live |
| Client.get_chat_members | `get_chat_members()` | live (99 members, all access_hash) |
| Client.join_chat / leave_chat | `join_chat()` / `leave_chat()` | built |
| Client.ban_chat_member / unban_chat_member | `ban_chat_member()` / `unban_chat_member()` | built |
| Client.promote_chat_member | `promote_chat_member()` | built |
| Client.get_messages | `fetch_messages()` | live |
| Client.get_chat_history | `fetch_history_full()` | live |
| Client.search_messages | `search_messages()` | live |
| Client.send_message | `send_message()` (messages.cpp) | live |
| Client.send_message (entities) | `send_markdown()` + `parse_markdown()` | live (pyrogram markdown set, UTF-16 offsets) |
| Client.edit_message_text | `edit_message_text()` | live |
| Client.delete_messages | `delete_messages()` | live |
| Client.send_chat_action (typing) | `send_chat_action()` | live |
| Client.forward_messages | `forward_messages()` | live (`id:Vector<int>` shape) |
| Client.pin_message | `pin_message()` | live |
| Client.set_reaction | `set_reaction()` | live-encoded (msg_reaction#8b65...) — test-account restricted to premium |
| Client.send_photo | `send_photo_message()` | live e2e (upload + sendMedia + decode echo) |
| Client.send_document | `send_document_message()` | live e2e (1.3 MB multipart, byte-identical) |
| Client.send_voice | `send_voice_message()` | live e2e (OGG/Opus + attributes) |
| Client.send_video | `send_video_message()` | live e2e (MP4 + streaming attrs) |
| download_media (photos) | `download_user_photo()` | live e2e (largest size type, JPEG verified) |
| download_media (documents) | `download_document_msg()` + `download_location()` | live e2e (byte-identical) |
| Client.upload_file (low-level) | `upload_file()` (saveFilePart + MD5) | live |
| Client.on_message (+ filters) | `on_message(MessageFilter) / off_message` | live (real pushed update) |
| idle() | `wait_updates(seconds)` | live |
| sync raw RPC | `invoke_raw()` / `api()` / `api_json()` | live |

## Deliberate raw-`api()` equivalents (pyrogram convenience sugar not duplicated)

All of these are one `client.api(...)` call; the engine does encoding, flag-word
management, and full response decoding with field ergonomics:

- chats/folders: `messages.getFilteredDialogs`, `contacts.block/unblock`,
  `messages.toggleDialogPin`, `contacts.addContact/deleteContacts`, `messages.readHistory`
- stickers/GIF: `messages.sendMedia` with `inputMediaDocument` + `inputDocument`
  (upload path for new stickers: `stickers.addStickerSet`, `messages.uploadStickerFile`)
- stories, business, bots payment, polls, bot commands menus, groups/forums/topics:
  every RPC exists by name in the engine table (see `schema/api.tl`)
- auth flows: `auth.sendCode/signIn/signUp/logOut/exportAuthorization/...` — all wrapped
  (`auth_send_code`, `auth_sign_in`, `auth_sign_up`, `auth_check_password` SRP-2FA,
  `auth_cancel_code`, `auth_log_out`) and live-verified; interactive CLI login
  (`cg_tool - login-*`) implemented and handshake verified on all production DCs

## Media specifics discovered against production

- `upload.saveFilePart` takes 512 KB parts; MD5 of the whole file is required on
  `inputFile`.
- Profile photos: `thumb_size` must be a real size type present in the photo's
  `sizes` (e.g. `a/b/c/x...`); empty string is answered `FILE_REFERENCE_EXPIRED`.
- `file_reference` values rotate — always use the freshest response's reference.
- `photos.getUserPhotos` self returns empty for account photos; use contacts/users
  responses for `access_hash` seeding (`contacts.getContacts` fallback built into
  `download_user_photo`).

## Update pipeline

`scan_message_events` decodes the Updates container with the schema engine and fans
out every inner Update by constructor name. Message handlers still receive a complete
`Message` object (`id/chat_id/from_id/text/date/raw Val`). Typed handlers (kurigram
decorator set) are registered via `on_edited_message` / `on_deleted_messages` /
`on_callback_query` / `on_message_reaction` / `on_chat_member_updated` /
`on_chat_join_request` / `on_poll` / `on_user_status` / `on_typing` / `on_story` /
`on_inline_query` / `on_chosen_inline_result` / `on_raw_update` / `on_error` /
`on_connect` / `on_disconnect`. The layer-230 `updateBot*` set is also wired
(`on_chat_boost`, `on_message_reaction_count`, `on_pre_checkout_query`,
`on_shipping_query`, `on_purchased_paid_media`, `on_business_connection`,
`on_deleted_business_messages`, `on_edited_business_message`, `on_managed_bot`,
`on_guest_message`, `on_stopped_message_generation`, `on_business_message`,
`on_start`, `on_stop`), plus `on_error`. `pts` tracking + `msgs_ack` keep the push
stream live.

## Kurigram-parity wrappers (`src/wrappers.cpp` + `src/parity_wrappers.cpp`)

Thin `api()` sugar covering the kurigram method surface (`src/wrappers.cpp`) plus the
full gap-closing batch (`src/parity_wrappers.cpp`, 259 declarations). Together they
cover **441/441 kurigram methods** (alias map for renamed pyrogram/kurigram names,
5 pyrogram-only framework names excluded). Shared helpers live in parity_wrappers.cpp:
`pj()`, `ich()`, `iuser()`, `iuser_peer()`, `esc()`, `nlist<T>()`, `frag()`, `jj()`,
`peer_color()`, `rnd_id()`, `reply_msg()`. Live-verified:

| group | methods | status |
|---|---|---|
| chats | set_chat_title/description/username, slow_mode, ttl, protected_content, archive, mark_unread, create_channel/group, delete_channel, get_chat_member, members_count, dialogs_count, restrict, unpin_all, admin title, event_log, forum topics, transfer_ownership | built (title/about/archive/topics cmds on cg_tool) |
| messages | send_poll, stop_poll, vote/retract, send_dice/contact/location/venue/sticker, copy_message, scheduled, read_mentions/reactions, view_messages, discussion, delete_history, send_chat_action | live: poll, dice, location |
| contacts/users | get_contacts, add/delete/search/import, block/unblock, update_profile, set_username, update_status, common_chats, chat_photos, delete_profile_photos, set_emoji_status | live: contacts |
| auth/account | get_password_hint, get_active_sessions, reset_sessions, sign_in_bot, get/set privacy, get/set account_ttl | live: sessions, privacy, ttl |
| invite links | export/create/revoke invite, approve/decline join request, get_join_requests | built |
| folders | get_folders, delete/join/leave folder | live: folders |
| stories | get_all_stories, get_chat_stories, read/delete stories | live: allstories |
| stickers | get_sticker_set, search_stickers, get_favorite_stickers | built |
| bots | answer_* (callback/inline/web_app/pre_checkout/shipping/guest/join-request), get/set bot_info (name, description, short_description), get_bot_commands, create_bot, get_managed_bot_token, set_managed_bot_access_settings, get/set bot default privileges, get/set chat menu button, send_game, set_game_score, get_game_high_scores, answer_terms_of_service | built |
| messages (parity) | checklists, poll options, gifs (add_to_gifs), suggested posts, AI (compose/summarize/translate/fix), drafts, media groups, cached/paid media, video note, live photo, live_photo, send_web_page, get/send web_app urls, stream_media, search_posts/global, get_available_effects | built |
| chats (parity) | add_chat_members, create/delete supergroup, set_chat_photo (uploaded), set_chat_permissions, set_send_as_chat, set_chat_member_tag, get/set profile accent color, get_chat_online_count, get_similar_channels, get_top_chats, update_chat_notifications, get/set direct-message topics, toggle_forum_topics, unarchive_chats | built |
| payments (parity) | gift collections (CRUD + get_chat_gifts), upgrade/transfer/convert/craft/drop-auction offers, resale (search/resell), subscriptions (edit/reuse/refund), get_payment_form + send_payment_form, gift premium, suggest_birthday, toggle_gift_pinned, hide_gift | built |
| stickers (parity) | owned/recent/favorite CRUD, add/remove sticker, create/delete set, install/reorder, set title/position/emoji/keywords/mask, upload, replace, custom emoji | built |
| stories (parity) | get_stories/archived/pinned, views, edit caption/media/privacy, delete, pin/unpin, hide, stealth mode, forward/copy, can_post | built |
| chats/invite links (parity) | edit/delete invite link, joiners + count, approve/decline all join requests, chats for folder invite, check folder invite link | built |
| folders (parity) | toggle_folder_tags, reorder_folders, get_chats_for_folder_invite_link | built |
| contacts (parity) | get_contacts_count, get_blocked_message_senders, set_contact_note, delete_user_history | built |
| account (parity) | set_profile_photo (jpeg+video), update_birthday, add/remove profile audio, global privacy settings, inactive session TTL, set_privacy, recover_password, send_recovery_code, change_phone_number | built |
| business (parity) | delete_business_messages, get_business_account_gifts, get/transfer business star balance, get_business_connection | built |
| advanced (parity) | invoke(method, json), recover_gaps(channel, pts, limit), resolve_peer(query), save_file (upload.saveBigFilePart) | built |
| premium/payments | apply_boost, get_boosts/status, get_stars_balance, get_available_gifts, send_gift | live: stars, gifts |
| layer 230 leftovers | get_app_config, get_ton_balance, send_rich_message, set_bot_profile_photo, edit_folder_invite_link, get_call_members, cloud-password enable/change/remove, set_bot_commands, send_story | live: appconfig, ton (starsTonAmount); sendrich encoded (server RICH_MESSAGE_UNSUPPORTED on this account) |

Everything else remains one `client.api("rpc.name", args)` call (839 RPCs in the engine).

### Auth & session layer

| step | cppgram | status |
|---|---|---|
| generate_auth_key (resPQ/DH) | `generate_auth_key(ip, port, dc)` | live on dc1/dc2/dc4/dc5 (~1.7 s) |
| auth.sendCode | `auth_send_code()` | live |
| auth.signIn / signUp | `auth_sign_in()` / `auth_sign_up()` | built |
| 2FA checkPassword (SRP) | `auth_check_password()` | built (client: same code path as login CLI; upstream hand-verified) |
| session import (pyrogram/gogram) | `parse_session_string()` | live |
| session export (both formats) | `session_string(pyrogram_format)` | live (round-trip 66-peer cache intact) |
| key sanity check | `cg_tool - keytest <dc> <api_id> <api_hash>` — DH exchange + encrypted `help.getConfig` | live (3056 B answer, no SMS) |

The DH handshake follows gogram's formulas: `new_nonce_hash1 =
sha1(new_nonce + 0x01 + sha1(auth_key)[0:8])[4:20]`; `server_DH_inner_data#b5890dba`
integrity hash covers only the unpadded TL structure. `tmp_aes_key =
sha1(nn+sn) + sha1(sn+nn)[0:12]`, `tmp_aes_iv = sha1(sn+nn)[12:20] + sha1(nn+nn) +
nn[0:4]`.

Note: `BN_mod_exp(..., nullptr)` segfaults on this toolchain's OpenSSL (3.0.22),
so `keygen.cpp` allocates a `BN_CTX` (CtxGuard RAII) at every call site.

## Remaining for full parity

1. `auth_check_password` unverified against a real 2FA account (code review only).
   SRP now follows kurigram's exact kdf (`sha256(s1+pwd+s1)`, `sha256(s2+h+s2)`,
   PBKDF2-HMAC-SHA512 100000 rounds, `sha256(s2+h+s2)`) — see `utils::compute_password_hash`.
2. Formatter-layer sugar (`message.reply(...)` style) — trivial over existing wrappers.
3. MTProxy / FakeTLS transport (kurigram `connection/proxy.py`, `crypto/faketls.py`).
4. Rich bound types (`message.reply()`, `chat.members`) — wrappers take `Peer` + ids instead.
5. ~~Payments/gifts deep flows (upgrade/transfer/auction) stay as raw `api()` calls.~~
   (implemented in `src/parity_wrappers.cpp`, see payments table above).
6. Bound Python types (`InputRichMessage`, `TonWalletTransfer`, `ChatEvent` filters) stay as decoded `Val` trees; TON_ADDRESS entity ctor `messageEntityTonAddress#fdabbfcd` is already in the schema engine.
7. `FileId` / `FileUniqueId` (TDLib 4-version binary layout) and `SQLiteStorage` stay deferred —
   no natural `Val`-JSON home and a separate persistence layer.
8. `Html::unparse()` emits `<blockquote expandable>` from the raw `collapsed` flag, whereas
   kurigram's `parse_one` checks `entity.expandable` that raw never sets (documented divergence).
   Likewise a malformed `<tg-emoji id="...">` (no `emoji-id`) makes kurigram raise `TypeError`
   while cppgram reports `document_id=0`.

## Parser layer parity (`src/html.cpp`, `src/markdown.cpp`, `src/parser.cpp`)

Verified byte-exact against the kurigram clone (`pyrogram/parser/*.py`) with a
248-line corpus (62 inputs x 4 parse modes) for parse, 19 fixtures for unparse
and 30 html cases; every case matches on message text, entity order, offsets,
lengths and extras.

- `html.unescape` runs **twice** on text data (CPython's `convert_charrefs`
  tokenizer pass plus kurigram's `handle_data`), once on attribute values.
- Leading/trailing whitespace survives only around a wrapping tag, using
  kurigram's two `re.sub`s: `^\s*(<[\w<>=\s"]*>)\s*` and `\s*(</[\w</>]*>)\s*$`.
  The tag class excludes `/`, so a `</tag>` tail ends the run and the regex
  backtracks to the last `>` inside it — hence `<pre language="x">` keeps the
  whitespace it wraps while bare text is untouched.
- Markdown is implemented by folding to html and delegating to `Html::parse()`,
  exactly like kurigram, so there is a single formatting engine.
- Quote folding runs before the delimiter pass; matches are collected once
  (like `re.finditer`) because every `replace_once` changes string length.
- `Parser::parse()` strips the text (`str(text or "").strip()`) before dispatch;
  the per-format parsers do not.
