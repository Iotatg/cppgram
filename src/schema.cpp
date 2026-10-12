// Generic schema-driven TL encode/decode engine + JSON bridge.
#include "cppgram/args.hpp"
#include "cppgram/schema_data.hpp"
#include "cppgram/tl.hpp"
#include "cppgram/client.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <vector>

namespace cppgram {

namespace {
constexpr uint32_t kVectorCrc = 0x1cb5c415u;
constexpr uint32_t kBoolTrue = 0x997275b5u;
constexpr uint32_t kBoolFalse = 0xbc799737u;
const Val kNil;
}  // namespace

const Val& Val::at(const std::string& key) const {
    if (k != OBJ) return kNil;
    auto it = o.find(key);
    return it == o.end() ? kNil : it->second;
}
long long Val::num(const std::string& key) const { return at(key).as_int(); }
const std::string& Val::text(const std::string& key) const {
    const Val& v = at(key);
    return v.k == STR ? v.s : kNil.s;
}
std::vector<Val> Val::list(const std::string& key) const {
    const Val& v = at(key);
    return v.k == VEC ? v.v : std::vector<Val>{};
}
const Val& Val::dig(const std::string& path) const {
    const Val* cur = this;
    size_t pos = 0;
    while (true) {
        size_t dot = path.find('.', pos);
        std::string part = path.substr(pos, dot == std::string::npos ? dot : dot - pos);
        if (cur->k != OBJ) return kNil;
        auto it = cur->o.find(part);
        if (it == cur->o.end()) return kNil;
        cur = &it->second;
        if (dot == std::string::npos) return *cur;
        pos = dot + 1;
    }
}

namespace schema {

int ctor_by_name(const std::string& name) {
    int lo = 0, hi = static_cast<int>(schema::kCtorCount) - 1;
    while (lo <= hi) {
        int m = (lo + hi) / 2;
        int ci = schema::kCtorByName[m];
        int c = std::string(schema::kNames[schema::kCtors[ci].name]).compare(name);
        if (c == 0) return ci;
        if (c < 0) lo = m + 1; else hi = m - 1;
    }
    return -1;
}

int ctor_by_crc(uint32_t crc) {
    int lo = 0, hi = static_cast<int>(schema::kCtorCount) - 1;
    while (lo <= hi) {
        int m = (lo + hi) / 2;
        int ci = schema::kCtorByCrc[m];
        uint32_t c = schema::kCtors[ci].crc;
        if (c == crc) return ci;
        if (c < crc) lo = m + 1; else hi = m - 1;
    }
    return -1;
}

const char* type_name(int type_idx) {
    if (type_idx < 0 || type_idx >= static_cast<int>(schema::kTypeCount)) return "?";
    return schema::kNames[schema::kTypes[type_idx].name];
}

const char* name_of_type_by_ctor(int type_idx, size_t i) {
    const auto& tr = schema::kTypes[type_idx];
    if (static_cast<int>(i) >= tr.ccount) return "";
    return schema::kNames[schema::kCtors[tr.cstart + i].name];
}

std::vector<int> ctors_of_type(int type_ret) {
    std::vector<int> out;
    if (type_ret < 0 || type_ret >= static_cast<int>(schema::kTypeCount)) return out;
    const auto& tr = schema::kTypes[type_ret];
    for (int i = 0; i < tr.ccount; i++) out.push_back(tr.cstart + i);
    return out;
}

}  // namespace schema

// Callers pass nullptr for "I don't care about errors", so every check has to
// survive a null pointer just like the write side already does.
inline bool has_err(const std::string* err) { return err && !err->empty(); }

// ---------------------------------------------------------------- JSON parse

namespace {

struct JsonParser {
    const std::string& s;
    size_t p = 0;
    std::string* err;
    bool failed = false;
    int depth = 0;

    // json.loads raises RecursionError around depth 1000; mirror that instead of
    // overflowing the stack on hostile or malformed input.
    static constexpr int kMaxDepth = 1024;
    struct DepthGuard {
        int& d;
        explicit DepthGuard(int& d) : d(d) { ++d; }
        ~DepthGuard() { --d; }
    };

