// MTProto 2.0 auth-key generation (DH handshake) + SRP 2FA helper.
// Wire protocol verified against core.telegram.org/mtproto/auth_key +
// samples-auth_key and ported from kurigram (session/auth.py, crypto/rsa.py,
// crypto/prime.py, utils/crypto.py).
#include "cppgram/client.hpp"
#include "cppgram/tl.hpp"
#include "cppgram/utils.hpp"

#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <random>
#include <stdexcept>

namespace cppgram {

namespace {

// ------------------------------- crypto primitives

std::string sha1_h(const std::string& data) {
    std::string out(20, '\0');
    SHA1(reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         reinterpret_cast<unsigned char*>(out.data()));
    return out;
}

std::string sha256_h(const std::string& data) {
    std::string out(32, '\0');
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, data.data(), data.size());
    unsigned int len = 0;
    EVP_DigestFinal_ex(ctx, reinterpret_cast<unsigned char*>(out.data()), &len);
    EVP_MD_CTX_free(ctx);
    return out;
}

std::string rnd_bytes(size_t n) {
    static thread_local std::mt19937_64 rng(std::random_device{}() ^
                                            static_cast<uint64_t>(::getpid()));
    std::string out(n, '\0');
    auto* p = reinterpret_cast<uint64_t*>(out.data());
    size_t i = 0;
    for (; i + 8 <= n; i += 8) p[i / 8] = rng();
    if (i < n) {
        uint64_t v = rng();
        std::memcpy(out.data() + i, &v, n - i);
    }
    return out;
}

std::string xor_bytes(const std::string& a, const std::string& b) {
    std::string out(a.size(), '\0');
    for (size_t i = 0; i < a.size(); i++) out[i] = static_cast<char>(a[i] ^ b[i]);
    return out;
}

std::string b64url_encode(const std::string& in) {
    static const char* alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string out;
    int val = 0, bits = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        bits += 8;
        while (bits >= 0) {
            out.push_back(alpha[(val >> bits) & 0x3f]);
            bits -= 6;
        }
    }
    if (bits > -6) out.push_back(alpha[((val << 8) >> (bits + 8)) & 0x3f]);
    return out;
}

// 2048-bit MODP safe prime (RFC 3526 group 14) == Telegram dh_prime; byte
// identical to samples-auth_key and kurigram CURRENT_DH_PRIME.
const char* kDhPrimeHex =
    "C71CAEB9C6B1C9048E6C522F70F13F73980D40238E3E21C14934D037563D930F"
    "48198A0AA7C14058229493D22530F4DBFA336F6E0AC925139543AED44CCE7C37"
    "20FD51F69458705AC68CD4FE6B6B13ABDC9746512969328454F18FAF8C595F64"
    "2477FE96BB2A941D5BCD1D4AC8CC49880708FA9B378E3C4F3A9060BEE67CF9A4"
    "A4A695811051907E162753B56B0F6B410DBA74D8A84B2A14B3144E0EF1284754"
    "FD17ED950D5965B4B9DD46582DB1178D169C6BC465B0D6FF9CA3928FEF5B9AE4"
    "E418FC15E83EBEA0F87FA9FF5EED70050DED2849F47BF959D956850CE929851F"
    "0D8115F635B105EE2E4E15D04B2454BF6F4FADF034B10403119CD8E3B92FCC5B";

