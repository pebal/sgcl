//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/mail_auth.h"
#include "dns.h"
#include "error.h"
#include "../async/coroutine.h"
#include "../core/aliases.h"
#include "../core/duration.h"
#include "../core/make_tracked.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"
#include "../crypto/detail/key_pem.h"
#include "../crypto/ed25519.h"
#include "../crypto/rsa.h"
#include "../crypto/secret.h"
#include "../crypto/sha256.h"
#include "../encoding/base64.h"
#include "../time/datetime.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

// DomainKeys Identified Mail (RFC 6376): a domain's signature over a
// message's head and body, made with a private key whose public half the
// domain publishes in DNS, and checked by a receiver against that record.
// rsa-sha256 and ed25519-sha256 (RFC 8463), both canonicalizations, l=;
// rsa-sha1 and RSA keys under 1024 bits refused (RFC 8301)
namespace sgcl::net::dkim {
    enum class algorithm : uint8_t {
        rsa_sha256,       // RSA PKCS #1 v1.5 over SHA-256 (RFC 6376 §3.3.1)
        ed25519_sha256    // Ed25519 over SHA-256 of the head (RFC 8463)
    };

    enum class canonicalization : uint8_t {
        simple,   // the text as it is (§3.4.1, §3.4.3)
        relaxed   // whitespace and the case of names normalized (§3.4.2, §3.4.4)
    };

    // A signature's verdict as RFC 8601 §2.7.1 names it
    enum class status : uint8_t {
        none,        // the message has no signature
        pass,        // the signature verified
        fail,        // it did not: the head or the body changed
        policy,      // it verified but a local policy refused it
        neutral,     // it could not be processed (not made by this library)
        temperror,   // a failure that may pass: the key's lookup failed
        permerror    // one that will not: a malformed signature, no key, a revoked key, expired
    };

    // "pass", "fail", ... as Authentication-Results writes it
    inline string to_string(status s) noexcept {
        switch (s) {
            case status::none: return string("none");
            case status::pass: return string("pass");
            case status::fail: return string("fail");
            case status::policy: return string("policy");
            case status::neutral: return string("neutral");
            case status::temperror: return string("temperror");
            case status::permerror: return string("permerror");
        }
        return string("none");
    }

    // How a message is signed. The fields signed are those of RFC 6376
    // §5.4.1 the message has, with Message-ID, MIME-Version and the
    // content fields, unless headers names them (From always); each named
    // once more than the message has it (oversign), so that no field of
    // the name can be added after
    struct sign_options {
        canonicalization header = canonicalization::relaxed;
        canonicalization body = canonicalization::relaxed;
        vector<string> headers;               // the names signed; empty: the default list
        bool oversign = true;
        string identity;                      // i= ("@mail.example.com", "user@example.com"); empty: none (@d)
        optional<uint64_t> body_length;       // l=: the first bytes of the body signed (RFC 6376 §8.2 advises against it)
        optional<time::datetime> time;        // t=; none: now
        duration expiration = {};             // x= is t= and this; zero: none
    };

    class signer;

    namespace detail {
        using namespace sgcl::net::detail;

        struct DkimKey {
            optional<crypto::rsa::private_key> rsa;
            optional<crypto::ed25519::private_key> ed;
        };

        struct SignerState {
            string domain;
            string selector;
            dkim::algorithm alg = dkim::algorithm::rsa_sha256;
            std::unique_ptr<DkimKey> key;
        };

        struct SignerAccess;
    }

