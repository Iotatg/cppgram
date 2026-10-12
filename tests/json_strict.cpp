// Regression tests for the strict JSON reader in src/schema.cpp. Inputs marked
// bad have to be rejected instead of silently decoding, and every good input is
// compared against json.loads on the reference corpus.
#include "cppgram/args.hpp"
#include "cppgram/tl.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace cppgram;

namespace {

int failures = 0;

std::string hexdump(const std::string& s) {
    std::string out;
    static const char* hd = "0123456789abcdef";
    for (unsigned char c : s) {
        out += hd[c >> 4];
        out += hd[c & 15];
        out += ' ';
    }
    return out;
}

void bad(const char* text) {
    std::string err;
    Val v = parse_json(text, &err);
    if (err.empty()) {
        std::printf("ACCEPTED %s -> %s\n", hexdump(text).c_str(), hexdump(to_json(v)).c_str());
        ++failures;
    }
}

// Compares the decoded tree against the tree json.loads would build, so a
// serializer detail (%.17g formatting, for one) cannot mask a wrong value.
void good(const char* text, const std::string& want, bool want_int, double want_num) {
    std::string err;
    Val v = parse_json(text, &err);
    if (!err.empty()) {
        std::printf("REJECTED %s (%s)\n", hexdump(text).c_str(), err.c_str());
        ++failures;
        return;
    }
    bool ok;
    std::string got;
    if (v.k == Val::STR) {
        ok = v.s == want;
        got = hexdump(v.s);
    } else if (v.k == Val::INT || v.k == Val::NUM) {
        ok = v.k == (want_int ? Val::INT : Val::NUM);
        if (want_int)
            ok = ok && v.i == std::stoll(want);
        else if (std::isnan(want_num))
            ok = ok && std::isnan(v.d);
        else if (std::isinf(want_num))
            ok = ok && v.d == want_num;
        else
            ok = ok && std::fabs(v.d - want_num) <= 1e-12 * std::fabs(want_num);
        got = std::to_string(want_int ? v.i : v.d) + (v.k == Val::INT ? " int" : " num");
    } else {
        ok = to_json(v) == want;
        got = to_json(v);
    }
    if (!ok) {
        std::printf("MISMATCH %s\n  got:  %s\n  want: %s\n", hexdump(text).c_str(), got.c_str(),
                    want.c_str());
        ++failures;
    }
}

void good_tree(const char* text, const char* want_json) {
    std::string err;
    Val v = parse_json(text, &err);
    if (!err.empty()) {
        std::printf("REJECTED %s (%s)\n", hexdump(text).c_str(), err.c_str());
        ++failures;
        return;
    }
    const std::string got = to_json(v);
    if (got != want_json) {
        std::printf("MISMATCH %s\n  got:  %s\n  want: %s\n", text, got.c_str(), want_json);
        ++failures;
    }
}

}  // namespace