    // Only the four JSON whitespace characters count; isspace() would let a
    // vertical tab or form feed through as if it were insignificant.
    void ws() {
        while (p < s.size() && (s[p] == ' ' || s[p] == '\t' || s[p] == '\n' || s[p] == '\r')) p++;
    }
    Val fail(const std::string& msg) {
        if (err && err->empty()) *err = "json@" + std::to_string(p) + ": " + msg;
        failed = true;
        return Val();
    }
    Val parse() {
        ws();
        if (p >= s.size()) return fail("empty");
        char c = s[p];
        if (c == '{' || c == '[') {
            if (depth >= kMaxDepth) return fail("nesting too deep");
            DepthGuard g(depth);
            return c == '{' ? obj() : arr();
        }
        if (c == '"') return string();
        if (s.compare(p, 4, "true") == 0) { p += 4; return Val::i64(1); }
        if (s.compare(p, 5, "false") == 0) { p += 5; return Val::i64(0); }
        if (s.compare(p, 4, "null") == 0) { p += 4; return Val(); }
        return number();
    }
    Val obj() {
        Val v; v.k = Val::OBJ;
        p++;  // {
        ws();
        if (p < s.size() && s[p] == '}') { p++; return v; }
        while (p < s.size() && !failed) {
            ws();
            if (s[p] != '"') return fail("expect key");
            Val key = string();
            if (failed) return v;
            ws();
            if (p >= s.size() || s[p] != ':') return fail("expect colon");
            p++;
            Val val = parse();
            if (failed) return v;
            // The constructor name lives in v.name; keep it out of o so it is not
            // printed twice when the tree is serialized again.
            if (key.s == "_ctor" && val.k == Val::STR) v.name = val.s;
            else v.o[key.s] = val;
            ws();
            if (p >= s.size()) return fail("unterminated object");
            if (s[p] == ',') { p++; continue; }
            if (s[p] == '}') { p++; break; }
            return fail("expect , or }");
        }
        return v;
    }
    Val arr() {
        Val v; v.k = Val::VEC;
        p++;
        ws();
        if (p < s.size() && s[p] == ']') { p++; return v; }
        while (p < s.size() && !failed) {
            Val e = parse();
            if (failed) return v;
            v.v.push_back(std::move(e));
            ws();
            if (p >= s.size()) return fail("unterminated array");
            if (s[p] == ',') { p++; continue; }
            if (s[p] == ']') { p++; break; }
            return fail("expect , or ]");
        }
        return v;
    }
    Val v_() { return Val(); }