    // A signing identity: the domain (d=), the selector (s=) and the
    // private key, RSA (PKCS #8 or PKCS #1) or Ed25519 (PKCS #8). A handle
    // of one word whose state is made in the constructor; the key lives in
    // unmanaged memory and is never copied (copies of the handle share it),
    // as tls::identity's.
    class signer {
    public:
        // The key in PEM (its first private key block); errc::malformed
        // or unsupported (crypto) as an io::error of op "dkim" for a key
        // of another kind or one that does not read, net::errc::invalid_address
        // for a domain or a selector that is no name
        static expected<signer, io::error> from_pem(const string& domain, const string& selector, const slice<const byte>& key_pem) noexcept {
            if (!net::detail::mail_valid_domain(domain.view(), false) || !net::detail::mail_valid_domain(selector.view(), false)) {
                return unexpected(net::detail::net_error(net::errc::invalid_address, "dkim", string::concat(selector, "._domainkey.", domain)));
            }
            auto block = crypto::detail::read_key_pem(key_pem);
            if (!block) {
                return unexpected(io::error(block.error().code(), "dkim", block.error().message()));
            }
            auto s = make_tracked<detail::SignerState>();
            s->domain = string(net::detail::mail_domain(domain.view()));
            s->selector = string(net::detail::mail_lowered(selector.view()));
            s->key = std::make_unique<detail::DkimKey>();
            if (block->label == "RSA PRIVATE KEY") {
                auto k = crypto::rsa::private_key::from_pkcs1_der(block->der);
                if (!k) {
                    return unexpected(io::error(k.error().code(), "dkim", k.error().message()));
                }
                s->key->rsa.emplace(std::move(*k));
            } else if (block->label == "PRIVATE KEY") {
                if (auto r = crypto::rsa::private_key::from_pkcs8_der(block->der)) {
                    s->key->rsa.emplace(std::move(*r));
                } else if (auto e = crypto::ed25519::private_key::from_pkcs8_der(block->der)) {
                    s->key->ed.emplace(std::move(*e));
                    s->alg = algorithm::ed25519_sha256;
                } else {
                    return unexpected(io::error(crypto::make_error_code(crypto::errc::unsupported), "dkim", string("a private key of neither RSA nor Ed25519")));
                }
            } else {
                return unexpected(io::error(crypto::make_error_code(crypto::errc::unsupported), "dkim", string("a private key of neither RSA nor Ed25519")));
            }
            return signer(std::move(s));
        }

        // The signer a key the program holds spells: from_pem's value or its
        // bad_expected_access<io::error> (DESIGN 234)
        signer(const string& domain, const string& selector, const slice<const byte>& key_pem)
        : signer(from_pem(domain, selector, key_pem).value()) {
        }

        // A new key: RSA of 2048 bits or Ed25519; record() is what to
        // publish, private_key_pem() what to keep. A domain or a selector
        // that is no name: bad_expected_access<io::error> of
        // net::errc::invalid_address
        static signer generate(const string& domain, const string& selector, dkim::algorithm a = dkim::algorithm::rsa_sha256) {
            if (!net::detail::mail_valid_domain(domain.view(), false) || !net::detail::mail_valid_domain(selector.view(), false)) {
                expected<signer, io::error> bad = unexpected(net::detail::net_error(net::errc::invalid_address, "dkim", string::concat(selector, "._domainkey.", domain)));
                return bad.value();
            }
            auto s = make_tracked<detail::SignerState>();
            s->domain = string(net::detail::mail_domain(domain.view()));
            s->selector = string(net::detail::mail_lowered(selector.view()));
            s->alg = a;
            s->key = std::make_unique<detail::DkimKey>();
            if (a == algorithm::rsa_sha256) {
                s->key->rsa.emplace(crypto::rsa::private_key::generate(2048));
            } else {
                s->key->ed.emplace(crypto::ed25519::private_key::generate());
            }
            return signer(std::move(s));
        }

        signer(const signer&) noexcept = default;
        signer& operator=(const signer&) noexcept = default;

        // The message with a DKIM-Signature field put first: the body hashed
        // as the options say, the fields named signed, the head hashed and
        // signed with the key. The message's line breaks are CRLF (a bare
        // LF is made CRLF); errc::malformed_message for a message without
        // a head, without From, or with a line of the head that is no field,
        // or l= past the body; errc::invalid_address for an identity outside
        // the signer's domain
        expected<string, io::error> sign(const string& message) const;
        expected<string, io::error> sign(const string& message, const sign_options& o) const;

        // d=, in lower case
        string domain() const noexcept {
            return _s->domain;
        }

        // s=, in lower case
        string selector() const noexcept {
            return _s->selector;
        }

        dkim::algorithm algorithm() const noexcept {
            return _s->alg;
        }

        // Where the key's record is published: "s2026._domainkey.example.com"
        string record_name() const {
            return string::concat(_s->selector, "._domainkey.", _s->domain);
        }

        // The TXT record to publish there: "v=DKIM1; k=rsa; p=MIIBIjAN..."
        string record() const {
            const auto& b64 = encoding::base64::standard;
            if (_s->key->rsa) {
                auto der = _s->key->rsa->public_key().to_pkix_der();
                return string::concat("v=DKIM1; k=rsa; p=", b64.encode(der));
            }
            auto pk = _s->key->ed->public_key();
            const auto& bytes = pk.bytes();
            return string::concat("v=DKIM1; k=ed25519; p=", b64.encode(slice<const byte>(bytes.data(), bytes.size())));
        }

        // The private key as PEM, "PRIVATE KEY" over PKCS #8: a
        // secret_bytes, never managed memory
        crypto::secret_bytes private_key_pem() const {
            return _s->key->rsa ? _s->key->rsa->to_pem() : _s->key->ed->to_pem();
        }

        friend bool operator==(const signer& a, const signer& b) noexcept {
            return a._s == b._s;
        }

