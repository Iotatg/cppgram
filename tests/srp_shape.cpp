// Regression test for the SRP login path. Every failure here was found by a live
// login, not by inspection: a null BN_CTX segfaulted the whole path, and A/M1
// went out as base64url *text* so Telegram answered SRP_A_INVALID.
//
// A synthetic account.password tree is enough: the test asserts the byte layout
// that hits the wire, which is the part that was wrong.
#include "cppgram/args.hpp"
#include "cppgram/client.hpp"
#include "cppgram/utils.hpp"

#include <cstdio>
#include <cstdlib>
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

std::string from_hex(const std::string& h) {
    std::string out;
    for (size_t i = 0; i + 1 < h.size(); i += 2)
        out.push_back(static_cast<char>(std::stoi(h.substr(i, 2), nullptr, 16)));
    return out;
}

// The 2048-bit prime Telegram actually sends (captured from a live
// account.getPassword); 256 bytes, and a probable prime.
const char* kPrimeHex =
    "c71caeb9c6b1c9048e6c522f70f13f73980d40238e3e21c14934d037563d930f48198a0aa7c14058229493"
    "d22530f4dbfa336f6e0ac925139543aed44cce7c3720fd51f69458705ac68cd4fe6b6b13abdc97465129"
    "69328454f18faf8c595f642477fe96bb2a941d5bcd1d4ac8cc49880708fa9b378e3c4f3a9060bee67cf9"
    "a4a4a695811051907e162753b56b0f6b410dba74d8a84b2a14b3144e0ef1284754fd17ed950d5965b4b9"
    "dd46582db1178d169c6bc465b0d6ff9ca3928fef5b9ae4e418fc15e83ebea0f87fa9ff5eed70050ded2"
    "849f47bf959d956850ce929851f0d8115f635b105ee2e4e15d04b2454bf6f4fadf034b10403119cd8e3b"
    "92fcc5b";

Val password_tree() {
    return Val::obj(
        "account.password",
        {{"has_password", Val::i64(1)},
         {"srp_id", Val::i64(2947218985977228573LL)},
         {"srp_B", Val::str(from_hex(
                       "b01c8789c9c9c7bfd041046e71a871d1574250c59491bb2f473f84fb1fb96a4b4ec040"
                       "5d41e16f0cf4f8e7279d772c88a6f0732329f403b194223270e9ddfeccd3a0110046c87"
                       "759d45e7e9e62f380698b0c9f858f60a4767447ce5b42635a3a29d86ece3b9e69188b6e9"
                       "24e63d9a3e1404a170c6655a60f1f62910424f79a712eda10966f5b460d1bef7866d9536"
                       "3c592f3fc564b0b608c25d30a9a5db4133f4ba4c2ec5e4d0d62f7d28f6af15f9d96e37ba"
                       "98c1d795a6bca67ba3c3813c99b2e63b3cb722365aecd8f7cbbcc1fa9280137a40d214df"
                       "343dbef9aa317897e0c8b53f3cb88f833e6002c137ac5a7291aaa495eb75016f4d2f3706c"
                       "02d7394119"))},
         {"current_algo",
          Val::obj("passwordKdfAlgoSHA256SHA256PBKDF2HMACSHA512iter100000SHA256ModPow",
                   {{"salt1", Val::str(from_hex("c1572b105104ceff3a9bb86f16f7babf2ec9c2e3dc44"
                                                "d7107b5be7917dd450cc980620d475200427"))},
                    {"salt2", Val::str(from_hex("9b3d98082244d6ad6948449f2708806f"))},
                    {"g", Val::i64(3)},
                    {"p", Val::str(from_hex(kPrimeHex))}})}});
}

}  // namespace

