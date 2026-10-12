// cg_tool — command-line driver for cppgram.
// usage: cg_tool <session-string-or-file> <command> [args]
//   info              session summary + connection check (help-free: updates.getState)
//   dialogs [limit]   refresh and list known peers
//   send <chat_id> <text> [reply_to]
//   sendmd <chat_id> <markdown> [reply_to]   (entities via markdown parser)
//   edit <chat_id> <msg_id> <text>
//   del  <chat_id> <msg_id>
//   typing <chat_id>
//   fwd <from_chat> <msg_id...> <to_chat>
//   dlmsg <chat_id> <msg_id> <outfile>       (largest photo/document/video)
//   react <chat_id> <msg_id> <emoji>
//   pin <chat_id> <msg_id> [off]             (toggleDialogPin)
//   read <chat_id> [max_id]                  (messages.readHistory)
//   search <query> [limit]                   (messages.searchGlobal)
//   ban <chat_id> <user_id> [off]
//   promote <chat_id> <user_id> [admin|revoke]
//   logout
//   export-session [pyrogram]                (print session string, persist it)
//   peers-dump | peers-save <path> | peers-load <path>
//   contacts | block <uid> | unblock <uid> | profile <first> <last> <about>
//   invite <chat> | settitle <chat> <title> | setabout <chat> <about>
//   poll <chat> <q> <opt...> | dice <chat> [emoji] | location <chat> <lat> <lon>
//   contact <chat> <phone> <first> <last>
//   folders | stories <chat> | allstories | stickerset <name> | favstickers
//   sessions | privacy <key> | ttl | stars | gifts | boosts <chat>
//   archive/unarchive <chat> | topics <chat> | createtopic <chat> <title>
//   joinreq <chat>
//   appconfig | ton | sendrich <chat> <markdown>
//
// Authentication (no session yet):
//   cg_tool - keygen <dc>
//   cg_tool - keytest <dc> <api_id> <api_hash>   (keygen + encrypted help.getConfig proof)
//   cg_tool - login-sendcode <phone> <api_id> <api_hash> [dc] [prefix]
//   cg_tool - login-signin <prefix> <code>
//   cg_tool - login-signup <prefix> <code> <first_name> <last_name>
//   cg_tool - login-2fa <prefix> <password>
//   cg_tool - login-cancel <prefix>
//   cg_tool - login-logout <session-file>
#include "cppgram/client.hpp"
#include "cppgram/enums.hpp"

#include <cstdio>
#include <thread>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

using namespace cppgram;

namespace {

std::string read_file_bytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

inline std::string read_file(const std::string& path) { return read_file_bytes(path); }

inline std::vector<long long> split_ids(const std::string& text, size_t* pos) {
    std::vector<long long> out;
    size_t start = *pos ? *pos : 0;
    for (;;) {
        size_t comma = text.find(',', start);
        std::string tok = text.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        while (!tok.empty() && std::isspace((unsigned char)tok.front())) tok.erase(tok.begin());
        while (!tok.empty() && std::isspace((unsigned char)tok.back())) tok.pop_back();
        if (!tok.empty() && tok.find("-.") != 0) out.push_back(std::atoll(tok.c_str()));
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    *pos = text.size();
    return out;
}

bool write_file_bytes(const std::string& path, const std::string& data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(out);
}

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

bool resolve_peer(Client& client, long long chat, Peer* out, std::string* err) {
    if (client.peer_for(chat, out, err)) return true;
    // One dialog refresh covers everything the account ever talked to.
    std::string we;
    client.refresh_dialog_peers(150, &we);
    err->clear();
    return client.peer_for(chat, out, err);
}

std::string hex_encode(const std::string& raw) {
    static const char* hx = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : raw) {
        out += hx[c >> 4];
        out += hx[c & 15];
    }
    return out;
}

std::string hex_decode(const std::string& hex) {
    auto v = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::string out;
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        int a = v(hex[i]), b = v(hex[i + 1]);
        if (a < 0 || b < 0) return {};
        out.push_back(static_cast<char>((a << 4) | b));
    }
    return out;
}

// ------------------------------- login state files

struct LoginState {
    bool ok{false};
    std::string phone, code_hash, api_hash;
    int dc{2};
    int32_t api_id{0};
    std::string key_hex;
};

std::string login_prefix(const std::string& phone) { return "/tmp/opencode/login-" + phone; }

LoginState load_login_state(const std::string& prefix) {
    LoginState st;
    std::string js = read_file_bytes(prefix + ".login.json");
    std::string key = trim(read_file_bytes(prefix + ".key"));
    if (js.empty() || key.empty()) return st;
    std::string err;
    Val v = parse_json(js, &err);
    if (!err.empty()) return st;
    st.phone = v.text("phone");
    st.code_hash = v.text("code_hash");
    st.api_hash = v.text("api_hash");
    st.dc = static_cast<int>(v.num("dc"));
    st.api_id = static_cast<int32_t>(v.num("api_id"));
    st.key_hex = key;
    st.ok = !st.phone.empty() && !st.code_hash.empty() && st.key_hex.size() == 512 && st.dc > 0;
    return st;
}

bool save_login_state(const std::string& prefix, const LoginState& st) {
    std::string js = "{\"phone\":" + std::string("\"") + st.phone + "\"," +
                     "\"code_hash\":\"" + st.code_hash + "\"," +
                     "\"api_hash\":\"" + st.api_hash + "\"," +
                     "\"dc\":" + std::to_string(st.dc) + "," +
                     "\"api_id\":" + std::to_string(st.api_id) + "}";
    return write_file_bytes(prefix + ".login.json", js) && write_file_bytes(prefix + ".key", st.key_hex);
}

Client login_client(const LoginState& st) {
    Session ses;
    ses.dc_id = st.dc;
    ses.ip = Client::dc_ip(st.dc);
    ses.api_id = st.api_id;
    ses.auth_key = hex_decode(st.key_hex);
    return Client(ses, ClientConfig{});
}

