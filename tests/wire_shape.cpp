// Verifies the wire shape of high-traffic wrappers against the schema without
// touching the network: CG_DRY_RUN captures the encoded TL, which is decoded
// back and compared to what layer 230 actually asks for.
#include "cppgram/args.hpp"
#include "cppgram/client.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

using namespace cppgram;

namespace {

int failures = 0;

Val dry_run(Client& c, const char* method, const std::string& args_json) {
    const std::string path = std::string("/tmp/dryapi_") + method + ".bin";
    setenv("CG_DRY_RUN", path.c_str(), 1);
    std::string dbg;
    (void)c.api_json(method, args_json, &dbg);
    if (!dbg.empty()) std::printf("  args error: %s\n", dbg.c_str());
    unsetenv("CG_DRY_RUN");
    std::string bin;
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) {
        std::printf("FAIL %s: no dump produced\n", method);
        ++failures;
        return {};
    }
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, fp)) > 0) bin.append(buf, n);
    std::fclose(fp);
    std::string err;
    Val v = decode_tl(bin, &err);
    if (!err.empty() || v.k != Val::OBJ) {
        std::printf("FAIL %s: %s\n", method, err.empty() ? "not a box" : err.c_str());
        ++failures;
        return {};
    }
    return v;
}

std::string field_json(const Val& box, const char* name) {
    auto it = box.o.find(name);
    return it == box.o.end() ? std::string("<missing>") : to_json(it->second);
}

}  // namespace

int main() {
    Client client{Session{}, ClientConfig{}};

    // edit: messages.editMessage#b106e66c peer, id, message, no_webpage
    {
        Val v = dry_run(client, "messages.editMessage",
                        "{\"_ctor\":\"editMessage\",\"peer\":{\"_ctor\":\"inputPeerUser\""
                        ",\"user_id\":8663044027},\"id\":990001,\"message\":\"edited\""
                        ",\"no_webpage\":true}");
        if (v.name != "messages.editMessage") {
            std::printf("FAIL edit: top ctor %s\n", v.name.c_str());
            ++failures;
        } else {
            const std::string got = field_json(v, "message");
            std::printf("edit:  %s\n", to_json(v).c_str());
            if (got != "\"edited\"") {
                std::printf("FAIL edit: message=%s\n", got.c_str());
                ++failures;
            }
            const std::string np = field_json(v, "no_webpage");
            if (np != "1") {
                std::printf("FAIL edit: no_webpage=%s\n", np.c_str());
                ++failures;
            }
        }
    }

    // media: messages.sendMedia#330e77f peer, reply_to, media, message
    // media is a nested inputMedia ctor, not a flattened set of fields
    {
        Val v = dry_run(client, "messages.sendMedia",
                        "{\"_ctor\":\"messages.sendMedia\","
                        "\"peer\":{\"_ctor\":\"inputPeerUser\",\"user_id\":8663044027},"
                        "\"message\":\"hi\",\"media\":{\"_ctor\":\"inputMediaUploadedPhoto\""
                        ",\"file\":{\"_ctor\":\"inputFile\",\"id\":1,\"parts\":1,"
                        "\"name\":\"x.png\",\"md5_checksum\":\"\"}}}");
        if (v.name != "messages.sendMedia") {
            std::printf("FAIL media: top ctor %s\n", v.name.c_str());
            ++failures;
        } else {
            auto it = v.o.find("media");
            if (it == v.o.end() || it->second.k != Val::OBJ) {
                std::printf("FAIL media: media not nested: %s\n",
                            field_json(v, "media").c_str());
                ++failures;
            } else {
                const Val& m = it->second;
                std::printf("media: %s\n", to_json(v).c_str());
                if (m.name != "inputMediaUploadedPhoto") {
                    std::printf("FAIL media: nested ctor %s\n", m.name.c_str());
                    ++failures;
                }
                if (m.o.find("file") == m.o.end()) {
                    std::printf("FAIL media: file field lost inside nested box\n");
                    ++failures;
                }
            }
        }
    }

    // login: auth.sendCode#a677244f phone_number, api_id, api_hash, settings
    {
        Val v = dry_run(client, "auth.sendCode",
                        "{\"_ctor\":\"auth.sendCode\",\"phone_number\":\"+1999000111\","
                        "\"api_id\":6,\"api_hash\":\"x\","
                        "\"settings\":{\"_ctor\":\"codeSettings\",\"allow_flashcall\":true}}");
        if (v.name != "auth.sendCode") {
            std::printf("FAIL login: top ctor %s\n", v.name.c_str());
            ++failures;
        } else {
            std::printf("login: %s\n", to_json(v).c_str());
            if (field_json(v, "phone_number") != "\"+1999000111\"") {
                std::printf("FAIL login: phone=%s\n", field_json(v, "phone_number").c_str());
                ++failures;
            }
        }
    }

    if (failures == 0) std::printf("wire_shape: all shapes correct\n");
    return failures != 0;
}
