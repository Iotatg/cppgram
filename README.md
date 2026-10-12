<div align="center">

<a href="https://github.com/Iotatg/cppgram">
<img src="assets/cppgram-logo.png" alt="cppgram" width="220">
</a>

<br/>
<br/>

<img src="https://readme-typing-svg.demolab.com?font=JetBrains+Mono&weight=600&size=24&pause=1200&color=58A6FF&center=true&vCenter=true&width=640&lines=MTProto+2.0+user-client+in+C%2B%2B23;DH+handshake+%C2%B7+2FA+%C2%B7+voice+chats;No+TDLib.+No+Python.+No+bot+API." alt="typing banner" />

<br/>
<br/>

<img src="https://img.shields.io/badge/C%2B%2B-23-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white" alt="C++23"/>
<img src="https://img.shields.io/github/v/release/Iotatg/cppgram?style=for-the-badge&color=brightgreen" alt="release"/>
<img src="https://img.shields.io/badge/tests-5%2F5%20passing-brightgreen?style=for-the-badge&logo=githubactions&logoColor=white" alt="tests"/>
<img src="https://img.shields.io/badge/TL%20layer-229-blue?style=for-the-badge" alt="TL layer 229"/>
<img src="https://img.shields.io/badge/Linux-x86__64%20%7C%20ARM64-informational?style=for-the-badge&logo=linux&logoColor=white" alt="Linux"/>
<img src="https://img.shields.io/badge/license-MIT-green?style=for-the-badge" alt="MIT license"/>

<br/>
<br/>

A small, dependency-light **MTProto Telegram user-client library in C++23** — made for the jobs the Bot API cannot do: userbot assistants, voice-chat (group call) control, raw TL RPC on a user session.