struct RsaKey {
    uint64_t fp;
    const char* mod_hex;
};
// Telegram server RSA public keys (kurigram/crypto/rsa.py); first five are the
// production DC keys, the rest are the CDN keys.
const RsaKey kRsaKeys[] = {
    {0xC3B42B026CE86B21ull,
     "C150023E2F70DB7985DED064759CFECF0AF328E69A41DAF4D6F01B538135A6F9"
     "1F8F8B2A0EC9BA9720CE352EFCF6C5680FFC424BD634864902DE0B4BD6D49F4E"
     "580230E3AE97D95C8B19442B3C0A10D8F5633FECEDD6926A7F6DAB0DDB7D457F"
     "9EA81B8465FCD6FFFEED114011DF91C059CAEDAF97625F6C96ECC74725556934"
     "EF781D866B34F011FCE4D835A090196E9A5F0E4449AF7EB697DDB9076494CA5F"
     "81104A305B6DD27665722C46B60E5DF680FB16B210607EF217652E60236C255F"
     "6A28315F4083A96791D7214BF64C1DF4FD0DB1944FB26A2A57031B32EEE64AD1"
     "5A8BA68885CDE74A5BFC920F6ABF59BA5C75506373E7130F9042DA922179251F"},
    {0x0BC35F3509F7B7A5ull,
     "AEEC36C8FFC109CB099624685B97815415657BD76D8C9C3E398103D7AD16C9BB"
     "A6F525ED0412D7AE2C2DE2B44E77D72CBF4B7438709A4E646A05C43427C7F184"
     "DEBF72947519680E651500890C6832796DD11F772C25FF8F576755AFE055B0A3"
     "752C696EB7D8DA0D8BE1FAF38C9BDD97CE0A77D3916230C4032167100EDD0F9E"
     "7A3A9B602D04367B689536AF0D64B613CCBA7962939D3B57682BEB6DAE5B6081"
     "30B2E52ACA78BA023CF6CE806B1DC49C72CF928A7199D22E3D7AC84E47BC9427"
     "D0236945D10DBD15177BAB413FBF0EDFDA09F014C7A7DA088DDE9759702CA760"
     "AF2B8E4E97CC055C617BD74C3D97008635B98DC4D621B4891DA9FB0473047927"},
    {0x015AE5FA8B5529542ull,
     "BDF2C77D81F6AFD47BD30F29AC76E55ADFE70E487E5E48297E5A9055C9C07D2B"
     "93B4ED3994D3ECA5098BF18D978D54F8B7C713EB10247607E69AF9EF44F38E28"
     "F8B439F257A11572945CC0406FE3F37BB92B79112DB69EEDF2DC71584A661638"
     "EA5BECB9E23585074B80D57D9F5710DD30D2DA940E0ADA2F1B878397DC1A72B5"
     "CE2531B6F7DD158E09C828D03450CA0FF8A174DEACEBCAA22DDE84EF66AD370F"
     "259D18AF806638012DA0CA4A70BAA83D9C158F3552BC9158E69BF332A45809E1"
     "C36905A5CAA12348DD57941A482131BE7B2355A5F4635374F3BD3DDF5FF925BF"
     "4809EE27C1E67D9120C5FE08A9DE458B1B4A3C5D0A428437F2BECA81F4E2D5FF"},
    {0xAEAE98E13CD7F94Full,
     "B3F762B739BE98F343EB1921CF0148CFA27FF7AF02B6471213FED9DAA0098976"
     "E667750324F1ABCEA4C31E43B7D11F1579133F2B3D9FE27474E462058884E5E1"
     "B123BE9CBBC6A443B2925C08520E7325E6F1A6D50E117EB61EA49D2534C8BB4D"
     "2AE4153FABE832B9EDF4C5755FDD8B19940B81D1D96CF433D19E6A22968A85DC"
     "80F0312F596BD2530C1CFB28B5FE019AC9BC25CD9C2A5D8A0F3A1C0C79BCCA52"
     "4D315B5E21B5C26B46BABE3D75D06D1CD33329EC782A0F22891ED1DB42A1D6C0"
     "DEA431428BC4D7AABDCF3E0EB6FDA4E23EB7733E7727E9A1915580796C55188D"
     "2596D2665AD1182BA7ABF15AAA5A8B779EA996317A20AE044B820BFF35B6E8A1"},
    {0x05A181B2235057D98ull,
     "BE6A71558EE577FF03023CFA17AAB4E6C86383CFF8A7AD38EDB9FAFE6F323F2D"
     "5106CBC8CAFB83B869CFFD1CCF121CD743D509E589E68765C96601E813DC5B9D"
     "FC4BE415C7A6526132D0035CA33D6D6075D4F535122A1CDFE017041F1088D141"
     "9F65C8E5490EE613E16DBF662698C0F54870F0475FA893FC41EB55B08FF1AC21"
     "1BC045DED31BE27D12C96D8D3CFC6A7AE8AA50BF2EE0F30ED507CC258E3DEC5"
     "6DE94F5DC0A7ABEE0BE990B893F2887BD2C6310A1E0A9E3E38BD34FDED254150"
     "8DC102A9C9B4C95EFFD9DD2DFE96C29BE647D6C69D66CA500843CFAED6E44019"
     "6F1DBE0E2E22163C61CA48C79116FA77216726749A976A1C4B0944B5121E8C01"},
    {0x995EFFD323B5DB80ull,
     "E2D587706265125931BB129027016325ABA59951E0771340DF0808D84F176CD9"
     "C0F3CCB9D57A205799BC04462E4D23F89655D9638652256E559455E79AC253FE"
     "19A3C9E16AA936A3C805BA9DBF6FC195F6F7542FA83FF5642141CBB4CBBC2334"
     "95B34812170B31FDF7655F007AD2C077EB912C5E901E8B32B39C34889EF1F946"
     "C6C03C727DE89CF042499EC29A5EB50EC11C9C31137E57FCC3C926B4118975BB"
     "5101D4C0D7D7F8C857521011B2EF7411F8BEC55354475628797339C1734FDA97"
     "FE0F1789B08F8872C9BEA41BF215626EFB3D35A00DC8295C044B7798EB9C1B5F"
     "F8E8B8A591CFCA7789E509CA219348C81CDA7CB32994CCD5913D41A64ECDD23F"},
    {0x0254672538E935938ull,
     "CEE1D50BBB04E742A1A3FC83559B569E5980E417FF68CF0A658DD6CD2D7AC3AC"
     "35B01AA2A63F2880C186ED42DB181B5898A11A23B20824EE963369B531A5D59E"
     "CA92F1DECF6860198B2F2B48DDD2ED2D9AF30A7845765E86CD09017BD9788CF8"
     "E6207208C05FC9C6C92A64B079891EB11508EE150EF1E4219A6FD4614129258E"
     "D53ADD087A68AE5114A9AA5450D8595CC876A161435CBDB2026F8FBF00FEDCA0"
     "A067E9C079172CCCECC09C2B16C428EC776373149DB66AAB9A4DEBF7916B391E"
     "832AE5A7892E27DE0AB1B4451C55F90F1F2ECE3ACEF708BC2C5EE022066EE434"
     "4C7268D724AABAAC667667D727AC3F2956ED4BDAF7089DDE0AEB18A6652DA16F"},
    {0xC884B3E62D09E5C5ull,
     "BA0E9F11355BECD917618BB9B9E66E334A6AC3585EBAA52B64D628990374952D"
     "28EE2EA6016C1BD7F162B1C14AFB2E0B412484C7EA14CD70218E6965F5F99BE1"
     "EECF6D4897A3C32E5CED118F77B9E35E835DF4E23981965E5151574690CEEF1A"
     "832D5EA4AC2414389D9E6CBB839AC9255D2AD7EDDE47E264D8E11CFC1EA53003"
     "AFDDF054EB1844641C8DF60A9CE9FBE4368D773D0F096195121ADB732E5632EF"
     "AB859AF141851B0953E2CD436F9BB131F069F49F7D56256766E824E6E5BEA6D1"
     "59EE39DC2CECB1A496CB0197390996BD56C47BB17CAB5922FECED2F01C585AB8"
     "522C9E9BB2EF556569C81C3F6F077E1017C20004F444F779A108E1C1A89C5205"},
    {0xBB27580FD5B01626ull,
     "BFF2FAB5DFA68FB0E5F353477EEF977F528DB6F64F475B52E7116A92252CA27D"
     "6EEC3DAE94AD398CCF072AFA55D68FB51CF3EA85A5E16ACBEC5FDD2EDD320745"
     "0D33B89289C4A022668DC62BBA611E8CE3009F555056CEEE32806B905313DB57"
     "800E59C090224166F753ACA4C11CE4DDF1517B959A5BBC5F55489EA71C9619DA"
     "F579E7285827C169FD530977DC4BBBDCCA58CD4F4DE9C2DD98C92E625C4E4807"
     "9C01079CAA4929B8198B56591EBB52322C5C177BED34697E82C6A5F39D507C41"
     "CF86528FC0FBEA6D73969203E49DA917A6C7E3E642CF4494A6FDE3EDB13C5859"
     "6F80C7F010F870C4C62275119ACAAF5894B9587780FE628AB453A8D2DCC59345"},
};