    // Reads exactly four hex digits as a \uXXXX escape code point.
    bool hex4(unsigned* out) {
        if (p + 4 > s.size()) { fail("\\u escape truncated"); return false; }
        unsigned cp = 0;
        for (int i = 0; i < 4; i++) {
            char h = s[p++];
            cp <<= 4;
            if (h >= '0' && h <= '9') cp += static_cast<unsigned>(h - '0');
            else if (h >= 'a' && h <= 'f') cp += static_cast<unsigned>(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') cp += static_cast<unsigned>(h - 'A' + 10);
            else { fail("\\u escape"); return false; }
        }
        *out = cp;
        return true;
    }
    static void append_utf8(std::string& out, unsigned cp) {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    Val string() {
        std::string out;
        p++;  // opening quote
        bool closed = false;
        while (p < s.size()) {
            unsigned char c = static_cast<unsigned char>(s[p++]);
            if (c == '"') { closed = true; break; }
            if (c == '\\') {
                if (p >= s.size()) break;
                char e = s[p++];
                switch (e) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case '/': out += '/'; break;
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case 'u': {
                        unsigned cp = 0;
                        if (!hex4(&cp)) return v_();
                        // A surrogate pair encodes one code point; a lone surrogate is
                        // not representable in UTF-8, so it is rejected rather than
                        // emitted as the invalid bytes Python keeps in its str.
                        if (cp >= 0xD800 && cp <= 0xDBFF) {
                            if (p + 2 > s.size() || s.compare(p, 2, "\\u") != 0) {
                                fail("unpaired high surrogate");
                                return v_();
                            }
                            const size_t save = p;
                            p += 2;
                            unsigned lo = 0;
                            if (!hex4(&lo)) return v_();
                            if (lo < 0xDC00 || lo > 0xDFFF) {
                                p = save;
                                fail("unpaired high surrogate");
                                return v_();
                            }
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                            fail("unpaired low surrogate");
                            return v_();
                        }
                        append_utf8(out, cp);
                        break;
                    }
                    default: fail("bad escape");
                }
                if (failed) return v_();
            } else if (c < 0x20) {
                // Raw control characters are only legal in JSON as two-char escapes.
                return fail("control character in string");
            } else {
                out += static_cast<char>(c);
            }
        }
        if (!closed) return fail("unterminated string");
        return Val::str(std::move(out));
    }
    Val number() {
        // json.loads accepts the three IEEE literals but nothing else beyond the
        // grammar, so 01, 1e, 1. and a bare + all have to fail here.
        if (s.compare(p, 9, "-Infinity") == 0) { p += 9; return Val::dbl(-HUGE_VAL); }
        if (s.compare(p, 8, "Infinity") == 0) { p += 8; return Val::dbl(HUGE_VAL); }
        if (s.compare(p, 3, "NaN") == 0) { p += 3; return Val::dbl(NAN); }
        size_t start = p;
        bool is_real = false;
        if (p < s.size() && s[p] == '-') p++;
        if (p >= s.size() || !isdigit(static_cast<unsigned char>(s[p]))) return fail("number");
        if (s[p] == '0') {
            p++;  // a leading zero rules out more integer digits
        } else {
            while (p < s.size() && isdigit(static_cast<unsigned char>(s[p]))) p++;
        }
        if (p < s.size() && s[p] == '.') {
            p++;
            if (p >= s.size() || !isdigit(static_cast<unsigned char>(s[p]))) return fail("number");
            while (p < s.size() && isdigit(static_cast<unsigned char>(s[p]))) p++;
            is_real = true;
        }
        if (p < s.size() && (s[p] == 'e' || s[p] == 'E')) {
            p++;
            if (p < s.size() && (s[p] == '+' || s[p] == '-')) p++;
            if (p >= s.size() || !isdigit(static_cast<unsigned char>(s[p]))) return fail("number");
            while (p < s.size() && isdigit(static_cast<unsigned char>(s[p]))) p++;
            is_real = true;
        }
        const std::string tok = s.substr(start, p - start);
        if (!is_real) return Val::i64(strtoll(tok.c_str(), nullptr, 10));
        return Val::dbl(strtod(tok.c_str(), nullptr));
    }
    // A JSON document is exactly one value: anything but whitespace left over is
    // the "two top-level values" case.
    void finish() {
        ws();
        if (p < s.size()) fail("trailing data");
    }
};

}  // namespace

Val parse_json(const std::string& text, std::string* err) {
    JsonParser jp{text, 0, err};
    Val v = jp.parse();
    jp.finish();
    if (jp.failed) return Val();
    return v;
}

// ---------------------------------------------------------------- to_json

namespace {

bool valid_utf8(const std::string& s) {
    size_t i = 0, n = s.size();
    while (i < n) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) { i++; continue; }
        unsigned need;
        unsigned cp;
        if ((c & 0xE0) == 0xC0) { need = 1; cp = c & 0x1Fu; }
        else if ((c & 0xF0) == 0xE0) { need = 2; cp = c & 0x0Fu; }
        else if ((c & 0xF8) == 0xF0) { need = 3; cp = c & 0x07u; }
        else return false;
        if (i + need >= n) return false;
        for (unsigned k = 1; k <= need; k++) {
            unsigned char cc = static_cast<unsigned char>(s[i + k]);
            if ((cc & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (cc & 0x3Fu);
        }
        if (need == 1 && cp < 0x80) return false;
        if (need == 2 && cp < 0x800) return false;
        if (need == 3 && cp < 0x10000) return false;
        if (cp > 0x10FFFF) return false;
        i += need + 1;
    }
    return true;
}

void esc(std::string* out, const std::string& s) {
    const bool utf8ok = valid_utf8(s);
    *out += '"';
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { *out += '\\'; *out += static_cast<char>(c); }
        else if (c == '\n') out->append("\\n");
        else if (c == '\r') out->append("\\r");
        else if (c == '\t') out->append("\\t");
        else if (c < 0x20 || c == 0x7f || (c >= 0x80 && !utf8ok)) {
            char b[8];
            std::snprintf(b, sizeof(b), "\\u%04x", c);
            out->append(b);
        } else {
            *out += static_cast<char>(c);
        }
    }
    *out += '"';
}

void val_to_json(std::string* out, const Val& v) {
    switch (v.k) {
        case Val::NIL: out->append("null"); break;
        case Val::INT: { char b[24]; std::snprintf(b, sizeof(b), "%lld", v.i); out->append(b); } break;
        case Val::NUM: { char b[32]; std::snprintf(b, sizeof(b), "%.17g", v.d); out->append(b); } break;
        case Val::STR: esc(out, v.s); break;
        case Val::VEC: {
            *out += '[';
            for (size_t i = 0; i < v.v.size(); i++) {
                if (i) *out += ',';
                val_to_json(out, v.v[i]);
            }
            *out += ']';
            break;
        }
        case Val::OBJ: {
            *out += '{';
            bool first = true;
            auto kv = [&](const char* k, const char* v) {
                if (!first) *out += ',';
                first = false;
                esc(out, k);
                *out += ':';
                esc(out, v);
            };
            char crcb[12];
            std::snprintf(crcb, sizeof(crcb), "0x%08x", v.crc);
            if (v.unknown) {
                kv("_unknown_crc", crcb);
                std::string hex;
                static const char* hd = "0123456789abcdef";
                for (size_t i = 0; i < std::min<size_t>(64, v.s.size()); i++) {
                    hex += hd[static_cast<unsigned char>(v.s[i]) >> 4];
                    hex += hd[static_cast<unsigned char>(v.s[i]) & 15];
                }
                kv("_raw_hex", hex.c_str());
            } else {
                if (!v.name.empty()) kv("_ctor", v.name.c_str());
                if (v.crc) kv("_crc", crcb);
                for (const auto& [k, fv] : v.o) {
                    if (fv.k == Val::NIL) continue;
                    if (k == "_ctor") continue;
                    if (!first) *out += ',';
                    first = false;
                    esc(out, k);
                    *out += ':';
                    val_to_json(out, fv);
                }
            }
            *out += '}';
            break;
        }
    }
}

}  // namespace

std::string to_json(const Val& v) {
    std::string out;
    val_to_json(&out, v);
    return out;
}

// ---------------------------------------------------------------- encode

namespace {

int single_ctor_of_type(int type_idx) {
    const auto& tr = schema::kTypes[type_idx];
    return tr.ccount == 1 ? tr.cstart : -1;
}

std::string encode_object(int ctor_idx, const Val& v, std::string* err);

// resolve a boxed field value: returns ctor idx, -1 on failure (err set)
int resolve_boxed_ctor(int type_idx, const Val& fv, const char* ctx, std::string* err) {
    if (fv.k == Val::OBJ && !fv.name.empty()) {
        int ci = schema::ctor_by_name(fv.name);
        if (ci < 0) {
            if (err) *err = std::string(ctx) + ": unknown _ctor " + fv.name;
            return -1;
        }
        if (type_idx >= 0 && schema::kCtors[ci].type != type_idx) {
            if (err && schema::type_name(schema::kCtors[ci].type) !=
                           std::string(schema::type_name(type_idx))) {
                // allow loose match: _ctor of exact right crc only for "Vector"...
                *err = std::string(ctx) + ": _ctor " + fv.name + " returns " +
                       schema::type_name(schema::kCtors[ci].type) + ", expected " +
                       schema::type_name(type_idx);
                return -1;
            }
        }
        return ci;
    }
    if (fv.k == Val::STR) {
        int ci = schema::ctor_by_name(fv.s);  // allow "inputPeerEmpty" shorthand
        if (ci >= 0 && (type_idx < 0 || schema::kCtors[ci].type == type_idx)) return ci;
        if (err) *err = std::string(ctx) + ": string value not a valid constructor of " +
                        schema::type_name(type_idx);
        return -1;
    }
    int solo = single_ctor_of_type(type_idx);
    if (solo >= 0) return solo;
    if (type_idx >= 0 && std::string(schema::type_name(type_idx)) == "Bool") {
        return fv.k == Val::INT && fv.i != 0 ? schema::ctor_by_name("boolTrue")
                                             : schema::ctor_by_name("boolFalse");
    }
    if (err) *err = std::string(ctx) + ": type " + schema::type_name(type_idx) +
                    " is abstract; set \"_ctor\"";
    return -1;
}

bool encode_scalar(int kind, int /*raw_type*/, const Val& fv, TlWriter& w, std::string* err,
                   const char* ctx) {
    switch (kind) {
        case 0: w.i32(static_cast<int32_t>(fv.as_int())); return true;
        case 1: w.i64(fv.as_int()); return true;
        case 2: {
            double dv = fv.k == Val::NUM ? fv.d : static_cast<double>(fv.i);
            uint64_t bits = 0;
            std::memcpy(&bits, &dv, 8);
            bits &= 0xFFFFFFFFFFFF0000ull;
            char buf[8];
            std::memcpy(buf, &bits, 8);
            w.raw(std::string(buf, 8));
            return true;
        }
        case 3: case 4:
            if (fv.k != Val::STR) { if (err) *err = std::string(ctx) + ": expect string"; return false; }
            w.str(fv.s); return true;
        case 9: case 10: {
            size_t want = kind == 9 ? 32 : 56;
            if (fv.k != Val::STR || fv.s.size() != want) {
                if (err) *err = std::string(ctx) + ": expect " + std::to_string(want) + "-byte value";
                return false;
            }
            w.raw(fv.s); return true;
        }
        default: return false;
    }
}

bool encode_value(int kind, int type_idx, int elem_kind, int elem_type, const Val& fv,
                  TlWriter& w, std::string* err, const char* ctx) {
    if (kind == 8) {  // vector
        if (fv.k != Val::VEC) { if (err) *err = std::string(ctx) + ": expect array"; return false; }
        w.u32(kVectorCrc);
        w.i32(static_cast<int32_t>(fv.v.size()));
        int n = 0;
        for (const Val& e : fv.v) {
            char cc[64];
            std::snprintf(cc, sizeof(cc), "%s[%d]", ctx, n++);
            if (!encode_value(elem_kind, elem_type, 0, 0xFFFF, e, w, err, cc)) return false;
        }
        return true;
    }
    if (kind == 7) {  // boxed
        if (fv.k == Val::NIL) { if (err) *err = std::string(ctx) + ": missing"; return false; }
        if (elem_kind == 7 && elem_type != 0xFFFF) {
            // vector element handled by recursion above (elem_kind path)
        }
        int ci = resolve_boxed_ctor(type_idx, fv, ctx, err);
        if (ci < 0) return false;
        std::string sub = encode_object(ci, fv, err);
        if (sub.empty()) return false;
        w.raw(sub);
        return true;
    }
    return encode_scalar(kind, type_idx, fv, w, err, ctx);
}

std::string encode_object(int ctor_idx, const Val& v, std::string* err) {
    const schema::CtorRow& cr = schema::kCtors[ctor_idx];
    const char* cname = schema::kNames[cr.name];
    if (std::getenv("CG_ENCODE_REPORT")) {
        // Fields the schema does not know are dropped without a trace, which is
        // how stale field names (payments.getResaleStarGifts once took a
        // "query") went unnoticed. Complain loudly when asked to.
        std::string unknown;
        for (const auto& [key, kv] : v.o) {
            (void)kv;
            if (key == "_ctor" || key == "_unknown_crc") continue;
            bool known = false;
            for (int i = 0; i < cr.fcount; ++i) {
                if (key == schema::kNames[schema::kFields[cr.fstart + i].name]) {
                    known = true;
                    break;
                }
            }
            if (!known) unknown += (unknown.empty() ? "" : ",") + key;
        }
        if (!unknown.empty()) {
            std::fprintf(stderr, "encode: %s: unknown field(s) %s\n", cname, unknown.c_str());
        }
        for (int i = 0; i < cr.fcount; ++i) {
            const schema::FieldRow& fr = schema::kFields[cr.fstart + i];
            if (fr.kind == 11 || fr.kind == 5 || fr.flag != 0xFFFF) continue;
            if (v.at(schema::kNames[fr.name]).k == Val::NIL) {
                std::fprintf(stderr, "encode: %s: %s left at default\n", cname,
                             schema::kNames[fr.name]);
            }
        }
    }
    TlWriter w;
    w.u32(cr.crc);
    uint32_t masks[4] = {0, 0, 0, 0};
    // pre-scan optionals to build flag words
    for (int i = 0; i < cr.fcount; i++) {
        const schema::FieldRow& fr = schema::kFields[cr.fstart + i];
        if (fr.kind == 11) continue;
        if (fr.flag == 0xFFFF) continue;
        if (fr.kind == 5) {  // true-typed flag
            const Val& fv = v.at(schema::kNames[fr.name]);
            if (fv.k == Val::NIL || (fv.k == Val::INT && fv.i == 0)) continue;
            masks[fr.flag / 32] |= 1u << (fr.flag % 32);
            continue;
        }
        const Val& fv = v.at(schema::kNames[fr.name]);
        if (fv.k != Val::NIL) masks[fr.flag / 32] |= 1u << (fr.flag % 32);
    }
    for (int i = 0; i < cr.fcount; i++) {
        const schema::FieldRow& fr = schema::kFields[cr.fstart + i];
        const char* fname = schema::kNames[fr.name];
        if (fr.kind == 11) {  // flag word
            w.i32(static_cast<int32_t>(masks[fr.type]));
            continue;
        }
        if (fr.kind == 5) continue;  // pure flag marker
        if (fr.flag != 0xFFFF && !(masks[fr.flag / 32] & (1u << (fr.flag % 32)))) continue;
        const Val& fv = v.at(fname);
        if (fr.kind == 6) {  // Bool via INT value
            TlWriter bw;
            bw.u32(fv.k != Val::NIL && fv.as_int() != 0 ? kBoolTrue : kBoolFalse);
            w.raw(bw.out());
            continue;
        }
        Val scratch;
        const Val* fv_ptr = &fv;
        if (fv.k == Val::NIL) {
            // ergonomic defaults (like python clients): scalars zero, empty
            // vectors, single-ctor boxed types auto-selected.
            scratch.k = (fr.kind == 3 || fr.kind == 4) ? Val::STR : Val::NIL;
            if (fr.kind <= 2 || fr.kind == 6) { scratch.k = Val::INT; scratch.i = 0; }
            else if (fr.kind == 8) scratch.k = Val::VEC;
            else if (fr.kind == 7) {
                int solo = single_ctor_of_type(fr.type);
                if (solo < 0) {
                    // well-known empty defaults for abstract input types
                    static const char* kDefs[][2] = {
                        {"InputPeer", "inputPeerEmpty"},
                        {"InputUser", "inputUserEmpty"},
                        {"InputChat", "inputChatEmpty"},
                        {"InputNotifyPeer", "inputNotifyEveryone"},
                        {"PeerNotifySettings", "peerNotifySettings"},
                        {"InputMessagesFilter", "inputMessagesFilterEmpty"},
                    };
                    for (auto& kv : kDefs) {
                        if (std::string(schema::type_name(fr.type)) == kv[0]) {
                            int ci = schema::ctor_by_name(kv[1]);
                            if (ci >= 0) { solo = ci; break; }
                        }
                    }
                }
                if (solo < 0) {
                    if (err) *err = std::string(cname) + "." + fname + ": missing (abstract type)";
                    return {};
                }
                scratch.k = Val::OBJ;
                scratch.name = schema::kNames[schema::kCtors[solo].name];
            } else {
                if (err) *err = std::string(cname) + "." + fname + ": required field missing";
                return {};
            }
            fv_ptr = &scratch;
        }
        const Val& fv2 = *fv_ptr;
        char ctx[96];
        std::snprintf(ctx, sizeof(ctx), "%s.%s", cname, fname);
        if (!encode_value(fr.kind, fr.kind == 7 ? fr.type : 0xFFFF,
                          fr.kind == 8 ? fr.elem_kind : 0,
                          fr.kind == 8 ? fr.elem_type : 0xFFFF, fv2, w, err, ctx)) {
            return {};
        }
    }
    return w.out();
}

}  // namespace

std::string encode_ctor(int ctor_idx, const Val& v, std::string* err) {
    if (ctor_idx < 0 || ctor_idx >= static_cast<int>(schema::kCtorCount)) {
        if (err) *err = "unknown constructor index";
        return {};
    }
    if (v.k != Val::OBJ) {
        if (err) *err = "expect object argument";
        return {};
    }
    return encode_object(ctor_idx, v, err);
}

// ---------------------------------------------------------------- decode

namespace {

struct Decoder {
    const std::string& b;
    size_t p = 0;
    std::string* err;
    bool dead = false;
    int depth = 0;

