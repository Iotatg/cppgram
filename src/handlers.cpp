// Pyrogram-style message dispatch layer over the update pipeline.
#include "cppgram/client.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <thread>

namespace cppgram {

namespace {
Message message_from_payload(const std::string& payload, int64_t self_id) {
    Message m;
    std::string err;
    Val v = parse_json(payload, &err);
    if (!err.empty() || v.k != Val::OBJ) return m;
    m.raw = v;
    m.id = v.num("id");
    m.chat_id = v.num("chat");
    if (!m.chat_id) {
        const Val& pid = v.at("peer_id");
        if (pid.name == "peerChannel") m.chat_id = -(100000000000LL + pid.num("channel_id"));
        else if (pid.name == "peerUser") m.chat_id = pid.num("user_id");
        else if (pid.name == "peerChat") m.chat_id = -pid.num("chat_id");
    }
    const Val& fid = v.at("from_id");
    m.from_id = fid.name == "peerUser" ? fid.num("user_id") : 0;
    m.text = v.text("message");
    m.date = v.num("date");
    m.incoming = m.from_id != self_id;
    if (v.o.empty()) m.id = 0;
    return m;
}
}  // namespace

void Client::dispatch_message_event(const UpdateEvent& ev) {
    if (ev.kind != "message") return;
    std::vector<std::tuple<int, MessageFilter, MessageHandler>> snapshot;
    {
        std::lock_guard<std::mutex> g(msg_mu_);
        snapshot = msg_handlers_;
    }
    if (snapshot.empty()) return;
    Message m = message_from_payload(ev.payload, session_.user_id ? session_.user_id : ev.chat_id);
    if (ev.chat_id) m.chat_id = ev.chat_id;
    if (m.id <= 0) return;
    if (getenv("CG_TRACE"))
        std::fprintf(stderr, "[disp] id=%lld chat=%lld from=%lld text=%.40s handlers=%zu\n",
                     (long long)m.id, (long long)m.chat_id, (long long)m.from_id,
                     m.text.c_str(), snapshot.size());
    for (auto& [h, f, fn] : snapshot)
        if (f.matches(m)) fn(m);
}

int Client::on_message(MessageFilter f, MessageHandler fn) {
    std::lock_guard<std::mutex> g(msg_mu_);
    int h = ++msg_handle_seq_;
    msg_handlers_.emplace_back(h, std::move(f), std::move(fn));
    return h;
}

void Client::off_message(int handle) {
    std::lock_guard<std::mutex> g(msg_mu_);
    msg_handlers_.erase(std::remove_if(msg_handlers_.begin(), msg_handlers_.end(),
                                       [&](auto& t) { return std::get<0>(t) == handle; }),
                        msg_handlers_.end());
}

int Client::wait_updates(int seconds, bool with_difference) {
    struct Tally { int n; } tally{0};
    auto prev = update_handler_;
    auto ours = [&](const UpdateEvent& ev) {
        if (ev.kind == "message") tally.n++;
        if (prev) prev(ev);
    };
    set_update_handler(ours);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (std::chrono::steady_clock::now() < deadline) {
        poll_updates();
        if (with_difference) poll_difference();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    set_update_handler(prev);
    return tally.n;
}

}  // namespace cppgram