    private:
        friend struct detail::SignerAccess;

        explicit signer(tracked_ptr<detail::SignerState> s) noexcept
        : _s(std::move(s)) {
        }

        tracked_ptr<detail::SignerState> _s;
    };

    // A signature's verdict: the status, the signing domain and selector,
    // the identity, the algorithm as the signature names it, b= (whose
    // first characters Authentication-Results gives as header.b), why it
    // did not pass, and whether the key says it is in testing (t=y)
    struct result {
        dkim::status status = dkim::status::none;
        string domain;
        string selector;
        string identity;
        string algorithm;
        string signature;
        string reason;
        bool testing = false;
    };

    // How the signatures are checked: the resolver of the key records (the
    // module's own; servers empty: /etc/resolv.conf's), the signatures
    // checked (the first in the head; the rest ignored), the time x= is
    // compared against (none: now)
    struct verify_options {
        net::dns::options dns;
        size_t max_signatures = 5;
        optional<time::datetime> at;
    };

    namespace detail {
        inline constexpr std::string_view DkimDefaultHeaders[] = {
            "from", "reply-to", "subject", "date", "to", "cc", "resent-date", "resent-from", "resent-to", "resent-cc",
            "in-reply-to", "references", "list-id", "list-help", "list-unsubscribe", "list-subscribe", "list-post",
            "list-owner", "list-archive", "message-id", "mime-version", "content-type", "content-transfer-encoding"};

        // The body canonicalized (RFC 6376 §3.4.3, §3.4.4) into SHA-256,
        // its first limit bytes; total counts all of them
        struct DkimBody {
            crypto::sha256 h;
            bool relaxed = false;
            uint64_t limit = UINT64_MAX;
            uint64_t total = 0;
            uint64_t hashed = 0;
            std::string buf;
            size_t used = 0;        // the bytes of buf that hold output
            size_t empty_lines = 0;

            // n more bytes at buf's end, the buffer flushed into the hash first
            // when they would not fit
            char* room(size_t n) {
                if (used + n > buf.size()) {
                    flush();
                    if (n > buf.size()) {
                        buf.resize(std::max(n, size_t(65536)));
                    }
                }
                return buf.data() + used;
            }

            // what was written at room() counted, the part past l= dropped
            void take(size_t n) {
                total += n;
                uint64_t left = limit - hashed;
                size_t keep = left < n ? size_t(left) : n;
                used += keep;
                hashed += keep;
            }

            void flush() {
                if (used) {
                    h.update(slice<const byte>(reinterpret_cast<const byte*>(buf.data()), used));
                    used = 0;
                }
            }

            void line(std::string_view l) {
                if (relaxed) {
                    // WSP at the line's end dropped, runs of WSP made one SP
                    size_t end = l.size();
                    while (end > 0 && mail_wsp(l[end - 1])) {
                        --end;
                    }
                    l = l.substr(0, end);
                }
                if (l.empty()) {
                    ++empty_lines;
                    return;
                }
                char* out = room(2 * empty_lines + l.size() + 2);
                size_t n = 0;
                for (; empty_lines; --empty_lines) {
                    out[n++] = '\r';
                    out[n++] = '\n';
                }
                if (!relaxed) {
                    copy_bytes(out + n, l.data(), l.size());
                    n += l.size();
                } else {
                    // branchless: a WSP written as SP, a WSP after a WSP not counted
                    bool prev = false;
                    for (char c : l) {
                        bool w = mail_wsp(c);
                        out[n] = w ? ' ' : c;
                        n += size_t(!(w & prev));
                        prev = w;
                    }
                }
                out[n++] = '\r';
                out[n++] = '\n';
                take(n);
            }

            void body(std::string_view b) {
                buf.resize(65536);
                size_t at = 0;
                while (at < b.size()) {
                    size_t eol = b.find("\r\n", at);
                    if (eol == std::string_view::npos) {
                        line(b.substr(at));
                        break;
                    }
                    line(b.substr(at, eol - at));
                    at = eol + 2;
                }
                if (!relaxed && total == 0) {
                    char* out = room(2);   // an empty body is one CRLF in simple (§3.4.3)
                    out[0] = '\r';
                    out[1] = '\n';
                    take(2);
                }
                flush();
            }
        };

        inline array<byte, 32> dkim_body_hash(std::string_view body, canonicalization c, uint64_t limit, uint64_t& total) {
            DkimBody b;
            b.relaxed = c == canonicalization::relaxed;
            b.limit = limit;
            b.body(body);
            total = b.total;
            return b.h.value();
        }