    // Recursive TL types (jsonObject/jsonArray) can nest arbitrarily deep in a
    // hostile payload; bound the recursion instead of overflowing the stack.
    static constexpr int kMaxDepth = 1024;
    struct DepthGuard {
        int& d;
        explicit DepthGuard(int& d) : d(d) { ++d; }
        ~DepthGuard() { --d; }
    };

    uint32_t u32() {
        uint32_t v = 0;
        if (p + 4 > b.size()) { dead = true; return 0; }
        std::memcpy(&v, b.data() + p, 4);
        p += 4;
        return v;
    }
    int64_t i64() {
        uint64_t v = 0;
        if (p + 8 > b.size()) { dead = true; return 0; }
        std::memcpy(&v, b.data() + p, 8);
        p += 8;
        return static_cast<int64_t>(v);
    }
    double dtag() {
        uint64_t raw = 0;
        if (p + 8 > b.size()) { dead = true; return 0; }
        std::memcpy(&raw, b.data() + p, 8);
        p += 8;
        // tl double is float64 with last 2 bytes zero
        raw &= 0xFFFFFFFFFFFF0000ull;
        double out = 0;
        std::memcpy(&out, &raw, 8);
        return out;
    }
    std::string strn(size_t n) {
        if (p + n > b.size()) { dead = true; return {}; }
        std::string out = b.substr(p, n);
        p += n;
        return out;
    }
    std::string str() {
        TlReader dummy(b, 0);
        (void)dummy;
        size_t save = p;
        unsigned char l0 = u8();
        size_t len, hdr;
        if (l0 < 254) { len = l0; hdr = 1; } else {
            if (p + 3 > b.size()) { dead = true; return {}; }
            len = static_cast<unsigned char>(b[p]) |
                  (static_cast<unsigned char>(b[p + 1]) << 8) |
                  (static_cast<unsigned char>(b[p + 2]) << 16);
            hdr = 4;
            p += 3;  // consume extended length bytes
        }
        size_t pad = (4 - ((hdr + len) % 4)) % 4;
        if (p + len + pad > b.size()) { dead = true; p = save; return {}; }
        std::string out = b.substr(p, len);
        p += len + pad;
        return out;
    }
    unsigned char u8() {
        if (p >= b.size()) { dead = true; return 0; }
        return static_cast<unsigned char>(b[p++]);
    }

