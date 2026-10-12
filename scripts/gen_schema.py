#!/usr/bin/env python3
"""Generate include/cppgram/schema_data.hpp from a Telegram api.tl schema.

The generated tables drive cppgram's generic TL engine (see src/schema.cpp):
every constructor and function of the schema becomes addressable at runtime,
so no RPC or TL type is ever "missing" from the library.

Usage:
    python3 scripts/gen_schema.py [path/to/api.tl]

Default schema path: schema/api.tl (layer 229, from telegramdesktop/tdesktop).
"""
import os
import re
import sys
import hashlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "schema", "api.tl")
DST = os.path.join(ROOT, "include", "cppgram", "schema_data.hpp")

BUILTIN = {
    "#": ("INT", None),
    "int": ("INT", None),
    "long": ("LONG", None),
    "double": ("DOUBLE", None),
    "string": ("STRING", None),
    "bytes": ("BYTES", None),
    "true": ("TRUE", None),
    "Bool": ("BOOL", None),
    "int256": ("RAW32", None),
    "int448": ("RAW56", None),
}

KIND = {"INT": 0, "LONG": 1, "DOUBLE": 2, "STRING": 3, "BYTES": 4,
        "TRUE": 5, "BOOL": 6, "BOXED": 7, "VECTOR": 8, "RAW32": 9, "RAW56": 10, "FLAGWORD": 11}

CTOR_RE = re.compile(r"^(?P<name>[a-zA-Z][a-zA-Z0-9_.]*)#(?P<crc>[0-9a-f]{1,8})(?: (?P<fields>.*?))? = (?P<ret>Vector<[a-zA-Z0-9_.]+>|Vector [a-zA-Z0-9_.]+|[a-zA-Z][a-zA-Z0-9_.]*);?$", re.S)


def split_fields(text):
    out, depth, cur = [], 0, ""
    for ch in text:
        if ch == "<":
            depth += 1
        elif ch == ">":
            depth -= 1
        if ch == " " and depth == 0:
            if cur:
                out.append(cur)
                cur = ""
        else:
            cur += ch
    if cur:
        out.append(cur)
    return out