        // A field relaxed (§3.4.2): the name in lower case, the value
        // unfolded, runs of WSP one SP, WSP around the colon and at the
        // value's ends dropped; CRLF after it
        inline void dkim_relaxed_field(std::string& out, const MailHeaderField& f) {
            for (char c : f.name) {
                out += mail_lower(c);
            }
            out += ':';
            std::string_view v = f.raw.substr(f.raw.find(':') + 1);
            bool space = false, any = false;
            for (char c : v) {
                if (c == '\r' || c == '\n') {
                    continue;
                }
                if (mail_wsp(c)) {
                    space = true;
                    continue;
                }
                if (space && any) {
                    out += ' ';
                }
                space = false;
                any = true;
                out += c;
            }
            out += "\r\n";
        }

        inline void dkim_field(std::string& out, const MailHeaderField& f, canonicalization c) {
            if (c == canonicalization::relaxed) {
                dkim_relaxed_field(out, f);
            } else {
                out.append(f.raw.data(), f.raw.size());
                if (f.raw.size() < 2 || f.raw.substr(f.raw.size() - 2) != "\r\n") {
                    out += "\r\n";
                }
            }
        }

        // The fields h= names, each the last not yet taken of its name from
        // the bottom (§5.4.2); a name with none left adds nothing
        inline void dkim_selected(std::string& out, const vector<MailHeaderField>& fields, const vector<std::string_view>& names, canonicalization c) {
            vector<pair<std::string_view, size_t>> next;   // a name and the index under which its next field is looked for
            for (auto name : names) {
                size_t* from = nullptr;
                for (auto& n : next) {
                    if (mail_iequal(n.first, name)) {
                        from = &n.second;
                    }
                }
                if (!from) {
                    next.push_back({name, fields.size()});
                    from = &next.back().second;
                }
                size_t i = *from;
                while (i > 0) {
                    --i;
                    if (mail_iequal(fields[i].name, name)) {
                        dkim_field(out, fields[i], c);
                        *from = i;
                        break;
                    }
                    if (i == 0) {
                        *from = 0;
                    }
                }
            }
        }

        // The signature's own field with b='s value emptied (§3.7)
        inline std::string dkim_without_b(std::string_view raw) {
            size_t colon = raw.find(':');
            size_t end = raw.size();
            if (end >= 2 && raw.substr(end - 2) == "\r\n") {
                end -= 2;
            }
            size_t at = colon + 1;
            while (at <= end) {
                size_t semi = raw.find(';', at);
                if (semi == std::string_view::npos || semi > end) {
                    semi = end;
                }
                std::string_view spec = raw.substr(at, semi - at);
                size_t eq = spec.find('=');
                if (eq != std::string_view::npos && mail_trim(spec.substr(0, eq)) == "b") {
                    std::string out(raw.substr(0, at + eq + 1));
                    out.append(raw.substr(semi));
                    return out;
                }
                at = semi + 1;
            }
            return std::string(raw);
        }

        // The head's hash: the fields h= names, then the signature's own
        // field without its CRLF
        inline array<byte, 32> dkim_head_hash(const vector<MailHeaderField>& fields, const vector<std::string_view>& names, std::string_view own_raw, canonicalization c) {
            std::string data;
            dkim_selected(data, fields, names, c);
            std::string own = dkim_without_b(own_raw);
            MailHeaderField f{std::string_view(own).substr(0, own.find(':')), own};
            while (!f.name.empty() && mail_wsp(f.name.back())) {
                f.name.remove_suffix(1);
            }
            dkim_field(data, f, c);
            data.resize(data.size() - 2);
            crypto::sha256 h;
            h.update(slice<const byte>(reinterpret_cast<const byte*>(data.data()), data.size()));
            return h.value();
        }

        inline bool dkim_number(std::string_view s, uint64_t& out) noexcept {
            if (s.empty() || s.size() > 19) {
                return false;
            }
            uint64_t n = 0;
            for (char c : s) {
                if (c < '0' || c > '9') {
                    return false;
                }
                n = n * 10 + uint64_t(c - '0');
            }
            out = n;
            return true;
        }

        inline vector<std::string_view> dkim_names(std::string_view h) {
            vector<std::string_view> out;
            size_t at = 0;
            while (at <= h.size()) {
                size_t c = h.find(':', at);
                std::string_view n = mail_trim(h.substr(at, c == std::string_view::npos ? std::string_view::npos : c - at));
                out.push_back(n);
                if (c == std::string_view::npos) {
                    break;
                }
                at = c + 1;
            }
            return out;
        }

        inline optional<vector<byte>> dkim_base64(std::string_view v) {
            auto d = encoding::base64::standard.decode(string(mail_no_space(v)));
            if (!d) {
                return nullopt;
            }
            return std::move(*d);
        }