    Val decode_boxed(int expected_type, const std::string& path = "", size_t st = 0) {
        if (p + 4 > b.size()) { dead = true; return Val(); }
        if (depth >= kMaxDepth) { dead = true; if (err && err->empty()) *err = "tl: nesting too deep"; return Val(); }
        DepthGuard g(depth);
        st = p;
        uint32_t crc = u32();
        int ci = schema::ctor_by_crc(crc);
        if (ci < 0) {
            Val v;
            v.k = Val::OBJ;
            v.crc = crc;
            v.unknown = true;
            v.s = b.substr(p);  // cannot continue; consume remainder
            p = b.size();
            return v;
        }
        if (crc == kBoolTrue || crc == kBoolFalse) {
            Val v = Val::i64(crc == kBoolTrue ? 1 : 0);
            return v;
        }
        const schema::CtorRow& cr = schema::kCtors[ci];
        static const bool trace = std::getenv("CG_TRACE") != nullptr;
        if (trace)
            std::fprintf(stderr, "[tr] %s %s start=%zu\n", path.c_str(), schema::kNames[cr.name], st);
        Val v;
        v.k = Val::OBJ;
        v.crc = crc;
        v.name = schema::kNames[cr.name];
        v.type = schema::type_name(cr.type);
        (void)expected_type;
        uint32_t masks[4] = {0, 0, 0, 0};
        for (int f = 0; f < cr.fcount && !dead; f++) {
            const schema::FieldRow& fr = schema::kFields[cr.fstart + f];
            const char* fname = schema::kNames[fr.name];
            if (fr.kind == 11) {  // flag word
                masks[fr.type] = u32();
                continue;
            }
            if (fr.kind == 5) {  // true flag
                bool on = fr.flag != 0xFFFF && (masks[fr.flag / 32] & (1u << (fr.flag % 32)));
                if (on) v.o[fname] = Val::i64(1);
                continue;
            }
            if (fr.flag != 0xFFFF && !(masks[fr.flag / 32] & (1u << (fr.flag % 32)))) {
                v.o[fname] = Val();
                continue;
            }
            if (fr.kind == 8) {  // vector
                std::string fpath = path + "." + fname;
                uint32_t vm = u32();
                if (vm != kVectorCrc) {
                    std::fprintf(stderr, "[vfail] %s off=%zu got=%08x msg_start=%zu crcs:%s\n", fpath.c_str(), p - 4, vm, st, "chainsee");
                    if (err && err->empty()) *err = "decode " + fpath + ": bad vector magic";
                    dead = true;
                    return v;
                }
                int32_t n = static_cast<int32_t>(u32());
                if (n < 0 || n > 200000) { dead = true; return v; }
                Val vec = Val::vec({});
                for (int32_t i = 0; i < n && !dead; i++) {
                    if (fr.elem_kind == 7) vec.v.push_back(decode_boxed(fr.elem_type, fpath + "[" + std::to_string(i) + "]"));
                    else vec.v.push_back(decode_scalar(fr.elem_kind));
                }
                v.o[fname] = std::move(vec);
                continue;
            }
            if (fr.kind == 7) {
                Val child = decode_boxed(fr.type, path + "." + fname, st);
                if (dead) return v;
                v.o[fname] = std::move(child);
                continue;
            }
            if (fr.kind == 6) {  // Bool
                if (p + 4 > b.size()) { dead = true; return v; }
                uint32_t bc = u32();
                v.o[fname] = Val::i64(bc == kBoolTrue ? 1 : 0);
                continue;
            }
            v.o[fname] = decode_scalar(fr.kind);
        }
        if (trace)
            std::fprintf(stderr, "[tr] %s %s end=%zu\n", path.c_str(), schema::kNames[cr.name], p);
        return v;
    }