int run_login_commands(int argc, char** argv) {
    std::string cmd = argv[2];
    std::string err;

    if (cmd == "keygen" && argc >= 4) {
        int dc = std::atoi(argv[3]);
        std::string ip = Client::dc_ip(dc);
        if (ip.empty()) { std::fprintf(stderr, "unknown dc %d\n", dc); return 1; }
        std::string key;
        if (!generate_auth_key(ip, 443, dc, &key, &err)) {
            std::fprintf(stderr, "keygen dc%d failed: %s\n", dc, err.c_str());
            return 1;
        }
        std::printf("auth_key dc%d (%zu bytes): %s\n", dc, key.size(), hex_encode(key).c_str());
        return 0;
    }

    // keytest <dc> <api_id> <api_hash>: generate an auth key and immediately
    // use it for an encrypted help.getConfig call (no login/SMS involved).
    if (cmd == "keytest" && argc >= 6) {
        int dc = std::atoi(argv[3]);
        std::string ip = Client::dc_ip(dc);
        if (ip.empty()) { std::fprintf(stderr, "unknown dc %d\n", dc); return 1; }
        std::string key;
        if (!generate_auth_key(ip, 443, dc, &key, &err)) {
            std::fprintf(stderr, "keygen dc%d failed: %s\n", dc, err.c_str());
            return 1;
        }
        Session ses;
        ses.dc_id = dc;
        ses.ip = ip;
        ses.port = 443;
        ses.auth_key = key;
        ses.api_id = static_cast<int32_t>(std::atoi(argv[4]));
        Client client(ses, ClientConfig{});
        if (!client.connect(&err)) {
            std::fprintf(stderr, "connect failed: %s\n", err.c_str());
            return 1;
        }
        Val args = Val::obj("help.getConfig");
        auto cfg = client.api("help.getConfig", args, &err);
        if (!err.empty()) {
            std::fprintf(stderr, "help.getConfig failed: %s\n", err.c_str());
            return 1;
        }
        std::printf("auth_key dc%d accepted; getConfig %zu bytes (dc_options=%s)\n", dc,
                    cppgram::to_json(cfg).size(),
                    cppgram::to_json(cfg.at("dc_options")).substr(0, 60).c_str());
        return 0;
    }

    if (cmd == "login-sendcode" && argc >= 6) {
        std::string phone = argv[3];
        int32_t api_id = static_cast<int32_t>(std::atoi(argv[4]));
        std::string api_hash = argv[5];
        int dc = argc >= 7 ? std::atoi(argv[6]) : 2;
        std::string prefix = argc >= 8 ? argv[7] : login_prefix(phone);
        std::string ip = Client::dc_ip(dc);
        if (ip.empty()) { std::fprintf(stderr, "unknown dc %d\n", dc); return 1; }
        std::string key;
        if (!generate_auth_key(ip, 443, dc, &key, &err)) {
            std::fprintf(stderr, "handshake failed: %s\n", err.c_str());
            return 1;
        }
        Session ses;
        ses.dc_id = dc;
        ses.ip = ip;
        ses.api_id = api_id;
        ses.auth_key = key;
        Client client(ses, ClientConfig{});
        if (!client.connect(&err)) {
            std::fprintf(stderr, "connect failed: %s\n", err.c_str());
            return 1;
        }
        Val sent = client.auth_send_code(phone, api_id, api_hash, &err);
        if (!err.empty()) { std::fprintf(stderr, "sendCode failed: %s\n", err.c_str()); return 1; }
        std::string code_hash = sent.text("phone_code_hash");
        if (code_hash.empty()) {
            std::fprintf(stderr, "sendCode: no phone_code_hash in %s\n%s\n", sent.name.c_str(),
                         to_json(sent).c_str());
            return 1;
        }
        // auth_send_code may have moved the session to another DC on a
        // PHONE_MIGRATE_x, so the persisted state has to come from the client
        // after the call rather than from what we keyed before it.
        const Session& after = client.session();
        LoginState st;
        st.phone = phone;
        st.code_hash = code_hash;
        st.api_hash = api_hash;
        st.dc = after.dc_id;
        st.api_id = after.api_id;
        st.key_hex = hex_encode(after.auth_key);
        dc = after.dc_id;
        ip = after.ip;
        if (!save_login_state(prefix, st)) { std::fprintf(stderr, "cannot write login state\n"); return 1; }
        std::printf("sentCode type=%s phone_code_hash=%s (state: %s)\n", sent.name.c_str(),
                    code_hash.c_str(), prefix.c_str());
        return 0;
    }

    if (cmd == "login-logout" && argc >= 4) {
        std::string raw = argv[3];
        if (std::ifstream in(raw); in) {
            std::stringstream ss;
            ss << in.rdbuf();
            raw = trim(ss.str());
        }
        Session ses;
        try {
            ses = Client::parse_session_string(raw);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "session parse failed: %s\n", e.what());
            return 1;
        }
        Client client(ses, ClientConfig{});
        if (!client.connect(&err)) { std::fprintf(stderr, "connect failed: %s\n", err.c_str()); return 1; }
        client.auth_log_out(&err);
        std::printf("logged out (%s)\n", err.empty() ? "ok" : err.c_str());
        return 0;
    }

    LoginState st = load_login_state(argc >= 4 ? argv[3] : "");
    if (!st.ok) { std::fprintf(stderr, "no usable login state for prefix %s\n", argc >= 4 ? argv[3] : "?"); return 1; }

    if (cmd == "login-signin" && argc >= 5) {
        Client client = login_client(st);
        if (!client.connect(&err)) { std::fprintf(stderr, "connect failed: %s\n", err.c_str()); return 1; }
        if (!client.auth_sign_in(st.phone, st.code_hash, argv[4], &err)) {
            std::fprintf(stderr, "signIn failed: %s\n", err.c_str());
            return 1;
        }
        std::string ss = client.session_string();
        write_file_bytes(std::string(argv[3]) + ".session", ss);
        client.save_peer_cache(std::string(argv[3]) + ".peers.json", &err);
        std::printf("signed in user_id=%lld\nsession: %s\n",
                    static_cast<long long>(client.session().user_id), ss.c_str());
        return 0;
    }

    if (cmd == "login-signup" && argc >= 7) {
        Client client = login_client(st);
        if (!client.connect(&err)) { std::fprintf(stderr, "connect failed: %s\n", err.c_str()); return 1; }
        if (!client.auth_sign_up(st.phone, st.code_hash, argv[5], argv[6], &err)) {
            std::fprintf(stderr, "signUp failed: %s\n", err.c_str());
            return 1;
        }
        std::string ss = client.session_string();
        write_file_bytes(std::string(argv[3]) + ".session", ss);
        std::printf("signed up user_id=%lld\nsession: %s\n",
                    static_cast<long long>(client.session().user_id), ss.c_str());
        return 0;
    }

    if (cmd == "login-2fa" && argc >= 5) {
        Client client = login_client(st);
        if (!client.connect(&err)) { std::fprintf(stderr, "connect failed: %s\n", err.c_str()); return 1; }
        if (!client.auth_check_password(argv[4], &err)) {
            std::fprintf(stderr, "checkPassword failed: %s\n", err.c_str());
            return 1;
        }
        std::string ss = client.session_string();
        write_file_bytes(std::string(argv[3]) + ".session", ss);
        std::printf("2FA ok user_id=%lld\nsession: %s\n",
                    static_cast<long long>(client.session().user_id), ss.c_str());
        return 0;
    }

    if (cmd == "login-cancel") {
        Client client = login_client(st);
        if (!client.connect(&err)) { std::fprintf(stderr, "connect failed: %s\n", err.c_str()); return 1; }
        if (!client.auth_cancel_code(st.phone, &err)) {
            std::fprintf(stderr, "cancelCode failed: %s\n", err.c_str());
            return 1;
        }
        std::printf("login request cancelled\n");
        return 0;
    }

    std::fprintf(stderr, "unknown login command: %s\n", cmd.c_str());
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: cg_tool <session-string|file> <command> [args]\n");
        return 2;
    }
    std::string cmd = argv[2];
    if (cmd.rfind("login-", 0) == 0 || cmd == "keygen" || cmd == "keytest") return run_login_commands(argc, argv);

    std::string raw = argv[1];
    std::string peer_cache;
    if (std::ifstream in(raw); in) {
        std::stringstream ss;
        ss << in.rdbuf();
        raw = ss.str();
        while (!raw.empty() && (raw.back() == '\n' || raw.back() == '\r' || raw.back() == ' ')) raw.pop_back();
        peer_cache = std::string(argv[1]) + ".peers.json";
    }
    Session sess;
    try {
        sess = Client::parse_session_string(raw);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "session parse failed: %s\n", e.what());
        return 1;
    }
    Client client(sess, ClientConfig{});
    std::string err;
    std::printf("session: dc=%d ip=%s user=%lld key=%zuB\n", sess.dc_id, sess.ip.c_str(),
                static_cast<long long>(sess.user_id), sess.auth_key.size());
    if (!client.connect(&err)) {
        std::fprintf(stderr, "connect failed: %s\n", err.c_str());
        return 1;
    }
    std::printf("connected + initialized\n");
    if (!peer_cache.empty()) client.load_peer_cache(peer_cache, &err);

    int rc = 2;
    if (cmd == "info") {
        std::printf("dc=%d ip=%s user=%lld key=%zuB peer_cache=%s\n", sess.dc_id, sess.ip.c_str(),
                    static_cast<long long>(sess.user_id), sess.auth_key.size(),
                    peer_cache.empty() ? "none" : "loaded");
        rc = 0;
    }
    if (cmd == "getme") {
        std::string e2;
        auto me = client.get_me(&e2);
        if (!e2.empty()) { std::fprintf(stderr, "getme: %s\n", e2.c_str()); rc = 1; }
        else { std::printf("%s\n", cppgram::to_json(me).c_str()); rc = 0; }
    }
    if (cmd == "resolve" && argc >= 4) {
        std::string e2;
        auto r = client.resolve_username(argv[3], &e2);
        if (!e2.empty()) { std::fprintf(stderr, "resolve: %s\n", e2.c_str()); rc = 1; }
        else { std::printf("%s\n", cppgram::to_json(r).c_str()); rc = 0; }
    }
    if (cmd == "chat" && argc >= 4) {
        std::string e2;
        cppgram::Peer pe;
        if (!resolve_peer(client, std::atoll(argv[3]), &pe, &e2)) {
            std::fprintf(stderr, "peer: %s\n", e2.c_str());
            rc = 1;
        } else {
            auto r = client.get_chat(pe, argc < 5 || std::string(argv[4]) != "brief", &e2);
            if (!e2.empty()) { std::fprintf(stderr, "chat: %s\n", e2.c_str()); rc = 1; }
            else { std::printf("%s\n", cppgram::to_json(r).c_str()); rc = 0; }
        }
    }
    if (cmd == "members" && argc >= 4) {
        std::string e2;
        cppgram::Peer pe;
        if (!resolve_peer(client, std::atoll(argv[3]), &pe, &e2)) {
            std::fprintf(stderr, "peer: %s\n", e2.c_str());
            rc = 1;
        } else {
            auto r = client.get_chat_members(pe, 0, argc >= 5 ? std::atoi(argv[4]) : 5, &e2);
            if (!e2.empty()) { std::fprintf(stderr, "members: %s\n", e2.c_str()); rc = 1; }
            else { std::printf("%s\n", cppgram::to_json(r).c_str()); rc = 0; }
        }
    }
    if (cmd == "history" && argc >= 4) {
        std::string e2;
        cppgram::Peer pe;
        if (!resolve_peer(client, std::atoll(argv[3]), &pe, &e2)) {
            std::fprintf(stderr, "peer: %s\n", e2.c_str());
            rc = 1;
        } else {
            auto r = client.fetch_history_full(pe, argc >= 5 ? std::atoi(argv[4]) : 3, 0, &e2);
            if (!e2.empty()) { std::fprintf(stderr, "history: %s\n", e2.c_str()); rc = 1; }
            else { std::printf("%s\n", cppgram::to_json(r).c_str()); rc = 0; }
        }
    }
    if (cmd == "api" && argc >= 4) {
        std::string argjson = argc >= 5 ? argv[4] : "";
        std::string e2;
        cppgram::Val res = client.api_json(argv[3], argjson, &e2);
        if (!e2.empty()) {
            std::fprintf(stderr, "api failed: %s\n", e2.c_str());
            rc = 1;
        } else {
            std::printf("%s\n", cppgram::to_json(res).c_str());
            rc = 0;
        }
    }
    if (cmd == "dialogs") {
        int limit = argc > 3 ? std::atoi(argv[3]) : 100;
        if (!client.refresh_dialog_peers(limit, &err)) {
            std::fprintf(stderr, "getDialogs failed: %s\n", err.c_str());
            rc = 1;
        } else {
            for (auto& d : client.get_dialogs(limit, &err)) {
                std::printf("%-10lld %s id=%lld ah=%016llx\n",
                            static_cast<long long>(d.peer.marked()),
                            d.peer.kind == PeerKind::Channel
                                ? "channel"
                                : (d.peer.kind == PeerKind::User ? "user" : "chat"),
                            static_cast<long long>(d.peer.id),
                            static_cast<unsigned long long>(d.peer.access_hash));
            }
            rc = 0;
        }
    }
    if (cmd == "send" && argc >= 5) {
        int64_t chat = std::atoll(argv[3]);
        Peer p;
        if (!resolve_peer(client, chat, &p, &err)) {
            std::fprintf(stderr, "peer resolve failed: %s\n", err.c_str());
            rc = 1;
        } else {
            int64_t reply = argc > 5 ? std::atoll(argv[5]) : 0;
            int64_t id = client.send_message_text(p, argv[4], reply, &err);
            if (id < 0) {
                std::fprintf(stderr, "send failed: %s\n", err.c_str());
                rc = 1;
            } else {
                std::printf("sent message_id=%lld\n", static_cast<long long>(id));
                rc = 0;
            }
        }
    }
    if (cmd == "sendmd" && argc >= 5) {
        int64_t chat = std::atoll(argv[3]);
        Peer p;
        if (!resolve_peer(client, chat, &p, &err)) {
            std::fprintf(stderr, "peer resolve failed: %s\n", err.c_str());
            rc = 1;
        } else {
            int64_t reply = argc > 5 ? std::atoll(argv[5]) : 0;
            std::string e2;
            client.send_markdown(p, argv[4], reply, &e2);
            if (!e2.empty()) { std::fprintf(stderr, "sendmd failed: %s\n", e2.c_str()); rc = 1; }
            else { std::printf("sent (markdown)\n"); rc = 0; }
        }
    }
    if (cmd == "edit" && argc >= 6) {
        int64_t chat = std::atoll(argv[3]);
        int64_t mid = std::atoll(argv[4]);
        Peer p;
        if (!resolve_peer(client, chat, &p, &err)) {
            std::fprintf(stderr, "peer resolve failed: %s\n", err.c_str());
            rc = 1;
        } else {
            if (!client.edit_message_text(p, mid, argv[5], &err)) {
                std::fprintf(stderr, "edit failed: %s\n", err.c_str());
                rc = 1;
            } else {
                std::printf("edited\n");
                rc = 0;
            }
        }
    }
    if (cmd == "del" && argc >= 5) {
        int64_t chat = std::atoll(argv[3]);
        int64_t mid = std::atoll(argv[4]);
        Peer p;
        if (!resolve_peer(client, chat, &p, &err)) {
            std::fprintf(stderr, "peer resolve failed: %s\n", err.c_str());
            rc = 1;
        } else {
            if (!client.delete_messages(p, {mid}, true, &err)) {
                std::fprintf(stderr, "delete failed: %s\n", err.c_str());
                rc = 1;
            } else {
                std::printf("deleted\n");
                rc = 0;
            }
        }
    }
    if (cmd == "typing" && argc >= 4) {
        Peer p;
        int64_t chat = std::atoll(argv[3]);
        if (!resolve_peer(client, chat, &p, &err)) {
            std::fprintf(stderr, "peer resolve failed: %s\n", err.c_str());
            rc = 1;
        } else {
            std::printf(client.set_typing(p, &err) ? "typing sent\n" : "typing failed: %s\n",
                        err.c_str());
            rc = 0;
        }
    }
    if (cmd == "fwd" && argc >= 6) {
        // fwd <from_chat> <msg_id...> <to_chat>
        int64_t from_chat = std::atoll(argv[3]);
        int64_t to_chat = std::atoll(argv[argc - 1]);
        std::vector<long long> ids;
        for (int i = 4; i < argc - 1; i++) ids.push_back(std::atoll(argv[i]));
        Peer from, to;
        bool ok = resolve_peer(client, from_chat, &from, &err);
        if (!ok) { std::fprintf(stderr, "peer resolve failed: %s\n", err.c_str()); }
        else {
            std::string e2;
            ok = resolve_peer(client, to_chat, &to, &e2);
            if (!ok) { err = e2; std::fprintf(stderr, "peer resolve failed: %s\n", err.c_str()); }
        }
        if (!ok) {
            rc = 1;
        } else if (!client.forward_messages(from, ids, to, false, &err)) {
            std::fprintf(stderr, "forward failed: %s\n", err.c_str());
            rc = 1;
        } else {
            std::printf("forwarded %zu messages\n", ids.size());
            rc = 0;
        }
    }
    if (cmd == "dlmsg" && argc >= 6) {
        int64_t chat = std::atoll(argv[3]);
        long long mid = std::atoll(argv[4]);
        Peer p;
        if (!resolve_peer(client, chat, &p, &err)) {
            std::fprintf(stderr, "peer resolve failed: %s\n", err.c_str());
            rc = 1;
        } else {
            Val node = client.download_media_msg(p, mid, argv[5], &err);
            if (!err.empty()) {
                std::fprintf(stderr, "download failed: %s\n", err.c_str());
                rc = 1;
            } else {
                std::printf("saved %s (%s)\n", argv[5], node.name.c_str());
                rc = 0;
            }
        }
    }
    if (cmd == "react" && argc >= 6) {
        int64_t chat = std::atoll(argv[3]);
        long long mid = std::atoll(argv[4]);
        Peer p;
        if (!resolve_peer(client, chat, &p, &err)) {
            std::fprintf(stderr, "peer resolve failed: %s\n", err.c_str());
            rc = 1;
        } else {
            if (!client.set_reaction(p, mid, argv[5], &err)) {
                std::fprintf(stderr, "react failed: %s\n", err.c_str());
                rc = 1;
            } else {
                std::printf("reacted %s\n", argv[5]);
                rc = 0;
            }
        }
    }
    if (cmd == "pin" && argc >= 5) {
        int64_t chat = std::atoll(argv[3]);
        long long mid = std::atoll(argv[4]);
        bool unpin = argc >= 6 && std::string(argv[5]) == "off";
        Peer p;
        if (!resolve_peer(client, chat, &p, &err)) {
            std::fprintf(stderr, "peer resolve failed: %s\n", err.c_str());
            rc = 1;
        } else {
            if (!client.pin_message(p, mid, unpin, &err)) {
                std::fprintf(stderr, "pin failed: %s\n", err.c_str());
                rc = 1;
            } else {
                std::printf(unpin ? "unpinned\n" : "pinned\n");
                rc = 0;
            }
        }
    }
    if (cmd == "read" && argc >= 4) {
        int64_t chat = std::atoll(argv[3]);
        int64_t max_id = argc >= 5 ? std::atoll(argv[4]) : 0;
        Peer p;
        if (!resolve_peer(client, chat, &p, &err)) {
            std::fprintf(stderr, "peer resolve failed: %s\n", err.c_str());
            rc = 1;
        } else {
            if (!client.read_history(p, max_id, &err)) {
                std::fprintf(stderr, "read failed: %s\n", err.c_str());
                rc = 1;
            } else {
                std::printf("history read\n");
                rc = 0;
            }
        }
    }
    if (cmd == "search" && argc >= 4) {
        int limit = argc >= 5 ? std::atoi(argv[4]) : 10;
        std::string e2;
        Val r = client.search_messages(argv[3], limit, &e2);
        if (!e2.empty()) { std::fprintf(stderr, "search failed: %s\n", e2.c_str()); rc = 1; }
        else { std::printf("%s\n", cppgram::to_json(r).c_str()); rc = 0; }
    }
    if (cmd == "ban" && argc >= 5) {
        int64_t chat = std::atoll(argv[3]);
        long long uid = std::atoll(argv[4]);
        bool revoke = argc >= 6 && std::string(argv[5]) == "off";
        Peer p;
        if (!resolve_peer(client, chat, &p, &err)) {
            std::fprintf(stderr, "peer resolve failed: %s\n", err.c_str());
            rc = 1;
        } else {
            bool ok = revoke ? client.unban_chat_member(p, uid, &err)
                             : client.ban_chat_member(p, uid, false, &err);
            if (!ok) {
                std::fprintf(stderr, "ban failed: %s\n", err.c_str());
                rc = 1;
            } else {
                std::printf(revoke ? "unbanned\n" : "banned\n");
                rc = 0;
            }
        }
    }
    if (cmd == "promote" && argc >= 5) {
        int64_t chat = std::atoll(argv[3]);
        long long uid = std::atoll(argv[4]);
        bool admin = !(argc >= 6 && std::string(argv[5]) == "revoke");
        Peer p;
        if (!resolve_peer(client, chat, &p, &err)) {
            std::fprintf(stderr, "peer resolve failed: %s\n", err.c_str());
            rc = 1;
        } else {
            if (!client.promote_chat_member(p, uid, admin, &err)) {
                std::fprintf(stderr, "promote failed: %s\n", err.c_str());
                rc = 1;
            } else {
                std::printf(admin ? "promoted\n" : "demoted\n");
                rc = 0;
            }
        }
    }
    if (cmd == "logout") {
        client.auth_log_out(&err);
        std::printf("logged out (%s)\n", err.empty() ? "ok" : err.c_str());
        rc = 0;
    }
    if (cmd == "export-session") {
        bool pyro = argc >= 4 && std::string(argv[3]) == "pyrogram";
        std::string ss = client.session_string(pyro);
        std::printf("%s\n", ss.c_str());
        rc = 0;
    }
    if (cmd == "peers-dump") {
        std::string e2;
        if (!client.save_peer_cache("/dev/stdout", &e2)) {
            std::fprintf(stderr, "peers-dump failed: %s\n", e2.c_str());
            rc = 1;
        } else {
            rc = 0;
        }
    }
    if (cmd == "peers-save" && argc >= 4) {
        std::string e2;
        // Refresh from live dialogs first so the cache covers the account.
        client.refresh_dialog_peers(200, &e2);
        if (!client.save_peer_cache(argv[3], &e2)) {
            std::fprintf(stderr, "peers-save failed: %s\n", e2.c_str());
            rc = 1;
        } else {
            std::printf("saved %s\n", argv[3]);
            rc = 0;
        }
    }
    if (cmd == "peers-load" && argc >= 4) {
        std::string e2;
        if (!client.load_peer_cache(argv[3], &e2)) {
            std::fprintf(stderr, "peers-load failed: %s\n", e2.c_str());
            rc = 1;
        } else {
            std::printf("loaded %s\n", argv[3]);
            rc = 0;
        }
    }
    if (cmd == "upphoto" && argc >= 5) {
        int64_t chat = std::atoll(argv[3]);
        std::string data = read_file_bytes(argv[4]);
        if (data.empty()) { std::fprintf(stderr, "cannot open %s\n", argv[4]); rc = 1; }
        else {
            Peer p;
            if (!resolve_peer(client, chat, &p, &err)) {
                std::fprintf(stderr, "peer resolve failed: %s\n", err.c_str());
                rc = 1;
            } else {
                Val up = client.send_photo_message(p, data, argv[4], argc >= 6 ? argv[5] : "", &err);
                if (!err.empty()) { std::fprintf(stderr, "send media failed: %s\n", err.c_str()); rc = 1; }
                else {
                    std::printf("sent photo, updates ctor=%s\n", up.name.c_str());
                    rc = 0;
                }
            }
        }
    }
    if (cmd == "dlphoto" && argc >= 5) {
        int64_t uid = std::atoll(argv[3]);
        if (!client.download_user_photo(uid, argv[4], &err)) {
            std::fprintf(stderr, "download failed: %s\n", err.c_str());
            rc = 1;
        } else {
            std::printf("saved %s\n", argv[4]);
            rc = 0;
        }
    }
    if (cmd == "senddoc" && argc >= 5) {
        int64_t chat = std::atoll(argv[3]);
        std::string data = read_file_bytes(argv[4]);
        if (data.empty()) { std::fprintf(stderr, "cannot open %s\n", argv[4]); rc = 1; }
        else {
            Peer p;
            if (!resolve_peer(client, chat, &p, &err)) {
                std::fprintf(stderr, "peer resolve failed: %s\n", err.c_str());
                rc = 1;
            } else {
                Val up = client.send_document_message(p, data, argv[4], argc >= 6 ? argv[5] : "",
                                                      false, &err);
                if (!err.empty()) { std::fprintf(stderr, "send doc failed: %s\n", err.c_str()); rc = 1; }
                else {
                    std::printf("sent document (%zu bytes)\n", data.size());
                    rc = 0;
                }
            }
        }
    }
    if (cmd == "dldoc" && argc >= 6) {
        int64_t chat = std::atoll(argv[3]);
        Peer p;
        if (!resolve_peer(client, chat, &p, &err)) {
            std::fprintf(stderr, "peer resolve failed: %s\n", err.c_str());
            rc = 1;
        } else {
            long long mid = std::atoll(argv[4]);
            if (!client.download_document_msg(p, mid, argv[5], &err)) {
                std::fprintf(stderr, "download doc failed: %s\n", err.c_str());
                rc = 1;
            } else {
                std::printf("saved %s\n", argv[5]);
                rc = 0;
            }
        }
    }
    if (cmd == "sendvoice" && argc >= 6) {
        int64_t chat = std::atoll(argv[3]);
        std::string data = read_file_bytes(argv[4]);
        if (data.empty()) { std::fprintf(stderr, "cannot open %s\n", argv[4]); rc = 1; }
        else {
            Peer p;
            if (!resolve_peer(client, chat, &p, &err)) {
                std::fprintf(stderr, "peer resolve failed: %s\n", err.c_str());
                rc = 1;
            } else {
                Val up = client.send_voice_message(p, data, std::atof(argv[5]),
                                                    argc >= 7 ? argv[6] : "", &err);
                if (!err.empty()) { std::fprintf(stderr, "send voice failed: %s\n", err.c_str()); rc = 1; }
                else {
                    std::printf("sent voice (%zu bytes)\n", data.size());
                    rc = 0;
                }
            }
        }
    }
    if (cmd == "sendvideo" && argc >= 8) {
        int64_t chat = std::atoll(argv[3]);
        std::string data = read_file_bytes(argv[4]);
        if (data.empty()) { std::fprintf(stderr, "cannot open %s\n", argv[4]); rc = 1; }
        else {
            Peer p;
            if (!resolve_peer(client, chat, &p, &err)) {
                std::fprintf(stderr, "peer resolve failed: %s\n", err.c_str());
                rc = 1;
            } else {
                Val up = client.send_video_message(p, data, argv[4], argc >= 9 ? argv[8] : "",
                                                   std::atof(argv[7]), std::atoi(argv[5]),
                                                   std::atoi(argv[6]), &err);
                if (!err.empty()) { std::fprintf(stderr, "send video failed: %s\n", err.c_str()); rc = 1; }
                else {
                    std::printf("sent video (%zu bytes)\n", data.size());
                    rc = 0;
                }
            }
        }
    }
    if (cmd == "watch" && argc >= 4) {
        int secs = std::atoi(argv[3]);
        MessageFilter f;
        if (argc >= 5) f.chat_id = std::atoll(argv[4]);
        client.on_message(f, [](const Message& m) {
            std::printf("[msg] chat=%lld id=%lld from=%lld text=%.80s\n", (long long)m.chat_id,
                        (long long)m.id, (long long)m.from_id, m.text.c_str());
            std::fflush(stdout);
        });
        int n = client.wait_updates(secs);
        std::printf("watched %d seconds, %d messages\n", secs, n);
        rc = 0;
    }
    auto need_peer = [&](long long id, Peer* pe) -> bool {
        std::string e2;
        if (!resolve_peer(client, id, pe, &e2)) {
            std::fprintf(stderr, "peer: %s\n", e2.c_str());
            return false;
        }
        return true;
    };
    auto print_val = [&](const char* tag, const Val& r, const std::string& e2) {
        if (!e2.empty()) { std::fprintf(stderr, "%s: %s\n", tag, e2.c_str()); rc = 1; }
        else { std::printf("%s\n", cppgram::to_json(r).c_str()); rc = 0; }
    };
    if (cmd == "contacts") {
        std::string e2; print_val("contacts", client.get_contacts(&e2), e2);
    }
    if (cmd == "block" && argc >= 4) {
        std::string e2; print_val("block", client.block_user(std::atoll(argv[3]), &e2), e2);
    }
    if (cmd == "unblock" && argc >= 4) {
        std::string e2; print_val("unblock", client.unblock_user(std::atoll(argv[3]), &e2), e2);
    }
    if (cmd == "profile" && argc >= 6) {
        std::string e2; print_val("profile", client.update_profile(argv[3], argv[4], argv[5], &e2), e2);
    }
    if (cmd == "setusername" && argc >= 4) {
        std::string e2; print_val("setusername", client.set_username(argv[3], &e2), e2);
    }
    if (cmd == "invite" && argc >= 4) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("invite", client.export_chat_invite_link(pe, &e2), e2);
        } else rc = 1;
    }
    if (cmd == "settitle" && argc >= 5) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("settitle", client.set_chat_title(pe, argv[4], &e2), e2);
        } else rc = 1;
    }
    if (cmd == "setabout" && argc >= 5) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("setabout", client.set_chat_description(pe, argv[4], &e2), e2);
        } else rc = 1;
    }
    if (cmd == "poll" && argc >= 6) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::vector<std::string> opts;
            for (int i = 5; i < argc; i++) opts.emplace_back(argv[i]);
            std::string e2; print_val("poll", client.send_poll(pe, argv[4], opts, false, -1, &e2), e2);
        } else rc = 1;
    }
    if (cmd == "dice" && argc >= 4) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string emoji = argc >= 5 ? argv[4] : "🎲";
            std::string e2; print_val("dice", client.send_dice(pe, emoji, &e2), e2);
        } else rc = 1;
    }
    if (cmd == "location" && argc >= 6) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("location", client.send_location(pe, std::atof(argv[4]), std::atof(argv[5]), &e2), e2);
        } else rc = 1;
    }
    if (cmd == "contact" && argc >= 7) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("contact", client.send_contact(pe, argv[4], argv[5], argv[6], &e2), e2);
        } else rc = 1;
    }
    if (cmd == "folders") {
        std::string e2; print_val("folders", client.get_folders(&e2), e2);
    }
    if (cmd == "stories" && argc >= 4) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("stories", client.get_chat_stories(pe, &e2), e2);
        } else rc = 1;
    }
    if (cmd == "allstories") {
        std::string e2; print_val("allstories", client.get_all_stories(&e2), e2);
    }
    if (cmd == "stickerset" && argc >= 4) {
        std::string e2; print_val("stickerset", client.get_sticker_set(argv[3], &e2), e2);
    }
    if (cmd == "favstickers") {
        std::string e2; print_val("favstickers", client.get_favorite_stickers(&e2), e2);
    }
    if (cmd == "sessions") {
        std::string e2; print_val("sessions", client.get_active_sessions(&e2), e2);
    }
    if (cmd == "privacy" && argc >= 4) {
        std::string e2; print_val("privacy", client.get_privacy(argv[3], &e2), e2);
    }
    if (cmd == "ttl") {
        std::string e2; print_val("ttl", client.get_account_ttl(&e2), e2);
    }
    if (cmd == "stars") {
        std::string e2; print_val("stars", client.get_stars_balance(&e2), e2);
    }
    if (cmd == "gifts") {
        std::string e2; print_val("gifts", client.get_available_gifts(&e2), e2);
    }
    if (cmd == "boosts" && argc >= 4) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("boosts", client.get_boosts_status(pe, &e2), e2);
        } else rc = 1;
    }
    if (cmd == "archive" && argc >= 4) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("archive", client.archive_chats({pe}, true, &e2), e2);
        } else rc = 1;
    }
    if (cmd == "unarchive" && argc >= 4) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("unarchive", client.archive_chats({pe}, false, &e2), e2);
        } else rc = 1;
    }
    if (cmd == "topics" && argc >= 4) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("topics", client.get_forum_topics(pe, 20, &e2), e2);
        } else rc = 1;
    }
    if (cmd == "createtopic" && argc >= 5) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("createtopic", client.create_forum_topic(pe, argv[4], &e2), e2);
        } else rc = 1;
    }
    if (cmd == "joinreq" && argc >= 4) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("joinreq", client.get_chat_join_requests(pe, 20, &e2), e2);
        } else rc = 1;
    }
    if (cmd == "appconfig") {
        std::string e2; print_val("appconfig", client.get_app_config(&e2), e2);
    }
    if (cmd == "ton") {
        std::string e2; print_val("ton", client.get_ton_balance(&e2), e2);
    }
    if (cmd == "sendrich" && argc >= 5) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("sendrich", client.send_rich_message(pe, argv[4], 0, &e2), e2);
        } else rc = 1;
    }

    /* ---------- parity: stories ---------- */
    if (cmd == "getstories" && argc >= 4) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("getstories", client.get_stories(pe, argc >= 5 ? std::atoll(argv[4]) : 0, &e2), e2);
        } else rc = 1;
    }
    if (cmd == "archivedstories" && argc >= 5) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("archivedstories", client.get_archived_stories(pe, std::atoi(argv[4]), argc >= 6 ? std::atoll(argv[5]) : 0, &e2), e2);
        } else rc = 1;
    }
    if (cmd == "pinnedstories" && argc >= 5) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("pinnedstories", client.get_pinned_stories(pe, std::atoi(argv[4]), argc >= 6 ? std::atoll(argv[5]) : 0, &e2), e2);
        } else rc = 1;
    }
    if (cmd == "viewstories" && argc >= 4) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::vector<int64_t> ids;
            for (int i = 4; i < argc; i++) ids.push_back(std::atoll(argv[i]));
            std::string e2; print_val("viewstories", client.view_stories(pe, ids, &e2), e2);
        } else rc = 1;
    }
    if (cmd == "storyviews" && argc >= 5) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("storyviews", client.get_story_views(pe, argv[4], 0, &e2), e2);
        } else rc = 1;
    }
    if (cmd == "editstorycaption" && argc >= 6) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("editstorycaption", client.edit_story_caption(pe, std::atoll(argv[4]), argv[5], "[]", &e2), e2);
        } else rc = 1;
    }
    if (cmd == "deleteStory" && argc >= 5) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("deleteStory", client.delete_story(pe, std::atoll(argv[4]), &e2), e2);
        } else rc = 1;
    }
    if (cmd == "pinstory" && argc >= 5) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::vector<int64_t> ids; { std::string a = argv[4]; size_t p2 = 0;
                for (long long v : split_ids(a, &p2)) ids.push_back(v); }
            std::string e2; print_val("pinstory", client.pin_chat_stories(pe, ids, true, &e2), e2);
        } else rc = 1;
    }
    if (cmd == "hidestories" && argc >= 5) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("hidestories", client.hide_chat_stories(pe, std::atoi(argv[4]) != 0, &e2), e2);
        } else rc = 1;
    }
    if (cmd == "stealthmode") {
        std::string e2; print_val("stealthmode", client.enable_stealth_mode(&e2), e2);
    }
    if (cmd == "fwdstory" && argc >= 7) {
        Peer to; if (need_peer(std::atoll(argv[3]), &to)) {
            Peer from; if (need_peer(std::atoll(argv[5]), &from)) {
                std::string e2; print_val("fwdstory", client.forward_story(to, std::atoll(argv[4]), from, &e2), e2);
            } else rc = 1;
        } else rc = 1;
    }
    if (cmd == "canstory" && argc >= 4) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("canstory", client.can_post_stories(pe, &e2), e2);
        } else rc = 1;
    }
    /* ---------- parity: stickers ---------- */
    if (cmd == "ownedstickersets") {
        std::string e2; print_val("ownedstickersets", client.get_owned_sticker_sets(&e2), e2);
    }
    if (cmd == "recentstickers") {
        std::string e2; print_val("recentstickers", client.get_recent_stickers(&e2), e2);
    }
    if (cmd == "clearrecentstickers") {
        std::string e2; print_val("clearrecentstickers", client.clear_recent_stickers(&e2), e2);
    }
    if (cmd == "favsticker" && argc >= 5) {
        std::string e2;
        cppgram::Val r = std::atoi(argv[4]) ? client.add_favorite_sticker(argv[3], &e2)
                                            : client.remove_favorite_sticker(argv[3], &e2);
        print_val("favsticker", r, e2);
    }
    if (cmd == "recsticker" && argc >= 5) {
        std::string e2;
        cppgram::Val r = std::atoi(argv[4]) ? client.add_recent_sticker(argv[3], &e2)
                                            : client.remove_recent_sticker(argv[3], &e2);
        print_val("recsticker", r, e2);
    }
    if (cmd == "searchstickersets" && argc >= 4) {
        std::string e2; print_val("searchstickersets", client.search_sticker_sets(argv[3], false, &e2), e2);
    }
    if (cmd == "getcustomemoji" && argc >= 4) {
        std::vector<long long> ids; { size_t p2 = 0;
            for (long long v : split_ids(argv[3], &p2)) ids.push_back(v); }
        std::string e2; print_val("getcustomemoji", client.get_custom_emoji_stickers(ids, &e2), e2);
    }
    if (cmd == "deletestickerset" && argc >= 4) {
        std::string e2; print_val("deletestickerset", client.delete_sticker_set(argv[3], &e2), e2);
    }
    if (cmd == "installstickerset" && argc >= 5) {
        Peer owner; std::string e2;
        if (resolve_peer(client, std::atoll(argv[3]), &owner, &e2)) {
            print_val("installstickerset", client.change_sticker_set(owner, argv[4], true, false, &e2), e2);
        } else rc = 1;
    }
    if (cmd == "setstickertitle" && argc >= 5) {
        std::string e2; print_val("setstickertitle", client.set_sticker_set_title(argv[3], argv[4], &e2), e2);
    }
    if (cmd == "setstickerpos" && argc >= 6) {
        std::string e2; print_val("setstickerpos", client.set_sticker_position_in_set(argv[3], argv[4], std::atoi(argv[5]), &e2), e2);
    }
    /* ---------- parity: gifts ---------- */
    if (cmd == "chatgifts" && argc >= 6) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("chatgifts", client.get_chat_gifts(pe, std::atoi(argv[5]), std::atoi(argv[4]), false, false, false, false, false, false, &e2), e2);
        } else rc = 1;
    }
    if (cmd == "chatgiftscount" && argc >= 4) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("chatgiftscount", client.get_chat_gifts_count(pe, &e2), e2);
        } else rc = 1;
    }
    if (cmd == "giftcollections" && argc >= 4) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("giftcollections", client.get_gift_collections(pe, &e2), e2);
        } else rc = 1;
    }
    if (cmd == "searchresalegifts" && argc >= 4) {
        std::string e2; print_val("searchresalegifts",
            client.search_gifts_for_resale(std::atoll(argv[3]),
                argc >= 5 ? std::atoi(argv[4]) : cppgram::enums::GiftForResaleOrder::CHANGE_DATE,
                argc >= 6 && std::atoi(argv[5]) != 0, argc >= 7 && std::atoi(argv[6]) != 0,
                argc >= 8 ? argv[7] : "", argc >= 9 ? std::atoi(argv[8]) : 20,
                argc >= 10 ? argv[9] : "", &e2),
            e2);
    }
    if (cmd == "giftsforcrafting" && argc >= 4) {
        std::string e2; print_val("giftsforcrafting",
            client.get_gifts_for_crafting(std::atoll(argv[3]), argc >= 5 ? std::atoi(argv[4]) : 20, &e2), e2);
    }
    if (cmd == "togglepingift" && argc >= 6) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::vector<long long> ids; { size_t p2 = 0;
                for (long long v : split_ids(argv[5], &p2)) ids.push_back(v); }
            std::string json = "{\"_ctor\":\"inputInvoiceStarGift\",\"p2p\":false,\"gift\":{\"_ctor\":\"starGift\",\"id\":"
                             + std::to_string(ids.empty() ? 0 : ids[0]) + "}}";
            std::string e2; print_val("togglepingift", client.toggle_gift_pinned(pe, json, std::atoi(argv[4]) != 0, &e2), e2);
        } else rc = 1;
    }
    /* ---------- parity: payments ---------- */
    if (cmd == "paymentform" && argc >= 4) {
        std::string e2; print_val("paymentform", client.get_payment_form(argv[3], &e2), e2);
    }
    if (cmd == "sendform" && argc >= 5) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("sendform", client.send_payment_form(pe, std::atoll(argv[4]), argv[3], argv[3], &e2), e2);
        } else rc = 1;
    }
    if (cmd == "editsub" && argc >= 5) {
        std::string e2; print_val("editsub", client.edit_star_subscription(argv[3], std::atoi(argv[4]) != 0, &e2), e2);
    }
    if (cmd == "giftpremium" && argc >= 5) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2;
            print_val("giftpremium", client.gift_premium_with_stars(pe, std::atoi(argv[4]),
                        argc >= 6 ? std::atoll(argv[5]) : 0, "{\"_ctor\":\"inputInvoicePremiumGiftStars\"}", &e2), e2);
        } else rc = 1;
    }
    if (cmd == "invoicelink" && argc >= 5) {
        std::string e2;
        print_val("invoicelink", client.create_invoice_link(argv[3], argv[4], "{\"_ctor\":\"dataJSON\",\"data\":\"{}\"}",
                    "", "XTR", argc >= 6 ? std::atoll(argv[5]) : 0, &e2), e2);
    }
    /* ---------- parity: bots ---------- */
    if (cmd == "botname" && argc >= 6) {
        std::string e2;
        cppgram::Val r = std::atoi(argv[4]) ? client.set_bot_name(argv[3], "", argv[5], &e2)
                                            : client.get_bot_name(argv[3], "", &e2);
        print_val("botname", r, e2);
    }
    if (cmd == "botinfo" && argc >= 5) {
        std::string e2;
        int which = std::atoi(argv[4]);
        cppgram::Val r = which == 0 ? client.get_bot_info_description(argv[3], "", &e2)
                     : which == 1 ? client.set_bot_info_description(argv[3], "", argc >= 6 ? argv[5] : "", &e2)
                     : which == 2 ? client.get_bot_info_short_description(argv[3], "", &e2)
                                  : client.set_bot_info_short_description(argv[3], "", argc >= 6 ? argv[5] : "", &e2);
        print_val("botinfo", r, e2);
    }
    if (cmd == "botcommands" && argc >= 4) {
        std::string e2; print_val("botcommands", client.get_bot_commands(argv[3], "", "{\"_ctor\":\"botCommandScopeDefault\"}", 0, &e2), e2);
    }
    if (cmd == "ownedbots") {
        std::string e2; print_val("ownedbots", client.get_owned_bots(&e2), e2);
    }
    if (cmd == "botdefaults") {
        std::string e2; print_val("botdefaults", client.get_bot_default_privileges(&e2), e2);
    }
    if (cmd == "menubutton" && argc >= 4) {
        std::string e2;
        if (std::atoi(argv[3])) print_val("menubutton", client.set_chat_menu_button("{\"_ctor\":\"botMenuButtonCommands\"}", &e2), e2);
        else print_val("menubutton", client.get_chat_menu_button(argc >= 5 ? std::atoll(argv[4]) : 0, &e2), e2);
    }
    if (cmd == "managedbot" && argc >= 5) {
        std::string e2;
        int what = std::atoi(argv[3]);
        if (what == 0) print_val("managedbot", client.get_managed_bot_token(std::atoll(argv[4]), &e2), e2);
        else if (what == 1) print_val("managedbot", client.set_managed_bot_access_settings(std::atoll(argv[4]), true, false, &e2), e2);
        else print_val("managedbot", client.get_managed_bot_access_settings(std::atoll(argv[4]), &e2), e2);
    }
    if (cmd == "answebapp" && argc >= 5) {
        std::string e2; print_val("answebapp", client.answer_web_app_query(argv[3], argv[4], &e2), e2);
    }
    /* ---------- parity: chats / folders ---------- */
    if (cmd == "mksupergroup" && argc >= 4) {
        std::string e2; print_val("mksupergroup", client.create_supergroup(argv[3], argc >= 5 ? argv[4] : "", argc >= 6 && std::atoi(argv[5]), &e2), e2);
    }
    if (cmd == "chatphoto" && argc >= 5) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string data = read_file(argv[4]);
            if (data.empty()) { std::fprintf(stderr, "cannot open %s\n", argv[4]); rc = 1; }
            else {
                std::string e2; print_val("chatphoto", client.set_chat_photo(pe, data, &e2), e2);
            }
        } else rc = 1;
    }
    if (cmd == "chatperms" && argc >= 4) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("chatperms", client.set_chat_permissions(pe, argc >= 5 ? argv[4] : "{}", &e2), e2);
        } else rc = 1;
    }
    if (cmd == "membertag" && argc >= 5) {
        std::string e2; print_val("membertag", client.set_chat_member_tag(std::atoll(argv[3]), std::atoll(argv[4]), argc >= 6 ? argv[5] : "", &e2), e2);
    }
    if (cmd == "onlinenum" && argc >= 4) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("onlinenum", client.get_chat_online_count(pe, &e2), e2);
        } else rc = 1;
    }
    if (cmd == "similarchats" && argc >= 4) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("similarchats", client.get_similar_channels(pe, &e2), e2);
        } else rc = 1;
    }
    if (cmd == "topchats" && argc >= 4) {
        std::string e2; print_val("topchats", client.get_top_chats(std::atoi(argv[3]), &e2), e2);
    }
    if (cmd == "chatsforfolder" && argc >= 4) {
        std::string e2; print_val("chatsforfolder", client.get_chats_for_folder_invite_link(std::atoll(argv[3]), &e2), e2);
    }
    if (cmd == "reorderfolders" && argc >= 4) {
        std::vector<int> order; { size_t p2 = 0;
            for (long long v : split_ids(argv[3], &p2)) order.push_back((int)v); }
        std::string e2; print_val("reorderfolders", client.reorder_folders(order, &e2), e2);
    }
    if (cmd == "foldertags" && argc >= 4) {
        std::string e2; print_val("foldertags", client.toggle_folder_tags(std::atoi(argv[3]) != 0, &e2), e2);
    }
    if (cmd == "approvedall" && argc >= 4) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("approvedall", client.approve_all_chat_join_requests(pe, "{}", &e2), e2);
        } else rc = 1;
    }
    if (cmd == "declinedall" && argc >= 4) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("declinedall", client.decline_all_chat_join_requests(pe, "{}", &e2), e2);
        } else rc = 1;
    }
    if (cmd == "linkjoiners" && argc >= 5) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("linkjoiners", client.get_chat_invite_link_joiners(pe, argv[4], argc >= 6 ? std::atoi(argv[5]) : 0, 0, &e2), e2);
        } else rc = 1;
    }
    if (cmd == "historycount" && argc >= 4) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("historycount", client.get_chat_history_count(pe, &e2), e2);
        } else rc = 1;
    }
    if (cmd == "sendchecklist" && argc >= 5) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("sendchecklist", client.send_checklist(pe, argv[4], argc >= 6 ? argv[5] : "", 0, &e2), e2);
        } else rc = 1;
    }
    if (cmd == "sendpaidmedia" && argc >= 6) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("sendpaidmedia", client.send_paid_media(pe, argv[4], std::atoll(argv[5]), &e2), e2);
        } else rc = 1;
    }
    if (cmd == "streammedia" && argc >= 6) {
        Peer pe; if (need_peer(std::atoll(argv[3]), &pe)) {
            std::string e2; print_val("streammedia", client.stream_media(pe, std::atoll(argv[4]), argv[5], &e2), e2);
        } else rc = 1;
    }
    if (cmd == "fwdmediagroup" && argc >= 6) {
        Peer from; if (need_peer(std::atoll(argv[3]), &from)) {
            Peer to; if (need_peer(std::atoll(argv[5]), &to)) {
                std::vector<int64_t> ids; { size_t p2 = 0;
                    for (long long v : split_ids(argv[4], &p2)) ids.push_back(v); }
                std::string e2; print_val("fwdmediagroup", client.forward_media_group(from, ids, to, &e2), e2);
            } else rc = 1;
        } else rc = 1;
    }
    if (cmd == "searchposts" && argc >= 4) {
        std::string e2; print_val("searchposts", client.search_posts(argv[3], argc >= 5 ? std::atoi(argv[4]) : 20, false, &e2), e2);
    }
    if (cmd == "effects") {
        std::string e2; print_val("effects", client.get_available_effects(&e2), e2);
    }
    if (cmd == "invoke" && argc >= 5) {
        std::string e2; print_val("invoke", client.invoke(argv[3], argv[4], &e2), e2);
    }
    if (cmd == "recovergaps" && argc >= 5) {
        std::string e2; print_val("recovergaps", client.recover_gaps(std::atoll(argv[3]), std::atoi(argv[4]), argc >= 6 ? std::atoi(argv[5]) : 100, &e2), e2);
    }

    if (cmd == "diff") {
        client.set_update_handler([](const UpdateEvent& e) {
            std::printf("update %s %s\n", e.kind.c_str(), e.payload.c_str());
            std::fflush(stdout);
        });
        client.poll_updates();
        client.poll_difference();  // prime state
        std::printf("primed pts, polling 25s...\n");
        std::fflush(stdout);
        for (int i = 0; i < 25; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            client.poll_difference();
            client.poll_updates();  // drain pushed backlog + ack
        }
        std::printf("done\n");
        rc = 0;
    }
    if (rc == 2) {
        std::fprintf(stderr, "unknown command: %s\n", cmd.c_str());
    }
    // Persist the peer cache next to the session file when it changed.
    if (!peer_cache.empty()) {
        std::string e2;
        client.save_peer_cache(peer_cache, &e2);
    }
    return rc;
}