int main() {
    // Unterminated or structurally broken strings.
    bad("\"unterminated");
    bad("\"tab\there\"");
    bad("\"newline\nhere\"");
    bad("\"bad escape \\q\"");
    bad("\"bad hex \\uZZZZ\"");
    bad("\"short escape \\u00\"");
    bad("\"missing u \\u0041\\uz000\"");
    bad("{\"a\" 1}");
    bad("[1 2]");
    bad("[,]");
    bad("{,}");
    bad("[1,]");
    bad("{\"a\":1,}");
    bad("{\"a\":}");
    bad("{'a':1}");
    bad("nul");
    bad("tru");
    bad("fals");

    // Trailing data: a document is exactly one value.
    bad("true false");
    bad("1 2");
    bad("{} {}");
    bad("\"a\" \"b\"");
    bad("null null");
    bad("[1] x");

    // Numbers that do not follow the grammar.
    bad("1e");
    bad("+");
    bad("+1");
    bad("01");
    bad("01 ");
    bad("-");
    bad("1.");
    bad(".5");
    bad("1e+");
    bad("1e-");
    bad("0x10");
    bad("00");
    bad("--1");
    bad("1.2.3");
    bad("1 1");
    bad("1-");

    // Lone surrogates: json.loads keeps the raw surrogate in its str, which cannot
    // become UTF-8 or be encoded on the wire, so these are rejected deliberately.
    // A high surrogate must also be paired to be usable.
    bad("\"\\ud83d\"");
    bad("\"\\ude00\"");
    bad("\"\\ud83dzz\"");
    bad("\"\\ud83d\\u0041\"");
    bad("\"\\ud83d\\n\"");

    // Whitespace is only the four JSON separators.
    bad("{}\v");
    bad("{}\f");
    bad("{}\xa0");

    // Good documents still decode to the value json.loads produces.
    good("123", "123", true, 0);
    good("-1", "-1", true, 0);
    good("0", "0", true, 0);
    good("1.5", "1.5", false, 1.5);
    good("-1e-5", "-1e-05", false, -1e-5);
    good("1e5", "1e5", false, 1e5);
    good("2.5e3", "2.5e3", false, 2500.0);
    good("Infinity", "Infinity", false, HUGE_VAL);
    good("-Infinity", "-Infinity", false, -HUGE_VAL);
    good("NaN", "NaN", false, NAN);
    good("0.125", "0.125", false, 0.125);
    good(" 1 ", "1", true, 0);
    good("\t-42\n", "-42", true, 0);
    good("\"\"", "", true, 0);
    good("\"\\u0041\\u00e9\"", "A\xc3\xa9", true, 0);
    good("\"\\ud83d\\ude00\"", "\xf0\x9f\x98\x80", true, 0);
    good("\"\\ud83e\\udd73\"", "\xf0\x9f\xa5\xb3", true, 0);
    good("\"\\uD83D\\uDC4D\\uD83E\\uDD73\"", "\xf0\x9f\x91\x8d\xf0\x9f\xa5\xb3", true, 0);
    good("\"\\u0000\\u001f\"", std::string("\0\x1f", 2), true, 0);
    good("\"tab\\tnewline\\nslash\\/quote\\\"backslash\\\\\"",
         "tab\tnewline\nslash/quote\"backslash\\", true, 0);
    good("\"plain utf8 \xc3\xa9\xe2\x82\xac\"", "plain utf8 \xc3\xa9\xe2\x82\xac", true, 0);
    good("true", "1", true, 0);
    good("false", "0", true, 0);
    good("null", "null", true, 0);
    good("[]", "[]", true, 0);
    good("{}", "{}", true, 0);
    good("  [ 1 , 2 , 3 ]  ", "[1,2,3]", true, 0);
    good("\t\r\n {}\n", "{}", true, 0);

    // Tree shapes: the constructor name must appear exactly once.
    good_tree("{\"_ctor\":\"foo\",\"x\":1}", "{\"_ctor\":\"foo\",\"x\":1}");
    good_tree("{\"a\":1,\"b\":[true,null,\"x\"]}", "{\"a\":1,\"b\":[1,null,\"x\"]}");
    good_tree("{\"key\":{\"deep\":{\"deeper\":[1,[2,[3]]]}}}",
              "{\"key\":{\"deep\":{\"deeper\":[1,[2,[3]]]}}}");
    good_tree("{\"n\":null,\"i\":0,\"d\":0.0}", "{\"d\":0,\"i\":0}");
    good_tree("[{\"_ctor\":\"a\"},{\"_ctor\":\"b\",\"y\":[]}]", "[{\"_ctor\":\"a\"},{\"_ctor\":\"b\",\"y\":[]}]");

    // Re-parsing the serialization of a tree must give the same tree, which is
    // how a call argument would round-trip through api_json().
    for (const char* doc : {"{\"_ctor\":\"x\",\"a\":\"y\"}", "[1,2,3]", "\"s\"", "123", "true",
                            "null", "{}", "[]", "{\"nested\":{\"_ctor\":\"z\",\"v\":[1,2]}}"}) {
        std::string err;
        Val first = parse_json(doc, &err);
        if (!err.empty()) {
            std::printf("ROUNDTRIP rejected %s (%s)\n", doc, err.c_str());
            ++failures;
            continue;
        }
        Val second = parse_json(to_json(first), &err);
        if (!err.empty() || to_json(first) != to_json(second)) {
            std::printf("ROUNDTRIP mismatch %s -> %s -> %s\n", doc, to_json(first).c_str(),
                        to_json(second).c_str());
            ++failures;
        }
    }

    // Deterministic fuzz: the reader must never crash, and anything it accepts
    // must serialize to something that parses back to the same tree.
    {
        unsigned long long seed = 0x9e3779b97f4a7c15ull;
        auto next = [&seed]() {
            seed = seed * 6364136223846793005ull + 1442695040888963407ull;
            return static_cast<unsigned>(seed >> 33);
        };
        static const char kTokens[] = "{}[]\",:0123456789.eE+-truefalsnl \\u/t\n\"\x00\x1f";
        for (int i = 0; i < 20000; i++) {
            std::string text;
            const size_t n = next() % 24;
            for (size_t j = 0; j < n; j++) text += kTokens[next() % (sizeof(kTokens) - 1)];
            std::string err;
            Val v = parse_json(text, &err);
            if (err.empty()) {
                Val again = parse_json(to_json(v), &err);
                if (!err.empty() || to_json(again) != to_json(v)) {
                    std::printf("FUZZ roundtrip %s -> %s -> %s\n", hexdump(text).c_str(),
                                to_json(v).c_str(), to_json(again).c_str());
                    ++failures;
                }
            }
        }
    }

    if (failures == 0) std::printf("json: all cases passed\n");
    return failures != 0;
}
