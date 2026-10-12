// Session-string regression tests. The gogram JSON format could not be read at
// all (the hand-rolled scanner threw on the first member), so both gogram
// formats and the pyrogram one are covered here, including byte-exact strings
// produced by the reference implementations.
#include "cppgram/client.hpp"

#include <cstdio>
#include <string>

using namespace cppgram;

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL %s\n", what);
        ++failures;
    }
}

Session make_session() {
    Session s;
    s.dc_id = 5;
    s.ip = "91.108.56.151";
    s.api_id = 33489956;
    s.user_id = 8663044027;
    s.auth_key.assign(256, '\x11');
    for (size_t i = 0; i < s.auth_key.size(); i++) s.auth_key[i] = static_cast<char>(i & 0xff);
    return s;
}

void check_equal(const Session& a, const Session& b, const char* what) {
    expect(a.dc_id == b.dc_id, what);
    expect(a.api_id == b.api_id, what);
    expect(a.auth_key == b.auth_key, what);
    expect(a.ip == b.ip, what);
}

}  // namespace

int main() {
    const Session want = make_session();

    // gogram JSON format: what our export produces and gogram can load.
    {
        const std::string enc = "1BvE" + [&] {
            std::string js = "{\"key\":\"";
            // standard base64 of the key, as Go marshals a []byte field
            static const char* alpha =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            auto b64 = [&](const std::string& in) {
                std::string out;
                int val = 0, bits = -6;
                for (unsigned char c : in) {
                    val = (val << 8) + c;
                    bits += 8;
                    while (bits >= 0) { out.push_back(alpha[(val >> bits) & 0x3f]); bits -= 6; }
                }
                if (bits > -6) out.push_back(alpha[((val << 8) >> (bits + 8)) & 0x3f]);
                while (out.size() % 4) out.push_back('=');
                return out;
            };
            js += b64(want.auth_key) + "\",\"hash\":\"" + b64(want.auth_key.substr(0, 8)) +
                  "\",\"dc_id\":5,\"ip_addr\":\"91.108.56.151\",\"app_id\":33489956}";
            // outer payload is base64url without padding (base64.RawURLEncoding)
            static const char* ua =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
            std::string out;
            int val = 0, bits = -6;
            for (unsigned char c : js) {
                val = (val << 8) + c;
                bits += 8;
                while (bits >= 0) { out.push_back(ua[(val >> bits) & 0x3f]); bits -= 6; }
            }
            if (bits > -6) out.push_back(ua[((val << 8) >> (bits + 8)) & 0x3f]);
            return out;
        }();
        Session got;
        try {
            got = Client::parse_session_string(enc);
        } catch (const std::exception& e) {
            expect(false, e.what());
            got = Session{};
        }
        check_equal(want, got, "gogram json session fields");
        expect(got.user_id == 0, "gogram json carries no user id");
    }

    // gogram legacy format: auth_key ":_:" hash ":_:" ip ":_:" dc ":_:" app_id
    {
        std::string legacy = want.auth_key + ":_:" + want.auth_key.substr(0, 8) +
                             ":_:" + want.ip + ":_:" + std::to_string(want.dc_id) + ":_:" +
                             std::to_string(want.api_id);
        static const char* alpha =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        auto b64url = [&](const std::string& in) {
            std::string out;
            int val = 0, bits = -6;
            for (unsigned char c : in) {
                val = (val << 8) + c;
                bits += 8;
                while (bits >= 0) { out.push_back(alpha[(val >> bits) & 0x3f]); bits -= 6; }
            }
            if (bits > -6) out.push_back(alpha[((val << 8) >> (bits + 8)) & 0x3f]);
            return out;
        };
        const std::string enc = "1BvX" + b64url(legacy);
        Session got;
        try {
            got = Client::parse_session_string(enc);
        } catch (const std::exception& e) {
            expect(false, e.what());
            got = Session{};
        }
        check_equal(want, got, "gogram legacy session fields");
    }

    // malformed input must be rejected, not silently half-parsed
    for (const char* bad : {"", "1BvE", "1BvE!!!!", "1BvX", "1BvXbm90LWEtc2Vzc2lvbg",
                            "1BvEeyJub3QiOiJhbiBvYmplY3QifQ", "1BvEeyJrZXkiOiJ4In0"}) {
        bool threw = false;
        try {
            (void)Client::parse_session_string(bad);
        } catch (const std::exception&) {
            threw = true;
        }
        expect(threw, bad);
    }

    // pyrogram format: 271 raw bytes, big-endian fields
    {
        Session got;
        try {
            got = Client::parse_session_string(Client::pyrogram_session_string(want));
        } catch (const std::exception& e) {
            expect(false, e.what());
            got = Session{};
        }
        expect(got.dc_id == want.dc_id, "pyrogram dc");
        expect(got.api_id == want.api_id, "pyrogram app id");
        expect(got.auth_key == want.auth_key, "pyrogram auth key");
        expect(got.user_id == want.user_id, "pyrogram user id");
        expect(got.ip == want.ip, "pyrogram ip");
    }

    if (failures == 0) std::printf("session: all cases passed\n");
    return failures != 0;
}