        inline int64_t dkim_unix(const optional<time::datetime>& t) noexcept {
            return t ? t->unix() : time::now().unix();
        }

        // The signature's fields, read from its tag list
        struct DkimSignature {
            string algorithm;
            dkim::algorithm alg = dkim::algorithm::rsa_sha256;
            vector<byte> b;
            vector<byte> bh;
            std::string domain;
            std::string selector;
            std::string identity;
            vector<std::string_view> names;
            canonicalization head = canonicalization::simple;
            canonicalization body = canonicalization::simple;
            uint64_t length = UINT64_MAX;
            string b_text;
        };

        inline bool dkim_fail(dkim::result& r, dkim::status s, const char* why) {
            r.status = s;
            r.reason = string(why);
            return false;
        }

        // §6.1.1: the tags a signature must have, their values, the
        // identity under the domain, the expiry
        inline bool dkim_parse_signature(std::string_view raw, int64_t now, DkimSignature& sig, dkim::result& r) {
            vector<MailTag> tags;
            if (!mail_tag_list(raw.substr(raw.find(':') + 1), tags)) {
                return dkim_fail(r, status::permerror, "signature syntax error");
            }
            auto tag = [&](std::string_view n) { return mail_tag(tags, n); };
            if (auto a = tag("a")) {
                r.algorithm = string(a->value);
            }
            if (auto d = tag("d")) {
                r.domain = string(mail_domain(d->value));
            }
            if (auto s = tag("s")) {
                r.selector = string(mail_lowered(s->value));
            }
            if (auto b = tag("b")) {
                r.signature = string(mail_no_space(b->value));
            }
            auto v = tag("v");
            if (!v || v->value != "1") {
                return dkim_fail(r, status::permerror, "incompatible version");
            }
            for (auto n : {"a", "b", "bh", "d", "h", "s"}) {
                if (!tag(n)) {
                    return dkim_fail(r, status::permerror, "signature missing required tag");
                }
            }
            std::string a = mail_lowered(tag("a")->value);
            if (a == "rsa-sha256") {
                sig.alg = algorithm::rsa_sha256;
            } else if (a == "ed25519-sha256") {
                sig.alg = algorithm::ed25519_sha256;
            } else if (a == "rsa-sha1") {
                return dkim_fail(r, status::permerror, "rsa-sha1 is not accepted (RFC 8301)");
            } else {
                return dkim_fail(r, status::permerror, "unsupported algorithm");
            }
            auto b = dkim_base64(tag("b")->value);
            auto bh = dkim_base64(tag("bh")->value);
            if (!b || !bh || b->empty()) {
                return dkim_fail(r, status::permerror, "signature syntax error");
            }
            sig.b = std::move(*b);
            sig.bh = std::move(*bh);
            sig.domain = mail_domain(tag("d")->value);
            sig.selector = mail_lowered(tag("s")->value);
            if (!mail_valid_domain(sig.domain, false) || !mail_valid_domain(sig.selector, false)) {
                return dkim_fail(r, status::permerror, "signature syntax error");
            }
            sig.names = dkim_names(tag("h")->value);
            bool from = false;
            for (auto n : sig.names) {
                if (n.empty()) {
                    return dkim_fail(r, status::permerror, "signature syntax error");
                }
                from |= mail_iequal(n, "from");
            }
            if (!from) {
                return dkim_fail(r, status::permerror, "From field not signed");
            }
            if (auto c = tag("c")) {
                std::string cv = mail_lowered(c->value);
                size_t slash = cv.find('/');
                std::string h = cv.substr(0, slash);
                std::string bd = slash == std::string::npos ? std::string("simple") : cv.substr(slash + 1);
                auto canon = [](const std::string& s, canonicalization& out) {
                    if (s == "simple") {
                        out = canonicalization::simple;
                    } else if (s == "relaxed") {
                        out = canonicalization::relaxed;
                    } else {
                        return false;
                    }
                    return true;
                };
                if (!canon(h, sig.head) || !canon(bd, sig.body)) {
                    return dkim_fail(r, status::permerror, "unsupported canonicalization");
                }
            }
            if (auto i = tag("i")) {
                std::string id(mail_trim(i->value));
                size_t at = id.rfind('@');
                if (at == std::string::npos || !mail_under(mail_domain(std::string_view(id).substr(at + 1)), sig.domain)) {
                    return dkim_fail(r, status::permerror, "domain mismatch");
                }
                sig.identity = id;
            } else {
                sig.identity = "@" + sig.domain;
            }
            r.identity = string(sig.identity);
            if (auto l = tag("l")) {
                if (!dkim_number(l->value, sig.length)) {
                    return dkim_fail(r, status::permerror, "signature syntax error");
                }
            }
            if (auto q = tag("q")) {
                bool dns_txt = false;
                for (auto m : dkim_names(q->value)) {
                    dns_txt |= mail_iequal(m, "dns/txt");
                }
                if (!dns_txt) {
                    return dkim_fail(r, status::permerror, "unsupported query method");
                }
            }
            uint64_t t = 0, x = 0;
            bool has_t = false;
            if (auto tt = tag("t")) {
                if (!dkim_number(tt->value, t)) {
                    return dkim_fail(r, status::permerror, "signature syntax error");
                }
                has_t = true;
            }
            if (auto xx = tag("x")) {
                if (!dkim_number(xx->value, x)) {
                    return dkim_fail(r, status::permerror, "signature syntax error");
                }
                if (has_t && x < t) {
                    return dkim_fail(r, status::permerror, "signature syntax error");
                }
                if (now > 0 && uint64_t(now) > x) {
                    return dkim_fail(r, status::permerror, "signature expired");
                }
            }
            return true;
        }