Born inside [AdvaitMusic](https://github.com/Iotatg/AdvaitMusic) as its voice-chat transport, now standalone.

</div>

---

## Why

TDLib is big, official, and bot-centric; Pyrogram/Telethon are Python. cppgram is a compact native alternative that:

- does the **full MTProto 2.0 auth-key DH handshake itself** (`generate_auth_key()`) — no pre-existing session needed
- authenticates with an existing session too (`pyrogram` StringSession or `gogram` string session), and exports both formats
- speaks MTProto 2.0 over TCP directly (AES-IGE + SHA1/256 via OpenSSL, optional gzip answers)
- layer **229** requests, CRCs verified against the current tdesktop `api.tl`
- works with **zero access hashes** — modern Telegram resolves bare ids for known peers

---

## Capabilities

| Area | API |
|---|---|
| Handshake | `generate_auth_key(ip, port, dc)` — resPQ/RSA/DH, all four production DCs |
| Login | `auth_send_code`, `auth_sign_in`, `auth_sign_up`, `auth_check_password` (SRP 2FA), `auth_cancel_code`, `auth_log_out` |
| Session | `parse_session_string()`, `session_string(pyrogram_format)` — pyrogram & gogram formats |
| Transport | initConnection handshake, BadServerSalt retry, gzip unwrap, container updates |
| Dialogs | `refresh_dialog_peers(limit)`, `get_dialogs(limit)`, `get_dialogs_full(limit)` |
| Text messages | `send_message_text()`, `send_markdown()`, `edit_message_text()`, `delete_messages()`, `read_history()`, `set_typing()`, `forward_messages()` |
| Entities | `parse_markdown()` — `**bold**`, `__italic__`, `*`/`_` italic aliases, `--underline--`, `~~strike~~`, `\|\|spoiler\|\|`, `` `code` ``, ` ```pre``` `, `[text](url)` (UTF-16 offsets) |
| History | `get_history()`, `fetch_history_full()`, `fetch_messages()`, `search_messages()` |
| Media | `send_photo_message()`, `send_document_message()`, `send_document_media()`, `download_user_photo()`, `download_media_msg()`, `download_location()` |
| Reactions/pins | `set_reaction()`, `pin_message()` |
| Chats | `get_chat()`, `join_chat()`, `leave_chat()`, `get_chat_members()`, `ban/unban/promote_chat_member()` |
| Updates | `poll_updates()` (pushed) + `poll_difference()` getDifference loop |
| Voice chats | `get_group_call()`, `join_group_call()` — powers AdvaitMusic playback today |
| Handlers | 31 typed `on_*` handlers (message, edited, deleted, callback, reaction, boost, pre_checkout, shipping, join request, poll, user status, typing, story, inline, business connection, raw update, error, connect/disconnect) |
| Peer cache | `save_peer_cache()`, `load_peer_cache()` — JSON access-hash cache |
| Kurigram parity | 259 extra wrappers in `src/parity_wrappers.cpp` — checklists, polls, gifs, suggested posts, AI text ops, drafts, media groups, paid media, video notes, checklists, stories, gifts, collections, auctions, resale, subscriptions, invoices, sticker sets, business accounts, folders, invite links, invite boosters, chat colors, direct-message topics |
| Enums | 44 kurigram enums / 401 members in `include/cppgram/enums.hpp` — table-driven, `constexpr`, TL-name and friendly-name lookups |
| Filters | `include/cppgram/filters.hpp` + `src/filters.cpp` — 76 named filters (media, chat type, service, text, keyboard, command, regex) plus `create`/`custom`/`pred`/`all_of`/`any_of`/`negate` |
| Parsers | `Parser::parse()/unparse()` (`Default`/`Markdown`/`MarkdownStrict`/`Html`/`Disabled`), `Markdown::parse()/unparse()`, `Html::parse()/unparse()` — UTF-16 offsets, byte-exact against kurigram |
| Utils | `include/cppgram/utils.hpp` + `src/utils.cpp` — peer id math, star/nano amounts, datetimes, `split_text`, JSON helpers, SHA-256/PBKDF2-SHA512 (SRP kdf), URL-safe base64, RLE, inline message ids |

---

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Requires a **C++23** compiler, OpenSSL (`libssl-dev`), zlib.

---

## Usage walkthrough

**1. Build it**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
```

**2. Generate an auth key — no phone, no SMS**

```bash
./build/cg_tool - keygen 2
```

**3. Log in interactively** (state files under `/tmp/opencode/login-<phone>.*`)

```bash
./build/cg_tool - login-sendcode +15551234567 <api_id> <api_hash>
./build/cg_tool - login-signin 15551234567 55441      # the SMS code
./build/cg_tool - login-2fa    15551234567 <password>  # only if 2FA is on
```

The flow follows phone-migration errors (`303 PHONE_MIGRATE_x`) automatically, so a DC change mid-login does not break it.

**4. The tool prints a session string.** Save it — it is your credential from now on.

**5. Use it**

```bash
./build/cg_tool /path/to/session.txt getme
./build/cg_tool /path/to/session.txt send -1001234567890 "hello from cppgram"
```

**6. Or drop into C++**

```cpp
auto session = cppgram::Client::parse_session_string(getenv("MY_SESSION"));
cppgram::Client client(session, cppgram::ClientConfig{});
std::string err;
if (!client.connect(&err)) throw std::runtime_error(err);
```

---

## Command-line tool

Any command that needs an authorized session takes a session string inline or as a file path.

```bash
# generate an auth key (no phone, no SMS) and verify it with a real RPC
./build/cg_tool - keygen 2
./build/cg_tool - keytest 2 <api_id> <api_hash>

# interactive login (state files under /tmp/opencode/login-<phone>.*)
./build/cg_tool - login-sendcode +15551234567 <api_id> <api_hash> [dc] [prefix]
./build/cg_tool - login-signin <prefix> <code_from_sms>
./build/cg_tool - login-signup <prefix> <code> <first> <last>
./build/cg_tool - login-2fa <prefix> <password>
./build/cg_tool - login-cancel <prefix>
./build/cg_tool - login-logout /path/to/session.txt

# authorized commands
./build/cg_tool /path/to/session.txt dialogs 100
./build/cg_tool /path/to/session.txt getme
./build/cg_tool /path/to/session.txt send -1001234567890 "hello from cppgram"
./build/cg_tool /path/to/session.txt sendmd -1001234567890 "**bold** and [link](https://x)"
./build/cg_tool /path/to/session.txt fwd -1001234567890 42 -1001987654321
./build/cg_tool /path/to/session.txt edit -1001234567890 42 "edited"
./build/cg_tool /path/to/session.txt del -1001234567890 42
./build/cg_tool /path/to/session.txt react -1001234567890 42 fire
./build/cg_tool /path/to/session.txt pin -1001234567890 42
./build/cg_tool /path/to/session.txt read -1001234567890
./build/cg_tool /path/to/session.txt search -1001234567890 keyword
./build/cg_tool /path/to/session.txt history -1001234567890 10
./build/cg_tool /path/to/session.txt dlphoto 8663044027 avatar.jpg
./build/cg_tool /path/to/session.txt dlmsg -1001234567890 42 file.bin
./build/cg_tool /path/to/session.txt export-session [pyrogram]
./build/cg_tool /path/to/session.txt peers-save peers.json
./build/cg_tool /path/to/session.txt peers-load peers.json
./build/cg_tool /path/to/session.txt watch 30 -1001234567890

# parity: raw escape hatch covers every TL RPC in the schema table
./build/cg_tool /path/to/session.txt invoke messages.getSearchCounters '{"peer":{"_ctor":"inputPeerSelf"}}'
./build/cg_tool /path/to/session.txt recovergaps -1001234567890 0 100

# parity: stories / stickers / gifts / payments / bots / chats
./build/cg_tool /path/to/session.txt getstories -1001234567890 0
./build/cg_tool /path/to/session.txt pinstory -1001234567890 1,2,3
./build/cg_tool /path/to/session.txt delstory -1001234567890 42
./build/cg_tool /path/to/session.txt chatgifts -1001234567890 20 0
./build/cg_tool /path/to/session.txt giftcollections -1001234567890
./build/cg_tool /path/to/session.txt searchresalegifts 0 keyword
./build/cg_tool /path/to/session.txt paymentform '{"_ctor":"inputInvoiceMessage","peer":{"_ctor":"inputPeerSelf"},"msg_id":1}'
./build/cg_tool /path/to/session.txt chatphoto -1001234567890 avatar.jpg
./build/cg_tool /path/to/session.txt chatperms -1001234567890 '{"_ctor":"chatBannedRights","send_messages":true}'
./build/cg_tool /path/to/session.txt linkjoiners -1001234567890 https://t.me/+abc 50
./build/cg_tool /path/to/session.txt onlinenum -1001234567890
```

Chat ids use Bot-API "marked" form: `-100xxxx` channels, `-xxx` basic groups, `+id` users.

After a successful login the tool prints the session string (gogram format by default); `export-session pyrogram` prints the pyrogram-compatible one. Both are mutually readable.

---

## Library usage

```cpp
#include "cppgram/client.hpp"

// full handshake, no existing session needed
std::string key, err;
if (!cppgram::generate_auth_key("149.154.167.50", 443, 2, &key, &err)) throw std::runtime_error(err);

// or resume from a session string
auto session = cppgram::Client::parse_session_string(getenv("MY_SESSION"));
cppgram::Client client(session, cppgram::ClientConfig{});
if (!client.connect(&err)) throw std::runtime_error(err);

cppgram::Peer p;
if (client.peer_for(-1002853048978, &p, &err)) {
    client.send_message_text(p, "hello", /*reply_to=*/0, &err);
}
```

Updates arrive on `poll_updates()` (server-pushed) or `poll_difference()` (long-poll); group-call signalling uses the same channel.

Note: after a long offline period the server replays a push backlog (paced ~1 batch/round-trip) before live events resume; long-running connections see events in real time.

---

## Tests

```bash
ctest --test-dir build --output-on-failure
```

| Test | Covers |
|---|---|
| `json_strict` | 81 corpus cases + 20 000-case LCG fuzz of the strict JSON parser |
| `session_roundtrip` | gogram JSON + legacy `1BvX` + pyrogram strings, 7 malformed inputs |
| `wire_shape` | CRCs and nesting of edit/sendMedia/sendCode against `schema/api.tl` |
| `srp_shape` | real SRP prime, checks the bytes reaching the encoder |
| `parser_parity` | html / markdown / parser byte-exact against kurigram |

---

## Debugging

`CG_DEBUG=1` prints TL response metadata; `CG_DUMP=file` writes the last raw RPC result; `CG_DUMP_API=file` / `CG_DUMP_RESP=file` capture the request/response bodies of the last `api()` call; `CG_DRY_RUN=1` encodes and dumps the TL without contacting the server; `CG_TRACE` is reserved.

---

## Status

Live-verified end to end: DH handshake on all four production DCs, phone-code login path, session import/export (both formats), send/edit/delete/forward/pin/react/read, markdown entities, dialogs, history, search, media upload/download, peer-cache round-trip, update polling, stories/stickers/gifts/payments probes (`historycount`, `effects`, `onlinenum`, `recentstickers`, `similarchats`). The parser layer (`html`/`markdown`/`parser`) is verified byte-exact against the kurigram reference implementation. Powers AdvaitMusic's voice-chat playback today.

---

## License

MIT

<div align="center">
<img src="https://capsule-render.vercel.app/api?type=waving&color=gradient&customColorList=6&height=90&section=footer" alt="footer wave" />
</div>