const RsaKey* find_rsa_key(uint64_t fp) {
    for (const RsaKey& k : kRsaKeys) {
        if (k.fp == fp) return &k;
    }
    return nullptr;
}

struct BnGuard {
    BIGNUM* b;
    BnGuard(BIGNUM* b) : b(b) {}
    ~BnGuard() { BN_free(b); }
    operator BIGNUM*() { return b; }
};

// BN_CTX wrapper: BN_mod_exp(..., NULL) segfaults on this toolchain's OpenSSL
// 3.0.22, so every context-taking BN_* op (mod_exp, mul, add, mod, ...) must
// run inside an explicit context. NULL is not a valid "no scratch space".
struct CtxGuard {
    BN_CTX* c;
    CtxGuard() : c(BN_CTX_new()) {}
    ~CtxGuard() { BN_CTX_free(c); }
    operator BN_CTX*() { return c; }
};

BnGuard bn_hex_str(const std::string& hex) {
    BIGNUM* b = nullptr;
    BN_hex2bn(&b, hex.c_str());
    return BnGuard(b);
}
BnGuard bn_hex(const char* hex) { return bn_hex_str(std::string(hex)); }

BnGuard bn_be(const std::string& be) {
    static const char* hx = "0123456789ABCDEF";
    std::string hex;
    for (unsigned char c : be) {
        hex += hx[c >> 4];
        hex += hx[c & 15];
    }
    return bn_hex_str(hex);
}

std::string bn_to_be(const BIGNUM* n, size_t width) {
    std::string out(width, '\0');
    BN_bn2binpad(n, reinterpret_cast<unsigned char*>(out.data()), static_cast<int>(width));
    return out;
}

// ------------------------------- TL (de)serialization for handshake bodies

struct W {
    std::string out;
    void i32(int32_t v) { out.append(reinterpret_cast<const char*>(&v), 4); }
    void i64(int64_t v) { out.append(reinterpret_cast<const char*>(&v), 8); }
    void u32(uint32_t v) { out.append(reinterpret_cast<const char*>(&v), 4); }
    void raw(const std::string& s) { out += s; }
    void bytes(const std::string& s) {  // TL string (abridged length prefixes)
        size_t n = s.size();
        if (n < 254) {
            out.push_back(static_cast<char>(n));
        } else if (n < (1u << 24)) {
            out.push_back(static_cast<char>(254));
            out.push_back(static_cast<char>(n & 0xff));
            out.push_back(static_cast<char>((n >> 8) & 0xff));
            out.push_back(static_cast<char>((n >> 16) & 0xff));
        } else {
            throw std::runtime_error("tl string too long");
        }
        out += s;
        while (out.size() % 4 != 0) out.push_back('\0');
    }
};

