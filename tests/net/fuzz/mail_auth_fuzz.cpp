//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Mail authentication's parsers and canonicalization on any bytes; the
// first byte picks what the rest is:
//   0  a message signed by net::dkim with an Ed25519 key, then checked
//      without DNS: its signature field parses, the body hash and the
//      head's hash recomputed from the signed message verify under the
//      key (canonicalization agrees with itself whatever the head holds)
//   1  a DKIM-Signature field and a key record, parsed: a failure has a
//      status of permerror and a reason, never a pass
//   2  a message: split, both canonicalizations of its body and of every
//      field, l= limits; the canonical length never shrinks with l=
//   3  an SPF record: parsed; every term's domain-spec of a record that
//      parses expands (no DNS: the deadline has passed) to at most 253 bytes
//   4  a DMARC record: one that parses writes itself back to the same record
//   5  an Authentication-Results value: one that parses writes itself back
//      to the same results
//   6  a DMARC aggregate report (XML)
// Every parser reads the input's own bytes or a malloc'd block of exactly
// its bytes (a DMARC report goes through encoding::xml, whose own harness
// reads it so), never a managed copy: a read past the end is ASan's to see.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/fuzz/mail_auth_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/smtp.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    namespace dk = sgcl::net::dkim::detail;
    namespace sp = sgcl::net::spf::detail;
    namespace nd = sgcl::net::detail;

    // The bytes in a malloc'd block of exactly their size, never a managed
    // or a std::string's copy: a read past their end is ASan's to see
    class Exact {
    public:
        explicit Exact(std::string_view s)
        : _p(static_cast<char*>(std::malloc(s.size()))), _n(s.size()) {
            std::copy_n(s.data(), s.size(), _p);
        }

        Exact(const Exact&) = delete;
        Exact& operator=(const Exact&) = delete;

        ~Exact() {
            std::free(_p);
        }

        std::string_view view() const noexcept {
            return std::string_view(_p, _n);
        }

    private:
        char* _p;
        size_t _n;
    };

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    const net::dkim::signer& signer() {
        static const std::string seed(32, '\x2a');
        static const crypto::ed25519::private_key key = *crypto::ed25519::private_key::from_seed(slice<const byte>(reinterpret_cast<const byte*>(seed.data()), seed.size()));
        static const crypto::secret_bytes pem = key.to_pem();
        static const rooted<net::dkim::signer> s(std::in_place, string("example.com"), string("f"), slice<const byte>(pem.as_slice()));
        return *s;
    }

    void sign_and_check(std::string_view text, uint8_t flags) {
        net::dkim::sign_options o;
        o.header = flags & 1 ? net::dkim::canonicalization::simple : net::dkim::canonicalization::relaxed;
        o.body = flags & 2 ? net::dkim::canonicalization::simple : net::dkim::canonicalization::relaxed;
        o.oversign = (flags & 4) != 0;
        if (flags & 8) {
            o.body_length = flags >> 4;
        }
        Exact message(text);
        auto r = dk::SignerAccess::sign(message.view(), signer(), o);
        if (!r) {
            check(r.error().code() == net::errc::malformed_message);
            return;
        }
        dk::DkimText t(r->view());
        auto split = nd::mail_split(t.view);
        check(!split.fields.empty() && nd::mail_iequal(split.fields[0].name, "DKIM-Signature"));
        dk::DkimSignature sig;
        net::dkim::result res;
        check(dk::dkim_parse_signature(split.fields[0].raw, 0, sig, res));
        uint64_t total = 0;
        auto bh = dk::dkim_body_hash(split.body, sig.body, sig.length, total);
        check(sig.bh.size() == 32 && std::equal(bh.begin(), bh.end(), sig.bh.begin()));
        auto hh = dk::dkim_head_hash(split.fields, sig.names, split.fields[0].raw, sig.head);
        auto pk = crypto::ed25519::public_key::from_bytes(*encoding::base64::standard.decode(string(std::string_view(signer().record().view()).substr(22))));
        check(pk && pk->verify(slice<const byte>(hh.data(), hh.size()), sig.b));
    }

    void signature_and_key(std::string_view text) {
        size_t cut = text.find('\n');
        Exact field_bytes("DKIM-Signature:" + std::string(text.substr(0, cut)) + "\r\n");
        std::string_view field = field_bytes.view();
        std::string_view key = cut == std::string_view::npos ? std::string_view() : text.substr(cut + 1);
        dk::DkimSignature sig;
        net::dkim::result r;
        if (!dk::dkim_parse_signature(field, 1700000000, sig, r)) {
            check(r.status == net::dkim::status::permerror && !r.reason.empty());
            return;
        }
        dk::DkimPublic pub;
        const char* why = nullptr;
        auto s = dk::dkim_parse_key(key, sig, pub, why);
        check(s == net::dkim::status::pass ? (bool(pub.rsa) != bool(pub.ed)) : (s == net::dkim::status::permerror && why));
        (void)dk::dkim_without_b(field);
    }

    void canonicalize(std::string_view text, uint8_t flags) {
        auto split = nd::mail_split(text);
        uint64_t whole = 0, limited = 0;
        for (auto c : {net::dkim::canonicalization::simple, net::dkim::canonicalization::relaxed}) {
            (void)dk::dkim_body_hash(split.body, c, UINT64_MAX, whole);
            (void)dk::dkim_body_hash(split.body, c, flags, limited);
            check(whole == limited);   // total counts every byte, the limit only what is hashed
            std::string out;
            for (auto& f : split.fields) {
                dk::dkim_field(out, f, c);
            }
        }
        vector<std::string_view> names;
        for (auto& f : split.fields) {
            names.push_back(f.name);
        }
        if (!split.fields.empty()) {
            (void)dk::dkim_head_hash(split.fields, names, split.fields[0].raw, net::dkim::canonicalization::relaxed);
        }
    }

    void spf_record(std::string_view text) {
        Exact record_bytes("v=spf1 " + std::string(text));
        std::string_view record = record_bytes.view();
        sp::SpfRecord rec;
        if (!sp::spf_parse(record, rec)) {
            return;
        }
        tracked_ptr st = make_tracked<sp::SpfState>();
        st->ip = net::ip_address::v4(192, 0, 2, 3);
        st->sender = "user@example.com";
        st->local = "user";
        st->sender_domain = "example.com";
        st->helo = "mail.example.com";
        st->deadline = time_point();   // passed: a %{p} answers at once, with no DNS
        st->max_lookups = 1000;
        for (auto& t : rec.terms) {
            if (t.domain.empty()) {
                continue;
            }
            std::string out;
            if (sp::spf_expand(st, "example.com", t.domain, false, &out).wait()) {
                check(out.size() <= 253 || out.find('.') == std::string::npos);
            }
        }
        for (auto* m : {&rec.redirect, &rec.exp}) {
            std::string out;
            if (!m->empty()) {
                (void)sp::spf_expand(st, "example.com", *m, false, &out).wait();
            }
        }
    }

    void dmarc_record(std::string_view text) {
        auto r = net::dmarc::detail::dmarc_record_parse(text);
        if (!r) {
            check(r.error().code() == net::errc::malformed_dmarc);
            return;
        }
        auto again = net::dmarc::record::parse(r->to_string());
        check(again && *again == *r);
    }

    void results(std::string_view text) {
        auto r = net::smtp::detail::results_parse(text);
        if (!r) {
            check(r.error().code() == net::errc::malformed_message);
            return;
        }
        auto again = net::smtp::authentication_results::parse(r->to_string());
        check(again && *again == *r);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 2) {
        return 0;
    }
    uint8_t mode = data[0] % 7;
    uint8_t flags = data[1];
    std::string_view text(reinterpret_cast<const char*>(data + 2), size - 2);
    switch (mode) {
        case 0: sign_and_check(text, flags); break;
        case 1: signature_and_key(text); break;
        case 2: canonicalize(text, flags); break;
        case 3: spf_record(text); break;
        case 4: dmarc_record(text); break;
        case 5: results(text); break;
        case 6: (void)net::dmarc::report::parse(string(text)); break;
    }
    return 0;
}