    Val decode_scalar(int kind) {
        switch (kind) {
            case 0: return Val::i64(static_cast<int32_t>(u32()));
            case 1: return Val::i64(i64());
            case 2: {
                Val v;
                v.k = Val::NUM;
                v.d = dtag();
                return v;
            }
            case 3: case 4: return Val::str(str());
            case 9: case 10: return Val::str(strn(kind == 9 ? 32 : 56));
            default: return Val();
        }
    }
};

}  // namespace

Val decode_tl(const std::string& body, std::string* err) {
    Decoder d{body, 0, err};
    // some methods answer with a bare Vector<...> instead of a boxed result
    if (body.size() >= 8) {
        uint32_t top = 0;
        std::memcpy(&top, body.data(), 4);
        if (top == 0x1cb5c415u) {
            d.p = 4;
            int32_t n = static_cast<int32_t>(d.u32());
            Val vec = Val::vec({});
            if (n >= 0 && n <= 200000) {
                for (int32_t i = 0; i < n && !d.dead; i++) vec.v.push_back(d.decode_boxed(-1));
            }
            if (d.dead && err && err->empty()) *err = "tl decode: truncated vector";
            return vec;
        }
    }
    Val v = d.p < body.size() ? d.decode_boxed(-1) : Val();
    if (d.dead && err && err->empty()) *err = "tl decode: truncated";
    return v;
}

Val decode_tl_at(const std::string& body, size_t offset, std::string* err) {
    if (offset + 4 > body.size()) {
        if (err) *err = "tl decode: offset past end";
        return {};
    }
    Decoder d{body, offset, err};
    Val v = d.decode_boxed(-1);
    if (d.dead && err && err->empty()) *err = "tl decode: truncated";
    return v;
}

// ------------------------------------------------------------- Client::api

Val Client::api(const std::string& method, const Val& args, std::string* err) {
    if (method.empty()) {
        if (err) *err = "api: empty method";
        return {};
    }
    int by_name = schema::ctor_by_name(method);
    if (by_name < 0) {
        if (err) *err = "api: unknown method " + method;
        return {};
    }
    if (!schema::kCtors[by_name].is_fn) {
        if (err) *err = "api: " + method + " is a type, not a function";
        return {};
    }
    std::string body = encode_ctor(by_name, args, err);
    if (body.empty()) return {};
    // CG_DRY_RUN: the dump replaces the call entirely, so a wrapper's wire
    // shape can be checked against the schema offline.
    if (const char* dry = std::getenv("CG_DRY_RUN")) {
        FILE* fp = std::fopen(dry, "wb");
        if (fp) { std::fwrite(body.data(), 1, body.size(), fp); std::fclose(fp); }
        return {};
    }
    if (const char* d = std::getenv("CG_DUMP_API")) {
        FILE* fp = std::fopen(d, "wb");
        if (fp) { std::fwrite(body.data(), 1, body.size(), fp); std::fclose(fp); }
    }
    std::string resp = call_rpc(body, err, true);
    if (resp.empty()) {
        if (err && !err->empty()) {
            Val e = Val::obj("rpc_error", {{"code", Val::i64(0)}, {"message", Val::str(*err)}});
            std::vector<std::pair<int, std::function<void(const Val&)>>> hs;
            {
                std::lock_guard<std::mutex> g(typed_mu_);
                auto it = typed_handlers_.find("error");
                if (it != typed_handlers_.end()) hs = it->second;
            }
            for (auto& p : hs) if (p.second) p.second(e);
        }
        return {};
    }
    if (const char* d = std::getenv("CG_DUMP_RESP")) {
        FILE* fp = std::fopen(d, "wb");
        if (fp) { std::fwrite(resp.data(), 1, resp.size(), fp); std::fclose(fp); }
    }
    Val decoded = decode_tl(resp, err);
    if (decoded.name == "rpc_error" || (err && !err->empty())) {
        Val e = decoded.name == "rpc_error" ? decoded
            : Val::obj("rpc_error", {{"code", Val::i64(0)}, {"message", Val::str(err ? *err : "")}});
        std::vector<std::pair<int, std::function<void(const Val&)>>> hs;
        {
            std::lock_guard<std::mutex> g(typed_mu_);
            auto it = typed_handlers_.find("error");
            if (it != typed_handlers_.end()) hs = it->second;
        }
        for (auto& p : hs) if (p.second) p.second(e);
    }
    return decoded;
}

Val Client::api_json(const std::string& method, const std::string& args_json, std::string* err) {
    Val args;
    if (!args_json.empty()) {
        args = parse_json(args_json, err);
        if (has_err(err)) return {};
    }
    return api(method, args, err);
}

}  // namespace cppgram