struct R {
    const std::string& s;
    size_t i = 0;
    explicit R(const std::string& s) : s(s) {}
    bool ok() const { return i <= s.size(); }
    size_t consumed() const { return i; }
    uint32_t u32() {
        if (i + 4 > s.size()) throw std::runtime_error("short tl read");
        uint32_t v;
        std::memcpy(&v, s.data() + i, 4);
        i += 4;
        return v;
    }
    int32_t i32() { return static_cast<int32_t>(u32()); }
    int64_t i64() {
        if (i + 8 > s.size()) throw std::runtime_error("short tl read");
        int64_t v;
        std::memcpy(&v, s.data() + i, 8);
        i += 8;
        return v;
    }
    std::string raw(size_t n) {
        if (i + n > s.size()) throw std::runtime_error("short tl read");
        std::string v = s.substr(i, n);
        i += n;
        return v;
    }
    std::string bytes() {
        if (i >= s.size()) throw std::runtime_error("short tl string");
        unsigned char n = static_cast<unsigned char>(s[i++]);
        if (n < 254) {
            std::string v = raw(n);
            while (i % 4 != 0) i++;
            return v;
        }
        if (n == 254) {
            if (i + 3 > s.size()) throw std::runtime_error("short tl long string");
            size_t len = static_cast<unsigned char>(s[i]) | (static_cast<unsigned char>(s[i + 1]) << 8) |
                         (static_cast<unsigned char>(s[i + 2]) << 16);
            i += 3;
            std::string v = raw(len);
            while (i % 4 != 0) i++;
            return v;
        }
        throw std::runtime_error("tl string 0xff reserved");
    }
};

// ------------------------------- plaintext abridged socket

int tcp_connect(const std::string& ip, int port, std::string* err) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (::getaddrinfo(ip.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 || !res) {
        if (err) *err = "keygen: dns failed for " + ip;
        return -1;
    }
    int fd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        ::freeaddrinfo(res);
        if (err) *err = "keygen: socket() failed";
        return -1;
    }
    timeval tv{};
    tv.tv_sec = 20;
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    if (::connect(fd, res->ai_addr, res->ai_addrlen) != 0) {
        ::close(fd);
        ::freeaddrinfo(res);
        if (err) *err = "keygen: tcp connect failed to " + ip;
        return -1;
    }
    ::freeaddrinfo(res);
    const char magic = static_cast<char>(0xef);
    if (::send(fd, &magic, 1, MSG_NOSIGNAL) != 1) {
        ::close(fd);
        if (err) *err = "keygen: transport announcement failed";
        return -1;
    }
    return fd;
}

class PlainSock {
public:
    bool open(const std::string& ip, int port, std::string* err) {
        fd_ = tcp_connect(ip, port, err);
        return fd_ >= 0;
    }
    void close() {
        if (fd_ >= 0) ::close(fd_);
        fd_ = -1;
    }
    ~PlainSock() { close(); }

    bool send_body(const std::string& body, std::string* err) {
        W w;
        w.i64(0);              // auth_key_id (plaintext)
        w.i64(next_msg_id());  // message_id: (time<<32) + N*4
        w.i32(static_cast<int32_t>(body.size()));
        w.raw(body);
        std::string framed;
        size_t words = w.out.size() / 4;
        if (words < 0x7f) {
            framed.push_back(static_cast<char>(words));
        } else {
            framed.push_back(static_cast<char>(0x7f));
            framed.push_back(static_cast<char>(words & 0xff));
            framed.push_back(static_cast<char>((words >> 8) & 0xff));
            framed.push_back(static_cast<char>((words >> 16) & 0xff));
        }
        framed += w.out;
        size_t off = 0;
        while (off < framed.size()) {
            ssize_t n = ::send(fd_, framed.data() + off, framed.size() - off, MSG_NOSIGNAL);
            if (n <= 0) {
                if (err) *err = "keygen: send failed";
                return false;
            }
            off += static_cast<size_t>(n);
        }
        return true;
    }

    bool recv_body(std::string* body, std::string* err) {
        std::string hdr;
        if (!read_exact(&hdr, 1, err)) return false;
        size_t len;
        if (static_cast<unsigned char>(hdr[0]) < 0x7f) {
            len = static_cast<size_t>(static_cast<unsigned char>(hdr[0])) * 4;
        } else {
            std::string ext;
            if (!read_exact(&ext, 3, err)) return false;
            len = (static_cast<size_t>(static_cast<unsigned char>(ext[0])) |
                   (static_cast<size_t>(static_cast<unsigned char>(ext[1])) << 8) |
                   (static_cast<size_t>(static_cast<unsigned char>(ext[2])) << 16)) *
                  4;
        }
        std::string msg;
        if (!read_exact(&msg, len, err)) return false;
        if (msg.size() < 20) {
            if (err) *err = "keygen: short plaintext message";
            return false;
        }
        *body = msg.substr(20);  // skip auth_key_id(8) + msg_id(8) + length(4)
        return true;
    }

private:
    int fd_{-1};
    std::string buf_;  // socket bytes already read but not yet consumed

    static int64_t next_msg_id() {
        timeval tv{};
        ::gettimeofday(&tv, nullptr);
        static int64_t counter = 0;
        return (static_cast<int64_t>(tv.tv_sec) << 32) + (++counter) * 4;
    }