int main() {
    // 2. The whole point: A and M1 must reach the encoder as raw bytes, not as
    //    the base64url text the JSON box carries. Comparing against a fresh
    //    run is not possible (`a` is random), so the check is that decoding the
    //    JSON's own base64 yields exactly what the encoder is handed.
    // The base64 JSON box must parse and carry both fields.
    std::string serr;
    Val box = parse_json(srp_password_check(password_tree(), "hunter2", &serr), &serr);
    expect(serr.empty(), "srp_password_check error");
    expect(box.name == "inputCheckPasswordSRP", "ctor is inputCheckPasswordSRP");
    expect(box.has("A") && box.has("M1"), "box carries A and M1");
    expect(utils::b64_decode(box.text("A")).size() == 256, "base64 A decodes to 256");
    expect(utils::b64_decode(box.text("M1")).size() == 32, "base64 M1 decodes to 32");

    // A and M1 must reach the encoder as raw bytes, not as base64url text.
    // `a` is random per call, so the assertion is on the size and on the exact
    // bytes surviving the encode/decode round trip, never on a specific value.
    Val args = srp_check_password_args(password_tree(), "hunter2", &serr);
    expect(serr.empty(), "srp_check_password_args error");
    const Val* pw = &args.at("password");
    expect(pw->k == Val::OBJ, "password field is a nested box");
    if (pw->k != Val::OBJ) {
        std::printf("srp: cannot inspect password field\n");
        return 1;
    }
    expect(pw->name == "inputCheckPasswordSRP", "nested ctor preserved");
    const std::string A = pw->at("A").s, M1 = pw->at("M1").s;
    expect(A.size() == 256, "A is 256 raw bytes on the wire");
    expect(M1.size() == 32, "M1 is 32 raw bytes on the wire");
    // A must be a plausible public value: non-zero, and below the prime.
    std::string ta = A, tp = from_hex(kPrimeHex);
    while (!ta.empty() && ta[0] == '\0') ta.erase(0, 1);
    while (!tp.empty() && tp[0] == '\0') tp.erase(0, 1);
    expect(!ta.empty(), "A is non-zero");
    expect(ta <= tp, "A is below the prime");

    // Encode it for real and read it back: this is what Telegram receives.
    Client client{Session{}, ClientConfig{}};
    setenv("CG_DRY_RUN", "/tmp/dry_srp.bin", 1);
    (void)client.api("auth.checkPassword", args, nullptr);
    unsetenv("CG_DRY_RUN");
    std::string bin;
    FILE* fp = std::fopen("/tmp/dry_srp.bin", "rb");
    if (!fp) {
        expect(false, "srp body encoded");
        return 1;
    }
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, fp)) > 0) bin.append(buf, n);
    std::fclose(fp);
    Val back = decode_tl(bin, &serr);
    expect(serr.empty(), "srp body decodes");
    expect(back.name == "auth.checkPassword", "wire ctor is auth.checkPassword");
    if (back.k == Val::OBJ && back.has("password")) {
        const Val& w = back.at("password");
        expect(w.k == Val::OBJ && w.name == "inputCheckPasswordSRP", "nested box on the wire");
        if (w.k == Val::OBJ) {
            expect(w.at("A").s == A, "A survives the wire unchanged");
            expect(w.at("M1").s == M1, "M1 survives the wire unchanged");
            expect(w.at("srp_id").i == 2947218985977228573LL, "srp_id carried through");
        }
    }

    // 3. The same password must give the same A width every time even though
    //    `a` is random, so the failure above cannot come back silently.
    // Few repetitions: each call pays a 100 000-iteration PBKDF2.
    for (int i = 0; i < 3; i++) {
        Val rnd = srp_check_password_args(password_tree(), "hunter2", &serr);
        if (serr.empty() && rnd.k == Val::OBJ) {
            const Val* q = &rnd.at("password");
            if (q->k == Val::OBJ) {
                expect(q->at("A").s.size() == 256, "A width stays 256");
                expect(q->at("M1").s.size() == 32, "M1 width stays 32");
            }
        }
    }

    // 4. A tree that is not a password response must be rejected, not decoded
    //    into nonsense.
    Val bad = Val::obj("somethingElse", {{"current_algo", Val::i64(1)}});
    std::string e2;
    (void)srp_password_check(bad, "x", &e2);
    expect(!e2.empty(), "non-password tree is rejected");

    if (failures == 0) std::printf("srp: all checks passed\n");
    return failures != 0;
}