def main():
    with open(SRC, "r", encoding="utf-8") as f:
        raw = f.read()
    raw = re.sub(r"^///.*$", "", raw, flags=re.M)
    raw = re.sub(r"//[^\n]*", "", raw)
    after_functions = raw.index("---functions---") if "---functions---" in raw else 10**9
    raw = re.sub(r"---[a-z-]+---", ";", raw)

    stmts = [s.strip() for s in raw.split(";\n") if s.strip() and "#" in s]

    # Pass 1: collect type set.
    type_names = set()
    parsed = []
    pos = 0
    skipped_generic = []
    for s in stmts:
        m = CTOR_RE.match(s)
        if not m:
            continue
        head = s[: s.find("=")]
        if "{" in head or "!" in head:
            skipped_generic.append(m["name"])
            # generic function constructors (invokeAfterMsg etc.) are emitted
            # separately below from a small hand table in the engine
            pos += 1
            continue
        fields = []
        for tok in split_fields(m["fields"] or ""):
            if tok == "{X:Type}":
                continue
            fields.append(tok)
        ret = m["ret"].replace("Vector ", "Vector<")
        if ret.startswith("Vector<") and ret.endswith(">"):
            type_names.add(ret)
        else:
            type_names.add(ret)
        parsed.append((m["name"], int(m["crc"], 16), fields, ret, False))
        pos += 1

    # function-vs-type distinction: tdesktop puts functions after marker;
    # recompute precisely using original text offsets.
    fn_names = set()
    tail = raw[after_functions:]
    for m in re.finditer(r"([a-zA-Z][a-zA-Z0-9_.]*)#\w+", tail):
        fn_names.add(m.group(1))

    names = {n for (n, crc, fs, ret, _f) in parsed} | set(type_names)
    for _, _, fs, _, _ in parsed:
        for tok in fs:
            if ":" not in tok:
                continue
            names.add(tok.split(":", 1)[0])
            t = tok.split(":", 1)[1]
            base = t
            if t.startswith("Vector<") and t.endswith(">"):
                base = t[7:-1]
            base = re.sub(r"^flags\d*\.\d+\?", "", base)
            base = base.rstrip("?")
            names.add(base)
    names = sorted(names)
    nidx = {n: i for i, n in enumerate(names)}

    tlist = sorted(type_names)
    tidx = {t: i for i, t in enumerate(tlist)}

    def parse_type(t):
        t = t.rstrip("?")
        if t in BUILTIN:
            k = BUILTIN[t][0]
            if k == "INT" and t == "#":
                k = "INT"
            return k, 0xFFFF, 0
        if t.startswith("Vector<") and t.endswith(">"):
            inner = t[7:-1]
            if inner in BUILTIN:
                return "VECTOR", BUILTIN[inner][0], 0xFFFF
            return "VECTOR", "BOXED", tidx[inner]
        return "BOXED", tidx[t], 0

    ctors = []  # dict rows
    for (name, crc, fields, ret, _isfn) in parsed:
        rows = []
        for tok in fields:
            fname, ftype = (tok.split(":", 1) + [""])[:2]
            flag = 0xFFFF
            if fname == "flags" or re.fullmatch(r"flags\d*", fname):
                w = int(fname.replace("flags", "") or 0)
                rows.append((fname, KIND["FLAGWORD"], w, 0, 0xFFFF, 0xFFFF))
                continue
            opt = re.match(r"^flags(\d*)\.(\d+)\?", ftype)
            if opt:
                flag = int(opt.group(1) or 0) * 32 + int(opt.group(2))
                ftype = ftype[opt.end():]
            parsed_t = parse_type(ftype)
            if parsed_t[0] == "VECTOR":
                _, ek, et = parsed_t + (None,) * (3 - len(parsed_t))
                rows.append((fname, KIND["VECTOR"], 0xFFFF, KIND[ek], et or 0xFFFF, flag))
            elif parsed_t[0] == "BOXED":
                rows.append((fname, KIND["BOXED"], parsed_t[1], 0, 0xFFFF, flag))
            else:
                rows.append((fname, KIND[parsed_t[0]], 0xFFFF, 0, 0xFFFF, flag))
        ctors.append({"name": name, "crc": crc, "ret": ret, "fields": rows,
                      "is_fn": name in fn_names})

    # error/flag note: functions returning "updates" etc. need nothing special.
    dup = {}
    for c in ctors:
        dup[c["name"]] = dup.get(c["name"], 0) + 1
    for c in ctors:
        dup[c["crc"]] = dup.get(c["crc"], 0) + 1
    for k, v in dup.items():
        assert v == 1, f"duplicate ctor {k}"

    ctors.sort(key=lambda c: (tlist.index(c["ret"]), c["crc"]))
    for i, c in enumerate(ctors):
        c["index"] = i

    with open(DST, "w") as f:
        f.write("// GENERATED by scripts/gen_schema.py — do not edit.\n")
        f.write(f"// schema sha256 {hashlib.sha256(open(SRC,'rb').read()).hexdigest()[:16]}\n")
        f.write("#pragma once\n#include <cstdint>\n#include <cstddef>\n\nnamespace cppgram::schema {\n\n")
        f.write(f"inline constexpr size_t kNameCount = {len(names)};\n")
        f.write("inline constexpr const char* kNames[] = {\n")
        for n in names:
            f.write(f'    "{n}",\n')
        f.write("};\n\n")
        f.write("// Kind: 0 int, 1 long, 2 double, 3 string, 4 bytes, 5 true, 6 bool, 7 boxed, 8 vector.\n\n")
        f.write(f"inline constexpr size_t kFieldCount = {sum(len(c['fields']) for c in ctors)};\n")
        f.write("struct FieldRow { uint16_t name; uint8_t kind; uint16_t type; uint8_t elem_kind; uint16_t elem_type; uint16_t flag; };\n")
        f.write("inline constexpr FieldRow kFields[] = {\n")
        f.write("    // name, kind, boxed_type, vec_elem_kind, vec_elem_type, flagbit (0xFFFF=required)\n")
        for c in ctors:
            for (fn, kd, ty, ek, et, fl) in c["fields"]:
                f.write(f"    {{{nidx[fn]}, {kd}, {ty}, {ek}, {et}, {fl}}},  // {c['name']}.{fn}\n")
        f.write("};\n\n")
        f.write(f"inline constexpr size_t kCtorCount = {len(ctors)};\n")
        f.write("struct CtorRow { uint32_t crc; uint16_t type; uint16_t fstart; uint16_t fcount; uint16_t name; uint8_t is_fn; };\n")
        f.write("inline constexpr CtorRow kCtors[] = {\n")
        start = 0
        for c in ctors:
            f.write(f"    {{0x{c['crc']:08x}u, {tlist.index(c['ret'])}, {start}, {len(c['fields'])}, {nidx[c['name']]}, {1 if c['is_fn'] else 0}}},  // {c['name']} = {c['ret']}\n")
            start += len(c["fields"])
        f.write("};\n\n")
        f.write("// binary-searchable: ctors sorted by crc, then by name\n")
        f.write("inline constexpr uint16_t kCtorByCrc[] = {\n")
        for i in sorted(range(len(ctors)), key=lambda i: ctors[i]["crc"]):
            f.write(f"    {ctors[i]['index']},\n")
        f.write("};\n")
        f.write("inline constexpr uint16_t kCtorByName[] = {\n")
        for i in sorted(range(len(ctors)), key=lambda i: ctors[i]["name"]):
            f.write(f"    {ctors[i]['index']},\n")
        f.write("};\n\n")
        f.write("struct TypeRow { uint16_t name; uint16_t cstart; uint16_t ccount; };\n")
        f.write(f"inline constexpr size_t kTypeCount = {len(tlist)};\n")
        f.write("inline constexpr TypeRow kTypes[] = {\n")
        from collections import defaultdict
        bytype = defaultdict(list)
        for c in ctors:
            bytype[tlist.index(c["ret"])].append(c["index"])
        for ti, t in enumerate(tlist):
            idxs = bytype[ti]
            f.write(f"    {{{nidx[t]}, {idxs[0] if idxs else 0}, {len(idxs)}}},  // {t}\n")
        f.write("};\n\n")
        f.write("}  // namespace cppgram::schema\n")

    fns = sum(1 for c in ctors if c["is_fn"])
    print(f"ok: {len(ctors)} ctors ({fns} functions), {len(tlist)} types, "
          f"{sum(len(c['fields']) for c in ctors)} fields, "
          f"skipped generic: {' '.join(skip for skip in skipped_generic[:0]) or ''}"
          f"{len(skipped_generic)} generic ctors ({', '.join(skipped_generic)})")


if __name__ == "__main__":
    main()