    bool fill(std::string* err) {
        pollfd p{fd_, POLLIN, 0};
        if (::poll(&p, 1, 20000) <= 0 || !(p.revents & POLLIN)) {
            if (err) *err = "keygen: read timeout";
            return false;
        }
        char tmp[16384];
        ssize_t r = ::recv(fd_, tmp, sizeof(tmp), 0);
        if (r <= 0) {
            if (err) *err = "keygen: recv failed";
            return false;
        }
        buf_.append(tmp, static_cast<size_t>(r));
        return true;
    }

    bool read_exact(std::string* out, size_t n, std::string* err) {
        out->clear();
        while (buf_.size() < n) {
            if (!fill(err)) return false;
        }
        out->assign(buf_, 0, n);
        buf_.erase(0, n);
        return true;
    }
};

// AES-256-IGE for the DH answer/client data (same shape as Client transport).
struct Ige {
    static bool ecb(const unsigned char* k, bool enc, const unsigned char* in, unsigned char* out) {
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        int olen = 0, tlen = 0;
        EVP_CipherInit_ex(ctx, EVP_aes_256_ecb(), nullptr, k, nullptr, enc ? 1 : 0);
        EVP_CIPHER_CTX_set_padding(ctx, 0);
        bool ok = EVP_CipherUpdate(ctx, out, &olen, in, 16) == 1;
        if (ok) ok = EVP_CipherFinal_ex(ctx, out + olen, &tlen) == 1 && olen + tlen == 16;
        EVP_CIPHER_CTX_free(ctx);
        return ok;
    }
    static bool run(const std::string& key32, const std::string& iv32, const std::string& in,
                    std::string& out, bool enc) {
        if (in.empty() || in.size() % 16 != 0) return false;
        out.assign(in.size(), '\0');
        std::array<unsigned char, 16> p_prev{}, c_prev{};
        std::memcpy(c_prev.data(), iv32.data(), 16);
        std::memcpy(p_prev.data(), iv32.data() + 16, 16);
        std::array<unsigned char, 16> block{};
        for (size_t i = 0; i < in.size(); i += 16) {
            if (enc) {
                for (int j = 0; j < 16; j++) block[j] = static_cast<unsigned char>(in[i + j]) ^ c_prev[j];
                if (!ecb(reinterpret_cast<const unsigned char*>(key32.data()), true, block.data(),
                         block.data()))
                    return false;
                for (int j = 0; j < 16; j++) {
                    unsigned char o = block[j] ^ p_prev[j];
                    out[i + j] = static_cast<char>(o);
                    c_prev[j] = o;
                    p_prev[j] = static_cast<unsigned char>(in[i + j]);
                }
            } else {
                std::array<unsigned char, 16> cin{};
                std::memcpy(cin.data(), in.data() + i, 16);
                for (int j = 0; j < 16; j++) block[j] = cin[j] ^ p_prev[j];
                if (!ecb(reinterpret_cast<const unsigned char*>(key32.data()), false, block.data(),
                         block.data()))
                    return false;
                for (int j = 0; j < 16; j++) {
                    unsigned char o = block[j] ^ c_prev[j];
                    out[i + j] = static_cast<char>(o);
                    p_prev[j] = o;
                    c_prev[j] = cin[j];
                }
            }
        }
        return true;
    }
};

// Pollard's rho (Brent) 64-bit factorization (kurigram/crypto/prime.py).
uint64_t decompose(uint64_t pq) {
    if (pq < 2) return pq;  // nothing to factor; also guards the pq-1 arithmetic below
    if (pq % 2 == 0) return 2;
    static thread_local std::mt19937_64 rng(std::random_device{}());
    uint64_t y = rng() % (pq - 1) + 1, c = rng() % (pq - 1) + 1, m = rng() % (pq - 1) + 1;
    uint64_t g = 1, r = 1, q = 1, x = 0, ys = 0;
    while (g == 1) {
        x = y;
        for (uint64_t i = 0; i < r; i++) y = (static_cast<unsigned __int128>(y) * y + c) % pq;
        uint64_t k = 0;
        while (k < r && g == 1) {
            ys = y;
            uint64_t lim = std::min(m, r - k);
            for (uint64_t i = 0; i < lim; i++) {
                y = (static_cast<unsigned __int128>(y) * y + c) % pq;
                uint64_t diff = x > y ? x - y : y - x;
                q = static_cast<uint64_t>((static_cast<unsigned __int128>(q) * diff) % pq);
            }
            g = std::gcd(q, pq);
            k += m;
        }
        r *= 2;
    }
    if (g == pq) {
        while (true) {
            ys = (static_cast<unsigned __int128>(ys) * ys + c) % pq;
            uint64_t diff = x > ys ? x - ys : ys - x;
            g = std::gcd(diff, pq);
            if (g > 1) break;
        }
    }
    return g;
}

void be4(uint64_t v, char out[4]) {
    for (int i = 0; i < 4; i++) out[i] = static_cast<char>((v >> (24 - 8 * i)) & 0xff);
}

}  // namespace

// ------------------------------- public: auth-key generation

