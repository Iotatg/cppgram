#pragma once
// Dynamic TL values: schema-driven encode/decode for every Telegram
// constructor (see scripts/gen_schema.py + src/schema.cpp).
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace cppgram {

struct Val;
using ValPtr = std::shared_ptr<Val>;

struct Val {
    enum K : uint8_t { NIL, INT, NUM, STR, VEC, OBJ };
    K k = NIL;
    long long i = 0;
    double d = 0;
    std::string s;                                   // STR bytes
    std::vector<Val> v;                              // VEC elements
    std::map<std::string, Val> o;                    // OBJ fields (name -> value)
    uint32_t crc = 0;                                // OBJ constructor
    std::string type;                                // OBJ boxed type name
    bool unknown = false;                            // OBJ: unrecognized ctor, bytes in s
    std::string name;                                // OBJ: constructor name

    Val() = default;
    static Val i64(long long x) { Val v; v.k = INT; v.i = x; return v; }
    static Val dbl(double x) { Val v; v.k = NUM; v.d = x; return v; }
    static Val str(std::string x) { Val v; v.k = STR; v.s = std::move(x); return v; }
    static Val obj(std::string ctor, std::map<std::string, Val> fields = {}) {
        Val v; v.k = OBJ; v.name = std::move(ctor); v.o = std::move(fields); return v;
    }
    static Val vec(std::vector<Val> x) { Val v; v.k = VEC; v.v = std::move(x); return v; }

    bool is_int() const { return k == INT || k == NUM; }
    long long as_int() const { return k == NUM ? static_cast<long long>(d) : i; }
    const std::map<std::string, Val>& fields() const { return o; }

    // Convenience accessors (NIL-safe).
    bool has(const std::string& key) const { return o.count(key) > 0; }
    const Val& at(const std::string& key) const;
    long long num(const std::string& key) const;
    const std::string& text(const std::string& key) const;
    std::vector<Val> list(const std::string& key) const;
    // Dig "a.b.c"; returns NIL static value on miss.
    const Val& dig(const std::string& path) const;
};

namespace schema {
// Lookups over the generated tables.
int ctor_by_name(const std::string& name);   // -1 miss
int ctor_by_crc(uint32_t crc);               // -1 miss
const char* type_name(int type_idx);
std::vector<int> ctors_of_type(int type_ret);  // sorted ctor indices returning type
const char* name_of_type_by_ctor(int type_idx, size_t i);
}  // namespace schema

// Parse a JSON document (objects/arrays/numbers/strings/true/false/null) into
// Val trees. Object key "_ctor" selects a boxed constructor when the type has
// multiple alternatives.
Val parse_json(const std::string& text, std::string* err);
std::string to_json(const Val& v);

// Encode an OBJ value against a schema constructor (index into kCtors).
// Returns serialized TL body (with leading crc) or empty with *err set.
std::string encode_ctor(int ctor_idx, const Val& v, std::string* err);
// Decode a TL body into an OBJ Val (recursively schema-aware). On unknown
// constructor the node is marked unknown=true with the raw remainder in .s.
Val decode_tl(const std::string& body, std::string* err);
// Decode one boxed TL object starting at `offset` inside a larger buffer
// (e.g. an inline Message inside an updates blob).
Val decode_tl_at(const std::string& body, size_t offset, std::string* err);

}  // namespace cppgram