        // A key of the record (§3.6.1), checked against the signature
        struct DkimPublic {
            optional<crypto::rsa::public_key> rsa;
            optional<crypto::ed25519::public_key> ed;
            bool testing = false;
            bool strict = false;
        };

        // status::none when the record is no key at all (another TXT of the name)
        inline dkim::status dkim_parse_key(std::string_view text, const DkimSignature& sig, DkimPublic& key, const char*& why) {
            vector<MailTag> tags;
            if (!mail_tag_list(text, tags)) {
                why = "key syntax error";
                return status::permerror;
            }
            if (auto v = mail_tag(tags, "v"); v && (v != &tags[0] || v->value != "DKIM1")) {
                why = "key syntax error";
                return status::permerror;
            }
            auto p = mail_tag(tags, "p");
            if (!p) {
                why = "key syntax error";
                return status::permerror;
            }
            if (auto h = mail_tag(tags, "h")) {
                bool ok = false;
                for (auto n : dkim_names(h->value)) {
                    ok |= mail_iequal(n, "sha256");
                }
                if (!ok) {
                    why = "inappropriate hash algorithm";
                    return status::permerror;
                }
            }
            if (auto s = mail_tag(tags, "s")) {
                bool ok = false;
                for (auto n : dkim_names(s->value)) {
                    ok |= n == "*" || mail_iequal(n, "email");
                }
                if (!ok) {
                    why = "inappropriate service type";
                    return status::permerror;
                }
            }
            if (auto t = mail_tag(tags, "t")) {
                for (auto f : dkim_names(t->value)) {
                    key.testing |= mail_iequal(f, "y");
                    key.strict |= mail_iequal(f, "s");
                }
            }
            std::string k = "rsa";
            if (auto kt = mail_tag(tags, "k")) {
                k = mail_lowered(kt->value);
            }
            bool want_rsa = sig.alg == algorithm::rsa_sha256;
            if ((want_rsa && k != "rsa") || (!want_rsa && k != "ed25519")) {
                why = "inappropriate key algorithm";
                return status::permerror;
            }
            std::string bytes64 = mail_no_space(p->value);
            if (bytes64.empty()) {
                why = "key revoked";
                return status::permerror;
            }
            auto der = dkim_base64(bytes64);
            if (!der) {
                why = "key syntax error";
                return status::permerror;
            }
            if (want_rsa) {
                auto r = crypto::rsa::public_key::from_pkix_der(*der);
                if (!r) {
                    r = crypto::rsa::public_key::from_pkcs1_der(*der);
                }
                if (!r) {
                    why = "key syntax error";
                    return status::permerror;
                }
                if (r->bits() < 1024) {
                    why = "key too small (RFC 8301)";
                    return status::permerror;
                }
                key.rsa.emplace(std::move(*r));
            } else {
                auto e = crypto::ed25519::public_key::from_bytes(*der);
                if (!e) {
                    why = "key syntax error";
                    return status::permerror;
                }
                key.ed.emplace(std::move(*e));
            }
            return status::pass;
        }

        // A message in CRLF, kept where it lies when it is one already
        struct DkimText {
            std::string copy;
            std::string_view view;

            explicit DkimText(std::string_view m) {
                if (mail_has_bare_lf(m)) {
                    copy = mail_crlf(m);
                    view = copy;
                } else {
                    view = m;
                }
            }
        };