bool generate_auth_key(const std::string& ip, int port, int dc_id, std::string* out_key,
                       std::string* err) {
    CtxGuard ctx;
    PlainSock sock;
    if (!sock.open(ip, port, err)) return false;

    const std::string nonce = rnd_bytes(16);
    {
        W w;
        w.u32(0xbe7e8ef1u);  // req_pq_multi#be7e8ef1
        w.raw(nonce);
        if (!sock.send_body(w.out, err)) return false;
    }

    std::string resp;
    if (!sock.recv_body(&resp, err)) return false;
    std::string server_nonce, pq;
    int64_t matched_fp = 0;
    {
        R r(resp);
        if (r.u32() != 0x05162463u) {  // resPQ#05162463
            if (err) *err = "keygen: expected resPQ";
            return false;
        }
        std::string got_nonce = r.raw(16);
        if (got_nonce != nonce) {
            if (err) *err = "keygen: resPQ nonce mismatch";
            return false;
        }
        server_nonce = r.raw(16);
        pq = r.bytes();
        if (r.u32() != 0x1cb5c415u) {  // Vector
            if (err) *err = "keygen: resPQ fingerprints tag";
            return false;
        }
        int32_t count = r.i32();
        for (int32_t i = 0; i < count; i++) {
            int64_t fp = r.i64();
            if (!matched_fp && find_rsa_key(static_cast<uint64_t>(fp))) matched_fp = fp;
        }
        if (!r.ok() || !matched_fp) {
            if (err) *err = "keygen: no known RSA fingerprint in resPQ";
            return false;
        }
    }
    if (pq.size() != 8) {
        if (err) *err = "keygen: pq is " + std::to_string(pq.size()) + " bytes";
        return false;
    }
    const RsaKey* key = find_rsa_key(static_cast<uint64_t>(matched_fp));

    uint64_t p_num = 0;
    {
        uint64_t pq_num = 0;
        for (unsigned char ch : pq) pq_num = (pq_num << 8) | ch;
        for (int attempt = 0; attempt < 24; attempt++) {
            uint64_t g = decompose(pq_num);
            if (g > 1 && g < pq_num && pq_num % g == 0) {
                p_num = g;
                break;
            }
        }
        if (!p_num) {
            if (err) *err = "keygen: pq factorization failed";
            return false;
        }
    }
    uint64_t q_num = 0;
    {
        uint64_t pq_num = 0;
        for (unsigned char ch : pq) pq_num = (pq_num << 8) | ch;
        q_num = pq_num / p_num;
        if (p_num > q_num) std::swap(p_num, q_num);
    }

    const std::string new_nonce = rnd_bytes(32);

    // p_q_inner_data_dc#a9f55f95 -> sha1+data (multiple of 255) -> RSA-2048
    {
        W w;
        w.u32(0xa9f55f95u);
        w.bytes(pq);
        char p4[4], q4[4];
        be4(p_num, p4);
        be4(q_num, q4);
        w.bytes(std::string(p4, 4));
        w.bytes(std::string(q4, 4));
        w.raw(nonce);
        w.raw(server_nonce);
        w.raw(new_nonce);
        w.i32(dc_id);
        std::string with_hash = sha1_h(w.out) + w.out;
        size_t pad = (255 - (with_hash.size() % 255)) % 255;
        with_hash += rnd_bytes(pad);
        if (with_hash.size() > 255) {
            if (err) *err = "keygen: p_q_inner_data too large";
            return false;
        }
        BnGuard n = bn_hex(key->mod_hex);
        BnGuard m = bn_be(with_hash);
        BnGuard e = bn_hex("010001");
        BnGuard c = BnGuard(BN_new());
        if (!n.b || !m.b || !e.b || !c.b || BN_mod_exp(c, m, e, n, ctx) != 1) {
            if (err) *err = "keygen: rsa encrypt failed";
            return false;
        }
        W r;
        r.u32(0xd712e4beu);  // req_DH_params#d712e4be
        r.raw(nonce);
        r.raw(server_nonce);
        r.bytes(std::string(p4, 4));
        r.bytes(std::string(q4, 4));
        r.i64(static_cast<int64_t>(key->fp));
        r.bytes(bn_to_be(c, 256));
        if (!sock.send_body(r.out, err)) return false;
    }

    if (!sock.recv_body(&resp, err)) return false;
    std::string encrypted_answer;
    {
        R r(resp);
        uint32_t crc = r.u32();
        if (crc == 0x79cb045du) {  // server_DH_params_fail#79cb045d
            if (err) *err = "keygen: server_DH_params_fail";
            return false;
        }
        if (crc != 0xd0e8075cu) {  // server_DH_params_ok#d0e8075c
            if (err) *err = "keygen: expected server_DH_params_ok";
            return false;
        }
        std::string n1 = r.raw(16), sn1 = r.raw(16);
        if (n1 != nonce || sn1 != server_nonce) {
            if (err) *err = "keygen: server_DH_params_ok nonce mismatch";
            return false;
        }
        encrypted_answer = r.bytes();
    }
    if (encrypted_answer.empty() || encrypted_answer.size() % 16 != 0) {
        if (err) *err = "keygen: bad encrypted_answer length";
        return false;
    }

    const std::string tmp_aes_key = sha1_h(new_nonce + server_nonce) +
                                    sha1_h(server_nonce + new_nonce).substr(0, 12);
    const std::string tmp_aes_iv = sha1_h(server_nonce + new_nonce).substr(12, 8) +
                                   sha1_h(new_nonce + new_nonce) + new_nonce.substr(0, 4);

    std::string answer;
    if (!Ige::run(tmp_aes_key, tmp_aes_iv, encrypted_answer, answer, false)) {
        if (err) *err = "keygen: IGE decrypt failed";
        return false;
    }
    if (answer.size() < 20) {
        if (err) *err = "keygen: dh answer too short";
        return false;
    }
    const std::string sdh = answer.substr(20);

    int32_t g = 0;
    std::string g_a;
    {
        R r(sdh);
        if (r.u32() != 0xb5890dbau) {  // server_DH_inner_data#b5890dba
            if (err) *err = "keygen: bad server_DH_inner_data";
            return false;
        }
        if (r.raw(16) != nonce || r.raw(16) != server_nonce) {
            if (err) *err = "keygen: dh inner nonce mismatch";
            return false;
        }
        g = r.i32();
        std::string dh_prime = r.bytes();
        g_a = r.bytes();
        r.i32();  // server_time
        if (dh_prime != bn_to_be(bn_hex(kDhPrimeHex), 256)) {
            if (err) *err = "keygen: dh_prime is not the known prime";
            return false;
        }
        // The hash covers the TL structure without the trailing zero padding,
        // so compare against the bytes actually consumed by the parser.
        std::string unpadded(sdh, 0, r.consumed());
        if (sha1_h(unpadded) != answer.substr(0, 20)) {
            if (err) *err = "keygen: dh answer hash mismatch";
            return false;
        }
    }
    if (g_a.size() != 256) {
        if (err) *err = "keygen: g_a length mismatch";
        return false;
    }
    if (g <= 1 || g > 16) {
        if (err) *err = "keygen: bad g";
        return false;
    }

    BnGuard prime = bn_hex(kDhPrimeHex);
    BnGuard g_a_bi = bn_be(g_a);
    BnGuard low = BnGuard(BN_new());
    BN_one(low.b);
    BN_lshift(low.b, low.b, 2048 - 64);
    BnGuard high = BN_new();
    {
        BnGuard gap = BnGuard(BN_new());
        BN_one(gap.b);
        BN_lshift(gap.b, gap.b, 2048 - 64);
        BN_sub(high.b, prime.b, gap.b);
    }
    if (BN_cmp(g_a_bi.b, low.b) <= 0 || BN_cmp(g_a_bi.b, high.b) >= 0) {
        if (err) *err = "keygen: g_a out of range";
        return false;
    }

    BnGuard b = BnGuard(BN_new());
    if (BN_rand(b.b, 2048, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY) != 1) {
        if (err) *err = "keygen: BN_rand failed";
        return false;
    }
    BnGuard g_bi = bn_hex(std::to_string(g).c_str());
    BnGuard g_b = BN_new();
    if (BN_mod_exp(g_b.b, g_bi.b, b.b, prime.b, ctx) != 1) {
        if (err) *err = "keygen: g_b pow failed";
        return false;
    }

    {
        W w;
        w.u32(0x6643b654u);  // client_DH_inner_data#6643b654
        w.raw(nonce);
        w.raw(server_nonce);
        w.i64(0);  // retry_id
        w.bytes(bn_to_be(g_b, 256));
        std::string with_hash = sha1_h(w.out) + w.out;
        size_t pad = (16 - (with_hash.size() % 16)) % 16;
        with_hash += rnd_bytes(pad);
        std::string enc;
        if (!Ige::run(tmp_aes_key, tmp_aes_iv, with_hash, enc, true)) {
            if (err) *err = "keygen: IGE encrypt failed";
            return false;
        }
        W r;
        r.u32(0xf5045f1fu);  // set_client_DH_params#f5045f1f
        r.raw(nonce);
        r.raw(server_nonce);
        r.bytes(enc);
        if (!sock.send_body(r.out, err)) return false;
    }

    if (!sock.recv_body(&resp, err)) return false;
    {
        R r(resp);
        uint32_t crc = r.u32();
        if (crc == 0x3bcbf734u) {  // dh_gen_ok#3bcbf734
            std::string n1 = r.raw(16), sn1 = r.raw(16), h1 = r.raw(16);
            if (n1 != nonce || sn1 != server_nonce) {
                if (err) *err = "keygen: dh_gen_ok nonce mismatch";
                return false;
            }
            // new_nonce_hash1 = sha1(new_nonce + 0x01 + sha1(auth_key)[0:8])[4:20]
            BnGuard ak = BN_new();
            if (BN_mod_exp(ak.b, g_a_bi.b, b.b, prime.b, ctx) != 1) {
                if (err) *err = "keygen: auth_key pow failed";
                return false;
            }
            std::string expect =
                sha1_h(new_nonce + std::string(1, '\1') + sha1_h(bn_to_be(ak, 256)).substr(0, 8))
                    .substr(4);
            if (h1 != expect) {
                if (err) *err = "keygen: new_nonce_hash1 mismatch";
                return false;
            }
        } else if (crc == 0x46dc1fb9u) {  // dh_gen_retry#46dc1fb9
            if (err) *err = "keygen: dh_gen_retry (server asked to retry)";
            return false;
        } else if (crc == 0xa69dae02u) {  // dh_gen_fail#a69dae02
            if (err) *err = "keygen: dh_gen_fail";
            return false;
        } else {
            if (err) *err = "keygen: handshake not accepted (0x" +
                            [&] {
                                char b[9];
                                std::snprintf(b, sizeof(b), "%08x", crc);
                                return std::string(b);
                            }() +
                            ")";
            return false;
        }
    }

    BnGuard ak = BN_new();
    if (BN_mod_exp(ak.b, g_a_bi.b, b.b, prime.b, ctx) != 1) {
        if (err) *err = "keygen: auth_key pow failed";
        return false;
    }
    *out_key = bn_to_be(ak, 256);
    return true;
}

// ------------------------------- public: SRP 2FA

// Builds the inputCheckPasswordSRP JSON for auth.checkPassword from the
// account.getPassword response tree (kurigram utils/crypto.py algorithm).
std::string srp_password_check(const Val& password_obj, const std::string& password,
                               std::string* err) {
    CtxGuard ctx;
    const Val& algo = password_obj.at("current_algo");
    const Val& b_val = password_obj.at("srp_B");
    if (algo.k != Val::OBJ || b_val.k != Val::STR) {
        if (err) *err = "srp: account.getPassword missing current_algo/srp_B";
        return {};
    }
    const std::string& p_bytes = algo.at("p").s;
    const std::string& salt1 = algo.at("salt1").s;
    const std::string& salt2 = algo.at("salt2").s;
    int g = static_cast<int>(algo.at("g").i);
    BnGuard g_bn = bn_hex(std::to_string(g).c_str());
    BnGuard p = bn_be(p_bytes);
    BnGuard B = bn_be(b_val.s);
    if (!g_bn.b || !p.b || !B.b) {
        if (err) *err = "srp: bad dh params";
        return {};
    }
    const std::string g_bytes = bn_to_be(g_bn, 256);
    const std::string x_bytes = utils::compute_password_hash(algo, password);
    BnGuard g_x = BN_new();
    BN_mod_exp(g_x.b, g_bn.b, bn_be(x_bytes).b, p.b, ctx);
    BnGuard k = bn_be(sha256_h(p_bytes + g_bytes));
    BnGuard kg_x = BN_new();
    BN_mul(kg_x.b, k.b, g_x.b, ctx);
    BN_mod(kg_x.b, kg_x.b, p.b, ctx);

    BnGuard a = BnGuard(BN_new());
    BN_rand(a.b, 2048, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY);
    BnGuard A = BN_new();
    BN_mod_exp(A.b, g_bn.b, a.b, p.b, ctx);
    const std::string A_bytes = bn_to_be(A, 256);
    const std::string& B_bytes = b_val.s;
    BnGuard u = bn_be(sha256_h(A_bytes + B_bytes));

    BnGuard g_b = BN_new();
    BN_sub(g_b.b, B.b, kg_x.b);
    BN_mod(g_b.b, g_b.b, p.b, ctx);
    BnGuard ux = BN_new();
    BN_mul(ux.b, u.b, bn_be(x_bytes).b, ctx);
    BN_add(ux.b, ux.b, a.b);
    BnGuard S = BN_new();
    BN_mod_exp(S.b, g_b.b, ux.b, p.b, ctx);
    const std::string K = sha256_h(bn_to_be(S, 256));
    std::string M1 = sha256_h(xor_bytes(sha256_h(p_bytes), sha256_h(g_bytes)) +
                              sha256_h(salt1) + sha256_h(salt2) + A_bytes + B_bytes + K);
    return "{\"_ctor\":\"inputCheckPasswordSRP\",\"srp_id\":" +
           std::to_string(password_obj.at("srp_id").i) + ",\"A\":\"" + b64url_encode(A_bytes) +
           "\",\"M1\":\"" + b64url_encode(M1) + "\"}";
}