        inline async::task<dkim::result> dkim_verify_one(MailSplit split, size_t index, int64_t now, net::dns::options o) {
            dkim::result r;
            DkimSignature sig;
            const MailHeaderField& own = split.fields[index];
            if (!dkim_parse_signature(own.raw, now, sig, r)) {
                co_return r;
            }
            // the key (§6.1.2)
            string name = string::concat(string(sig.selector), "._domainkey.", string(sig.domain), ".");
            auto txt = co_await net::dns::async_lookup_txt(name, o);
            if (!txt) {
                auto c = txt.error().code();
                if (c == net::errc::host_not_found || c == net::errc::no_data) {
                    r.status = status::permerror;
                    r.reason = string("no key for signature");
                } else {
                    r.status = status::temperror;
                    r.reason = string("key unavailable");
                }
                co_return r;
            }
            DkimPublic key;
            const char* why = "no key for signature";
            dkim::status got = status::none;
            for (auto& t : *txt) {
                DkimPublic k;
                const char* w = nullptr;
                dkim::status s = dkim_parse_key(t.view(), sig, k, w);
                if (s == status::pass) {
                    key = std::move(k);
                    got = s;
                    break;
                }
                if (got == status::none) {
                    why = w;
                    got = s;
                }
            }
            if (got != status::pass) {
                r.status = status::permerror;
                r.reason = string(why);
                co_return r;
            }
            r.testing = key.testing;
            if (key.strict) {
                size_t at = sig.identity.rfind('@');
                if (mail_domain(std::string_view(sig.identity).substr(at + 1)) != sig.domain) {
                    r.status = status::permerror;
                    r.reason = string("domain mismatch");
                    co_return r;
                }
            }
            // the body (§6.1.3)
            uint64_t total = 0;
            auto bh = dkim_body_hash(split.body, sig.body, sig.length, total);
            if (sig.length != UINT64_MAX && sig.length > total) {
                r.status = status::permerror;
                r.reason = string("body length tag exceeds the body");
                co_return r;
            }
            if (sig.bh.size() != bh.size() || !crypto::detail::equal_bytes(reinterpret_cast<const unsigned char*>(bh.data()), reinterpret_cast<const unsigned char*>(sig.bh.data()), bh.size())) {
                r.status = status::fail;
                r.reason = string("body hash did not verify");
                co_return r;
            }
            // the head
            auto hh = dkim_head_hash(split.fields, sig.names, own.raw, sig.head);
            slice<const byte> digest(hh.data(), hh.size());
            bool ok = key.rsa ? key.rsa->verify_digest(crypto::hash_id::sha256, digest, sig.b) : key.ed->verify(digest, sig.b);
            if (!ok) {
                r.status = status::fail;
                r.reason = string("signature did not verify");
                co_return r;
            }
            r.status = status::pass;
            co_return r;
        }

        inline async::task<vector<dkim::result>> dkim_verify(string message, verify_options o) {
            DkimText text(message.view());
            MailSplit split = mail_split(text.view);
            vector<dkim::result> out;
            int64_t now = dkim_unix(o.at);
            for (size_t i = 0; i < split.fields.size() && out.size() < o.max_signatures; ++i) {
                if (mail_iequal(split.fields[i].name, "DKIM-Signature")) {
                    out.push_back(co_await dkim_verify_one(split, i, now, o.dns));
                }
            }
            co_return out;
        }

        inline void dkim_fold(std::string& field, size_t& col, std::string_view piece) {
            if (col + piece.size() + 1 > 76) {
                field += "\r\n\t";
                col = 8;
            } else if (col > 0) {
                field += ' ';
                ++col;
            }
            field += piece;
            col += piece.size();
        }

        struct SignerAccess {
            static expected<string, io::error> sign(std::string_view message, const signer& sg, const sign_options& o) {
                const SignerState& st = *sg._s;
                DkimText text(message);
                MailSplit split = mail_split(text.view);
                bool has_from = false;
                for (auto& f : split.fields) {
                    has_from |= mail_iequal(f.name, "from");
                }
                if (split.malformed || split.fields.empty() || !has_from) {
                    return unexpected(net_error(net::errc::malformed_message, "dkim sign", string(split.fields.empty() ? "no head" : !has_from ? "no From field" : "a line of the head without a colon")));
                }
                if (!o.identity.empty()) {
                    std::string_view id = o.identity.view();
                    size_t at = id.rfind('@');
                    if (at == std::string_view::npos || !mail_under(mail_domain(id.substr(at + 1)), st.domain.view())) {
                        return unexpected(net_error(net::errc::invalid_address, "dkim sign", o.identity));
                    }
                }
                uint64_t total = 0;
                uint64_t limit = o.body_length ? *o.body_length : UINT64_MAX;
                auto bh = dkim_body_hash(split.body, o.body, limit, total);
                if (o.body_length && *o.body_length > total) {
                    return unexpected(net_error(net::errc::malformed_message, "dkim sign", string("l= past the body's canonical length")));
                }
                // h=: each name as many times as the message has it, once more oversigned
                vector<std::string_view> wanted;
                if (o.headers.empty()) {
                    for (auto n : DkimDefaultHeaders) {
                        wanted.push_back(n);
                    }
                } else {
                    wanted.push_back("from");
                    for (auto& n : o.headers) {
                        if (!mail_iequal(n.view(), "from")) {
                            wanted.push_back(n.view());
                        }
                    }
                }
                vector<std::string> names;
                for (auto n : wanted) {
                    size_t count = 0;
                    for (auto& f : split.fields) {
                        count += mail_iequal(f.name, n);
                    }
                    if (count == 0 && o.headers.empty()) {
                        continue;
                    }
                    if (count == 0 || o.oversign) {
                        ++count;
                    }
                    for (size_t i = 0; i < count; ++i) {
                        names.push_back(mail_lowered(n));
                    }
                }
                const auto& b64 = encoding::base64::standard;
                int64_t t = dkim_unix(o.time);
                std::string field = "DKIM-Signature:";
                size_t col = field.size();
                auto put = [&](std::string piece) { dkim_fold(field, col, piece); };
                put("v=1;");
                put(st.alg == algorithm::rsa_sha256 ? "a=rsa-sha256;" : "a=ed25519-sha256;");
                put(std::string("c=") + (o.header == canonicalization::relaxed ? "relaxed" : "simple") + "/" + (o.body == canonicalization::relaxed ? "relaxed" : "simple") + ";");
                put("d=" + std::string(st.domain.view()) + ";");
                put("s=" + std::string(st.selector.view()) + ";");
                put("t=" + std::to_string(t) + ";");
                if (o.expiration > duration::zero()) {
                    put("x=" + std::to_string(t + o.expiration.milliseconds() / 1000) + ";");
                }
                if (!o.identity.empty()) {
                    put("i=" + std::string(o.identity.view()) + ";");
                }
                if (o.body_length) {
                    put("l=" + std::to_string(*o.body_length) + ";");
                }
                for (size_t i = 0; i < names.size(); ++i) {
                    std::string piece = (i == 0 ? "h=" : "") + names[i] + (i + 1 < names.size() ? ":" : ";");
                    if (i == 0) {
                        put(piece);
                        continue;
                    }
                    if (col + piece.size() > 76) {
                        field += "\r\n\t";
                        col = 8;
                    }
                    field += piece;
                    col += piece.size();
                }
                put("bh=" + std::string(b64.encode(slice<const byte>(bh.data(), bh.size())).view()) + ";");
                put("b=");
                vector<std::string_view> hn;
                for (auto& n : names) {
                    hn.push_back(n);
                }
                auto hh = dkim_head_hash(split.fields, hn, field + "\r\n", o.header);
                slice<const byte> digest(hh.data(), hh.size());
                std::string sig;
                if (st.key->rsa) {
                    auto s = st.key->rsa->sign_digest(crypto::hash_id::sha256, digest);
                    sig = std::string(b64.encode(s).view());
                } else {
                    auto s = st.key->ed->sign(digest);
                    sig = std::string(b64.encode(slice<const byte>(s.data(), s.size())).view());
                }
                size_t at = 0;
                while (at < sig.size()) {
                    size_t room = col < 76 ? 76 - col : 0;
                    if (room < 8) {
                        field += "\r\n\t ";
                        col = 9;
                        room = 76 - col;
                    }
                    size_t n = std::min(room, sig.size() - at);
                    field.append(sig, at, n);
                    col += n;
                    at += n;
                }
                field += "\r\n";
                field.append(text.view.data(), text.view.size());
                return string(field);
            }
        };
    }

    inline expected<string, io::error> signer::sign(const string& message) const {
        return detail::SignerAccess::sign(message.view(), *this, sign_options());
    }

    inline expected<string, io::error> signer::sign(const string& message, const sign_options& o) const {
        return detail::SignerAccess::sign(message.view(), *this, o);
    }

    // A verdict per DKIM-Signature field, in the head's order (the first
    // max_signatures of them); none when the message has no signature.
    // Each key is asked of DNS ("s._domainkey.d"); a check never fails,
    // what went wrong is its status and reason
    // `verify(...)` on this thread, `co_await async_verify(...)` in a task
    inline vector<result> verify(const string& message, const verify_options& o = {}) {
        return detail::dkim_verify(message, o).wait();
    }

    inline async::task<vector<result>> async_verify(string message, verify_options o = {}) noexcept {
        return detail::dkim_verify(std::move(message), std::move(o));
    }
}