std::string srp_new_password_hash(const Val& password_obj, const std::string& new_password,
                                  std::string* algo_json_out, std::string* err) {
    CtxGuard ctx;
    const Val& algo_in = password_obj.at("new_algo");
    if (algo_in.k != Val::OBJ || algo_in.name.find("passwordKdfAlgo") == std::string::npos) {
        if (err)
            *err = "srp: account.getPassword missing new_algo; cloud password cannot be set";
        return {};
    }
    // kurigram appends 32 random bytes to salt1 so the stored v does not reveal the
    // password; the same salted algo must be echoed back for checkPassword to agree.
    Val algo = algo_in;
    algo.o["salt1"].s += random_bytes(32);
    const std::string& p_bytes = algo.at("p").s;
    int g = static_cast<int>(algo.at("g").i);
    BnGuard g_bn = bn_hex(std::to_string(g).c_str());
    BnGuard p = bn_be(p_bytes);
    BnGuard x = bn_be(utils::compute_password_hash(algo, new_password));
    BnGuard v = BN_new();
    if (!g_bn.b || !p.b || !x.b || !v.b) {
        if (err) *err = "srp: bad dh params for the new password";
        return {};
    }
    BN_mod_exp(v.b, g_bn.b, x.b, p.b, ctx);
    if (algo_json_out) *algo_json_out = to_json(algo);
    return b64url_encode(bn_to_be(v, 256));
}

}  // namespace cppgram
