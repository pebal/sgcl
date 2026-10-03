//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The boundaries of the module (DESIGN 408) that its other suites leave
// out: the longest message of each AEAD and one byte past it, a message of
// no bytes given as a null slice, a tag alone; objects assigned to
// themselves and used after a move; the counters at their ends; a KDF's
// output over its own input and at the limit of its length; the secrets'
// containers moved onto themselves; the random generator at the sizes
// where it changes its way; ML-KEM's lengths and coefficients at q.
// What another suite covers is named where it would go.
#include "cipher_test.h"
#include "sgcl/io/io.h"

#include <cstdint>
#include <limits>
#include <stdexcept>

#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#endif

using namespace cipher_test;
using sgcl::slice;

namespace {
    // A slice that claims n bytes at p: for the length checks that come
    // before any byte is read or written, so that 64 GiB need not exist
    slice<const std::byte> claimed(const std::byte* p, uint64_t n) {
        return slice<const std::byte>(p, size_t(n));
    }

    slice<std::byte> claimed_out(std::byte* p, uint64_t n) {
        return slice<std::byte>(p, size_t(n));
    }

    // Moves an object onto itself through a reference, as a container's
    // reordering may (no -Wself-move for the compiler to see)
    template<class T>
    void move_onto_itself(T& t) {
        T& same = t;
        t = std::move(same);
    }
}

// The longest plaintext of each AEAD is what its specification allows under
// one nonce: GCM 2^36 - 32 bytes (SP 800-38D), ChaCha20-Poly1305 2^32 - 1
// blocks of 64 (RFC 8439), XChaCha20 the same
static_assert(crypto::aes_gcm::max_plaintext_size == (uint64_t(1) << 36) - 32);
static_assert(crypto::chacha20_poly1305::max_plaintext_size == ((uint64_t(1) << 32) - 1) * 64);
static_assert(crypto::xchacha20_poly1305::max_plaintext_size == ((uint64_t(1) << 32) - 1) * 64);
static_assert(crypto::aes_gcm::tag_size == 16 && crypto::chacha20_poly1305::tag_size == 16 && crypto::xchacha20_poly1305::tag_size == 16);

namespace {
    // One byte past the longest plaintext: every seal is std::length_error
    // before a byte is read and before the output is made; every open of
    // that much sealed data is errc::authentication before a byte is read
    template<class Aead>
    void past_the_longest(const Aead& a) {
        bytes nonce(Aead::nonce_size), out(64);
        std::byte any {};
        const uint64_t past = Aead::max_plaintext_size + 1;
        EXPECT_THROW((void)a.seal(nonce, claimed(&any, past)), std::length_error);
        EXPECT_THROW((void)a.seal(nonce, claimed(&any, past), bytes(3)), std::length_error);
        EXPECT_THROW((void)a.seal_to(out, nonce, claimed(&any, past)), std::length_error);
        auto o = a.open(nonce, claimed(&any, past + Aead::tag_size));
        ASSERT_FALSE(o.has_value());
        EXPECT_EQ(o.error().code(), crypto::errc::authentication);
        auto t = a.open_to(out, nonce, claimed(&any, past + Aead::tag_size));
        ASSERT_FALSE(t.has_value());
        EXPECT_EQ(t.error().code(), crypto::errc::authentication);
        // the largest size_t: no length wraps on the way
        const uint64_t largest = std::numeric_limits<size_t>::max();
        EXPECT_THROW((void)a.seal(nonce, claimed(&any, largest)), std::length_error);
        EXPECT_EQ(error_of(a.open(nonce, claimed(&any, largest))).code(), crypto::errc::authentication);
        // a wrong nonce is still the first thing said
        EXPECT_THROW((void)a.seal(bytes(Aead::nonce_size + 1), claimed(&any, past)), std::invalid_argument);
    }

    // No plaintext and no additional data, given as null slices, as empty
    // vectors and as nothing: the same 16-byte tag; it opens to nothing,
    // into a null output too, and the bytes of a larger output past what
    // was opened are left as they were
    template<class Aead>
    void nothing_sealed(const Aead& a) {
        bytes nonce(Aead::nonce_size, std::byte(9));
        auto tag = a.seal(nonce, slice<const std::byte>(), slice<const std::byte>());
        ASSERT_EQ(tag.size(), Aead::tag_size);
        EXPECT_EQ(to_hex(a.seal(nonce, bytes())), to_hex(tag));
        EXPECT_EQ(to_hex(a.seal(nonce, bytes(), bytes())), to_hex(tag));
        auto opened = a.open(nonce, tag, slice<const std::byte>());
        ASSERT_TRUE(opened.has_value());
        EXPECT_TRUE(opened->empty());
        auto none = a.open_to(slice<std::byte>(), nonce, tag);
        ASSERT_TRUE(none.has_value());
        EXPECT_EQ(*none, 0u);
        bytes into(16, std::byte(0xaa));
        EXPECT_EQ(a.seal_to(into, nonce, slice<const std::byte>()), Aead::tag_size);
        EXPECT_EQ(to_hex(into), to_hex(tag));
        // a tag of the empty message under additional data is another tag
        EXPECT_NE(to_hex(a.seal(nonce, bytes(), bytes(1))), to_hex(tag));
        // the tag alone, one byte short: authentication, nothing written
        EXPECT_EQ(error_of(a.open(nonce, slice<const std::byte>(tag.data(), Aead::tag_size - 1))).code(), crypto::errc::authentication);
        EXPECT_EQ(error_of(a.open(nonce, slice<const std::byte>())).code(), crypto::errc::authentication);
        // opened into a larger buffer: only the plaintext's bytes are written
        bytes pt = {std::byte(1), std::byte(2), std::byte(3)};
        auto sealed = a.seal(nonce, pt);
        bytes larger(10, std::byte(0xcc));
        auto n = a.open_to(larger, nonce, sealed);
        ASSERT_TRUE(n.has_value());
        EXPECT_EQ(*n, 3u);
        EXPECT_EQ(to_hex(larger), "010203cccccccccccccc");
    }

    // A key moved onto itself is the same key; a key moved from refuses
    // seal_to and open_to too, and works again once a key is moved into it
    template<class Aead>
    void moved_onto_itself_and_from(size_t key_size) {
        bytes key(key_size, std::byte(5)), nonce(Aead::nonce_size), out(64);
        Aead a(key);
        auto want = a.seal(nonce, bytes(7));
        move_onto_itself(a);
        EXPECT_EQ(to_hex(a.seal(nonce, bytes(7))), to_hex(want));
        Aead b = std::move(a);
        EXPECT_THROW((void)a.seal_to(out, nonce, bytes(7)), std::logic_error);
        EXPECT_THROW((void)a.open_to(out, nonce, want), std::logic_error);
        EXPECT_THROW((void)a.open(nonce, bytes(3)), std::logic_error);   // refused before the length is looked at
        move_onto_itself(a);
        EXPECT_THROW((void)a.seal(nonce, bytes(7)), std::logic_error);   // still moved from
        a = std::move(b);
        EXPECT_EQ(to_hex(a.seal(nonce, bytes(7))), to_hex(want));
    }
}

TEST(CryptoBoundary_Tests, AeadPastTheLongestMessage) {
    past_the_longest(crypto::aes_gcm(bytes(16)));
    past_the_longest(crypto::aes_gcm(bytes(32)));
    past_the_longest(crypto::chacha20_poly1305(bytes(32)));
    crypto::xchacha20_poly1305 x(bytes(32));
    past_the_longest(x);
    std::byte any {};
    EXPECT_THROW((void)x.seal_random(claimed(&any, crypto::xchacha20_poly1305::max_plaintext_size + 1)), std::length_error);
    const uint64_t sealed_past = crypto::xchacha20_poly1305::max_plaintext_size + 1 + 24 + 16;
    EXPECT_EQ(error_of(x.open_random(claimed(&any, sealed_past))).code(), crypto::errc::authentication);
}

TEST(CryptoBoundary_Tests, AeadOfNothing) {
    nothing_sealed(crypto::aes_gcm(bytes(16)));
    nothing_sealed(crypto::aes_gcm(bytes(24)));
    nothing_sealed(crypto::chacha20_poly1305(bytes(32)));
    crypto::xchacha20_poly1305 x(bytes(32));
    nothing_sealed(x);
    auto r = x.seal_random(slice<const std::byte>());
    ASSERT_EQ(r.size(), 24u + 16u);
    auto back = x.open_random(r, slice<const std::byte>());
    ASSERT_TRUE(back.has_value());
    EXPECT_TRUE(back->empty());
    // exactly a nonce and a tag of zeros: a forgery, not a crash
    EXPECT_EQ(error_of(x.open_random(bytes(40))).code(), crypto::errc::authentication);
}

TEST(CryptoBoundary_Tests, AeadKeysMovedOntoThemselvesAndFrom) {
    moved_onto_itself_and_from<crypto::aes_gcm>(16);
    moved_onto_itself_and_from<crypto::chacha20_poly1305>(32);
    moved_onto_itself_and_from<crypto::xchacha20_poly1305>(32);
}

// The block cipher and the streams moved onto themselves, and the streams
// over nothing: a null input is no keystream, and the next call goes on
// where the last left off
TEST(CryptoBoundary_Tests, StreamsMovedOntoThemselvesAndOverNothing) {
    crypto::aes a(bytes(16, std::byte(1)));
    auto block = a.encrypt_block({});
    move_onto_itself(a);
    EXPECT_EQ(to_hex(a.encrypt_block({})), to_hex(block));
    EXPECT_EQ(to_hex(a.decrypt_block(block)), to_hex(bytes(16)));

    for (int kind = 0; kind < 2; ++kind) {
        bytes ref(100), got(100);
        auto run = [&](auto& s) {
            s.xor_key_stream(slice<std::byte>(), slice<const std::byte>());
            s.xor_key_stream(slice<std::byte>(got.data(), 30), slice<const std::byte>(got.data(), 30));
            s.xor_key_stream(slice<std::byte>(), slice<const std::byte>());
            move_onto_itself(s);
            s.xor_key_stream(slice<std::byte>(got.data() + 30, 70), slice<const std::byte>(got.data() + 30, 70));
        };
        if (kind == 0) {
            crypto::aes_ctr s(bytes(16, std::byte(2)), bytes(16, std::byte(3)));
            crypto::aes_ctr t(bytes(16, std::byte(2)), bytes(16, std::byte(3)));
            t.xor_key_stream(ref, ref);
            run(s);
        } else {
            crypto::chacha20 s(bytes(32, std::byte(2)), bytes(24, std::byte(3)));
            crypto::chacha20 t(bytes(32, std::byte(2)), bytes(24, std::byte(3)));
            t.xor_key_stream(ref, ref);
            run(s);
        }
        EXPECT_EQ(to_hex(got), to_hex(ref)) << kind;
    }
}

// AES-CTR's counter is 128 bits and wraps: from a counter of all ones a
// seek of 2^64 - 1 blocks lands on 2^64 - 2, as the same key started there
// gives it
TEST(CryptoBoundary_Tests, CtrSeekWrapsAtTwoTo128) {
    bytes key(16, std::byte(4));
    crypto::aes_ctr from_top(key, hex("ffffffffffffffffffffffffffffffff"));
    from_top.seek(std::numeric_limits<uint64_t>::max());
    crypto::aes_ctr there(key, hex("0000000000000000fffffffffffffffe"));
    bytes a(48), b(48);
    from_top.xor_key_stream(a, a);
    there.xor_key_stream(b, b);
    EXPECT_EQ(to_hex(a), to_hex(b));
}

// XChaCha20 ends where ChaCha20 does: 2^32 blocks under one nonce, the
// counter given by seek (covered for the 12-byte nonce by
// CryptoContract_Tests.ChachaKeystreamEnds)
TEST(CryptoBoundary_Tests, XChachaKeystreamEnds) {
    crypto::chacha20 c(bytes(32), bytes(24));
    c.seek(0xffffffffu);
    bytes last(64), one(1);
    c.xor_key_stream(last, last);
    EXPECT_THROW(c.xor_key_stream(one, one), std::length_error);
    c.xor_key_stream(slice<std::byte>(), slice<const std::byte>());   // nothing asked: nothing refused
}

// The counter at its ends: started at all ones it gives that one nonce and
// is spent; moved onto itself it goes on; moved onto another counter it
// takes the other's place and spends it (covered: the start at zero, the
// carry into the high word, the last two nonces, a move out of a counter:
// CryptoContract_Tests.NonceCounterNeverRepeats)
TEST(CryptoBoundary_Tests, NonceCounterAtItsEnds) {
    sgcl::array<std::byte, 12> top;
    for (auto& b : top) {
        b = std::byte(0xff);
    }
    crypto::nonce_counter last(top);
    EXPECT_EQ(to_hex(last.next()), "ffffffffffffffffffffffff");
    EXPECT_THROW(last.next(), std::out_of_range);
    crypto::nonce_counter c;
    (void)c.next();
    move_onto_itself(c);
    EXPECT_EQ(to_hex(c.next()), "000000000000000000000001");
    crypto::nonce_counter d;
    d = std::move(c);
    EXPECT_EQ(to_hex(d.next()), "000000000000000000000002");
    EXPECT_THROW(c.next(), std::out_of_range);
    // a spent counter moved onto a fresh one spends it
    crypto::nonce_counter e;
    e = std::move(last);
    EXPECT_THROW(e.next(), std::out_of_range);
    move_onto_itself(e);
    EXPECT_THROW(e.next(), std::out_of_range);
}

// HKDF's output over the info it reads for every block, and PBKDF2's over
// the salt: the second block would read the first written into it, so an
// output that overlaps them is std::invalid_argument and nothing is
// written. The key, the input keying material and the password are read
// whole before the first byte is written: an output over them is fine
TEST(CryptoBoundary_Tests, KdfOutputOverItsInput) {
    using K = crypto::hkdf_sha256;
    bytes key = hex("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    bytes buffer(100, std::byte(7));
    bytes info(buffer.begin(), buffer.begin() + 40);
    bytes expected(70);
    K::expand_to(expected, key, info);
    bytes before = buffer;
    EXPECT_THROW(K::expand_to(slice<std::byte>(buffer.data() + 10, 70), key, slice<const std::byte>(buffer.data(), 40)), std::invalid_argument);
    EXPECT_EQ(to_hex(buffer), to_hex(before));
    EXPECT_THROW(K::expand_to(slice<std::byte>(buffer.data(), 70), key, slice<const std::byte>(buffer.data(), 40)), std::invalid_argument);
    EXPECT_THROW(K::derive_to(slice<std::byte>(buffer.data(), 70), bytes(), key, slice<const std::byte>(buffer.data() + 69, 1)), std::invalid_argument);
    auto prk = K::extract(bytes(), key);
    EXPECT_THROW(K::expand_to(slice<std::byte>(buffer.data(), 70), prk, slice<const std::byte>(buffer.data() + 60, 20)), std::invalid_argument);
    EXPECT_EQ(to_hex(buffer), to_hex(before));
    // touching but not overlapping: fine
    K::expand_to(slice<std::byte>(buffer.data() + 40, 60), key, slice<const std::byte>(buffer.data(), 40));
    EXPECT_EQ(to_hex(buffer.data() + 40, 60), to_hex(expected.data(), 60));
    // over the key: read before anything is written
    bytes k2 = key;
    bytes out_over_key(70);
    std::memcpy(out_over_key.data(), key.data(), key.size());
    K::expand_to(out_over_key, slice<const std::byte>(out_over_key.data(), 32), info);
    EXPECT_EQ(to_hex(out_over_key), to_hex(expected));

    using P = crypto::pbkdf2<crypto::sha256>;
    bytes salt_buffer(100, std::byte(3));
    bytes pexpected(64);
    P::derive_to(pexpected, text("password"), slice<const std::byte>(salt_buffer.data(), 16), 2);
    bytes pbefore = salt_buffer;
    EXPECT_THROW(P::derive_to(slice<std::byte>(salt_buffer.data(), 64), text("password"), slice<const std::byte>(salt_buffer.data(), 16), 2), std::invalid_argument);
    EXPECT_EQ(to_hex(salt_buffer), to_hex(pbefore));
    P::derive_to(slice<std::byte>(salt_buffer.data() + 16, 64), text("password"), slice<const std::byte>(salt_buffer.data(), 16), 2);
    EXPECT_EQ(to_hex(salt_buffer.data() + 16, 64), to_hex(pexpected));
    bytes over_password = text("password");
    over_password.resize(64);
    P::derive_to(over_password, slice<const std::byte>(over_password.data(), 8), slice<const std::byte>(salt_buffer.data(), 16), 2);
    EXPECT_EQ(to_hex(over_password), to_hex(pexpected));
}

// PBKDF2's limit of 2^32 - 1 blocks, at the lengths where counting the
// blocks of n would wrap: std::invalid_argument, nothing allocated and
// nothing written (covered: 0 iterations, n of 0, Crypto_Pbkdf2.Rfc6070AndRfc7914)
TEST(CryptoBoundary_Tests, Pbkdf2PastTheLongestOutput) {
    using P = crypto::pbkdf2<crypto::sha256>;
    const size_t largest = std::numeric_limits<size_t>::max();
    EXPECT_THROW((void)P::derive("p", "s", 1, largest), std::invalid_argument);
    EXPECT_THROW((void)P::derive("p", "s", 1, largest - 30), std::invalid_argument);
    const uint64_t longest = uint64_t(0xffffffffu) * 32;
    EXPECT_THROW((void)P::derive("p", "s", 1, size_t(longest + 1)), std::invalid_argument);
    std::byte any {};
    EXPECT_THROW(P::derive_to(claimed_out(&any, largest), "p", "s", 1), std::invalid_argument);
    EXPECT_THROW(P::derive_to(claimed_out(&any, longest + 1), "p", "s", 1), std::invalid_argument);
    using P512 = crypto::pbkdf2<crypto::sha512>;
    EXPECT_THROW((void)P512::derive("p", "s", 1, largest - 62), std::invalid_argument);
    // HKDF's limit, at the largest size_t (covered: max_size and one past it,
    // Crypto_Hkdf.AgainstOpenSsl)
    EXPECT_THROW((void)crypto::hkdf_sha256::expand(bytes(32), bytes(), largest), std::invalid_argument);
}

// The secrets' containers moved onto themselves keep their bytes; a
// secret_bytes moved from is empty and takes new bytes; a secret moved
// from is zeros
TEST(CryptoBoundary_Tests, SecretsMovedOntoThemselves) {
    for (size_t n : {size_t(0), size_t(64), size_t(65)}) {
        crypto::secret_bytes s(n);
        for (auto& b : s.as_slice()) {
            b = std::byte(0x3c);
        }
        move_onto_itself(s);
        ASSERT_EQ(s.size(), n);
        for (auto b : s.as_slice()) {
            ASSERT_EQ(b, std::byte(0x3c));
        }
        crypto::secret_bytes t = std::move(s);
        EXPECT_TRUE(s.empty());
        s.resize(3);
        EXPECT_EQ(to_hex(s), "000000");
        EXPECT_FALSE(s == t);
        crypto::secret_bytes u = s.clone();
        EXPECT_TRUE(u == s);
    }
    crypto::secret_bytes empty;
    EXPECT_TRUE(empty == crypto::secret_bytes());
    EXPECT_TRUE(empty.clone().empty());
    EXPECT_EQ(empty.as_slice().size(), 0u);

    auto dk = crypto::mlkem768::decapsulation_key::generate();
    auto e = dk.encapsulation_key().encapsulate();
    auto copy = e.shared_key.clone();
    move_onto_itself(e.shared_key);
    EXPECT_TRUE(e.shared_key == copy);
    auto taken = std::move(e.shared_key);
    EXPECT_TRUE(taken == copy);
    EXPECT_TRUE(all_zero(e.shared_key.bytes().data(), 32));
}

// The generator at the sizes where its way changes: nothing (a null slice,
// 0 bytes), one byte, its buffer of 576 and one past it, the 1024 bytes up
// to which it serves from the buffer and one past that (covered: a call
// past one buffer, Crypto_Random.FillsWholeBuffersPastOneCall)
TEST(CryptoBoundary_Tests, RandomAtItsSizes) {
    crypto::random::fill(slice<std::byte>());
    EXPECT_TRUE(crypto::random::bytes(0).empty());
    EXPECT_TRUE(crypto::random::secret(0).empty());
    for (size_t n : {size_t(1), size_t(575), size_t(576), size_t(577), size_t(1023), size_t(1024), size_t(1025), size_t(1153)}) {
        bytes a(n), b(n);
        crypto::random::fill(a);
        crypto::random::fill(b);
        if (n >= 16) {
            EXPECT_FALSE(all_zero(a.data(), n)) << n;
            EXPECT_NE(to_hex(a), to_hex(b)) << n;
            // the last 16 bytes too: a request is filled to its end
            EXPECT_FALSE(all_zero(a.data() + n - 16, 16)) << n;
        }
        EXPECT_EQ(crypto::random::bytes(n).size(), n);
        EXPECT_EQ(crypto::random::secret(n).size(), n);
    }
}

// Constant-time comparison and zeroing over nothing (covered: one bit of
// a byte, a length one short, two empty vectors:
// CryptoContract_Tests.ConstantTimeEqualAndSecureZero)
TEST(CryptoBoundary_Tests, ComparisonAndZeroingOfNothing) {
    EXPECT_TRUE(crypto::constant_time::equal(slice<const std::byte>(), bytes()));
    EXPECT_TRUE(crypto::constant_time::equal(slice<const std::byte>(), slice<const std::byte>()));
    EXPECT_FALSE(crypto::constant_time::equal(slice<const std::byte>(), bytes(1)));
    EXPECT_FALSE(crypto::constant_time::equal(bytes(1), slice<const std::byte>()));
    bytes x(5, std::byte(1));
    EXPECT_TRUE(crypto::constant_time::equal(x, x));   // a slice against itself
    crypto::secure_zero(slice<std::byte>());
    crypto::secure_zero(slice<std::byte>(x.data(), size_t(0)));
    EXPECT_EQ(to_hex(x), "0101010101");
}

// ML-KEM's lengths around each right one and its coefficients at q - 1 and
// q in the last place; a decapsulation key moved onto itself and assigned
// after a move (covered: one byte short, a 63-byte seed, q in the first
// coefficient, the operations of a key moved from: MlKem.TheContractOfTheTypes)
TEST(CryptoBoundary_Tests, MlKemLengthsAndCoefficients) {
    namespace k = crypto::mlkem768;
    auto dk = k::decapsulation_key::generate();
    auto ek = dk.encapsulation_key();
    auto e = ek.encapsulate();
    bytes ct(e.ciphertext.begin(), e.ciphertext.end());
    for (size_t n : {size_t(0), size_t(1), k::ciphertext_size + 1, k::ciphertext_size * 2}) {
        bytes c = ct;
        c.resize(n);
        EXPECT_EQ(error_of(dk.decapsulate(c)).code(), crypto::errc::malformed) << n;
    }
    EXPECT_EQ(error_of(dk.decapsulate(slice<const std::byte>())).code(), crypto::errc::malformed);
    for (size_t n : {size_t(0), size_t(32), size_t(65), size_t(128)}) {
        EXPECT_EQ(error_of(k::decapsulation_key::from_seed(bytes(n))).code(), crypto::errc::invalid_key) << n;
    }
    auto ek_bytes = ek.bytes();
    bytes ekb(ek_bytes.begin(), ek_bytes.end());
    for (size_t n : {size_t(0), k::encapsulation_key_size + 1}) {
        bytes b = ekb;
        b.resize(n);
        EXPECT_EQ(error_of(k::encapsulation_key::from_bytes(b)).code(), crypto::errc::invalid_key) << n;
    }
    // the last coefficient of the last polynomial: bytes 1150 and 1151 hold its twelve bits
    const size_t at = 384 * 3 - 2;
    bytes below = ekb, at_q = ekb;
    below[at] = std::byte((uint8_t(below[at]) & 0x0f) | 0x00);
    below[at + 1] = std::byte(0xd0);   // 0xD00 = 3328 = q - 1
    at_q[at] = std::byte((uint8_t(at_q[at]) & 0x0f) | 0x10);
    at_q[at + 1] = std::byte(0xd0);    // 0xD01 = 3329 = q
    EXPECT_TRUE(k::encapsulation_key::from_bytes(below).has_value());
    EXPECT_EQ(error_of(k::encapsulation_key::from_bytes(at_q)).code(), crypto::errc::invalid_key);
    // moved onto itself, moved from, assigned again
    move_onto_itself(dk);
    auto key = dk.decapsulate(e.ciphertext.as_slice());
    ASSERT_TRUE(key.has_value());
    EXPECT_TRUE(*key == e.shared_key);
    auto other = std::move(dk);
    EXPECT_THROW((void)dk.decapsulate(e.ciphertext.as_slice()), std::logic_error);
    dk = std::move(other);
    auto again = dk.decapsulate(e.ciphertext.as_slice());
    ASSERT_TRUE(again.has_value());
    EXPECT_TRUE(*again == e.shared_key);
}

// The private keys of the curves moved onto themselves sign and agree as
// before (a move out of one: Crypto_Ed25519.AKeyMovedFromIsUsedByNothing,
// Crypto_X25519.AKeyMovedFromIsUsedByNothing, Crypto_Ec.SecretsAreZeroed);
// a message of no bytes, given as a null slice, signs and verifies
TEST(CryptoBoundary_Tests, CurveKeysMovedOntoThemselvesAndEmptyMessages) {
    auto ed = crypto::ed25519::private_key::generate();
    auto sig = ed.sign(slice<const std::byte>());
    EXPECT_EQ(to_hex(ed.sign(bytes())), to_hex(sig.data(), 64));
    move_onto_itself(ed);
    EXPECT_TRUE(ed.public_key().verify(slice<const std::byte>(), slice<const std::byte>(sig.data(), 64)));
    EXPECT_FALSE(ed.public_key().verify(bytes(1), slice<const std::byte>(sig.data(), 64)));
    EXPECT_FALSE(ed.public_key().verify(slice<const std::byte>(), slice<const std::byte>()));

    auto x = crypto::x25519::private_key::generate();
    auto peer = crypto::x25519::private_key::generate();
    auto shared = x.shared_secret(peer.public_key());
    ASSERT_TRUE(shared.has_value());
    move_onto_itself(x);
    auto again = x.shared_secret(peer.public_key());
    ASSERT_TRUE(again.has_value());
    EXPECT_TRUE(*again == *shared);

    auto p = crypto::p256::private_key::generate();
    auto digest = crypto::sha256::of(bytes());
    move_onto_itself(p);
    auto s = p.sign_digest(digest);
    EXPECT_TRUE(p.public_key().verify_digest(digest, s));
}

// read_secret at the sizes where its buffer doubles (64 in the object, then
// 128, 256), and a path that opens but cannot be read, a directory: the
// error of the read, and the descriptor closed then as after a whole read,
// not left for the collector (covered: 0, 10, 64, 65 and 5000 bytes, a
// missing file: Crypto_SecretBytes.ReadSecret)
TEST(CryptoBoundary_Tests, ReadSecretAtItsEdges) {
    auto dir = sgcl::io::make_temp_dir({}, "crypto-test-*");
    ASSERT_TRUE(dir);
    sgcl::string path = sgcl::io::path::join(*dir, "key");
    for (size_t n : {size_t(127), size_t(128), size_t(129), size_t(256), size_t(257)}) {
        bytes content(n);
        for (size_t i = 0; i < n; ++i) {
            content[i] = std::byte(i * 7 + 1);
        }
        ASSERT_TRUE(sgcl::io::write_file(path, content));
        auto s = crypto::read_secret(path);
        ASSERT_TRUE(s) << n;
        EXPECT_EQ(to_hex(*s), to_hex(content)) << n;
    }
    auto first = crypto::read_secret(*dir);   // the reactor's and io's own descriptors made, if any
    EXPECT_FALSE(first);
#if !defined(_WIN32)
    int probe = ::open("/dev/null", O_RDONLY);
    ASSERT_GE(probe, 0);
    ::close(probe);
    auto failed = crypto::read_secret(*dir);
    EXPECT_FALSE(failed);
    int after = ::open("/dev/null", O_RDONLY);
    EXPECT_EQ(after, probe) << "the directory's descriptor was left open";
    ::close(after);
#endif
    sgcl::io::remove_all(*dir);
}

// The handles of x509 at their edges: a certificate from no bytes or no
// text is errc::malformed at offset 0; a pool from no text is empty; a
// pool is a handle, so one moved from is still the pool (a move copies the
// pointer) and one moved onto itself too (covered: the bounds of a
// certificate, Crypto_X509.TheBoundsOfACertificate; a pool from a file,
// Crypto_X509.PoolFromFile)
TEST(CryptoBoundary_Tests, X509HandlesAtTheirEdges) {
    auto none = crypto::x509::certificate::parse(slice<const std::byte>());
    ASSERT_FALSE(none);
    EXPECT_EQ(none.error().code(), crypto::errc::malformed);
    EXPECT_EQ(error_of(crypto::x509::certificate::from_pem(sgcl::string())).code(), crypto::errc::malformed);
    auto empty = crypto::x509::certificate_pool::from_pem(sgcl::string());
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.size(), 0u);
    crypto::x509::certificate_pool pool;
    EXPECT_TRUE(pool.empty());
    crypto::x509::certificate_pool other = std::move(pool);
    EXPECT_TRUE(pool.empty());   // NOLINT: a handle; the move copied it
    move_onto_itself(other);
    EXPECT_TRUE(other.empty());
    EXPECT_TRUE(other.clone().empty());
}

// The error at its edges: made with nothing it is errc::malformed at no
// offset (error/error.md); the largest offset is written whole; a code
// outside the list has the category's words for it (covered: the codes'
// words and comparisons, CryptoContract_Tests.ErrorValues and
// Crypto_Error.WordsAndCodes)
TEST(CryptoBoundary_Tests, ErrorAtItsEdges) {
    crypto::error none;
    EXPECT_EQ(none.code(), crypto::errc::malformed);
    EXPECT_EQ(none.offset(), 0u);
    EXPECT_EQ(none.reason(), crypto::x509::reason::none);
    EXPECT_EQ(std::string(none.message().view()), "malformed data");
    EXPECT_TRUE(none == crypto::error(crypto::errc::malformed));
    crypto::error far(crypto::errc::malformed, std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(std::string(far.message().view()), "offset 18446744073709551615: malformed data");
    EXPECT_FALSE(far == none);
    EXPECT_EQ(std::string(crypto::error(crypto::errc(0)).message().view()), "unknown crypto error");
    EXPECT_EQ(std::string(crypto::error(crypto::errc(200), sgcl::string()).message().view()), "unknown crypto error");
    EXPECT_EQ(crypto::make_error_code(crypto::errc(0)).message(), "unknown crypto error");
}

// The digests and HMAC over nothing given as a null slice, and SHAKE asked
// for no bytes (covered: every length from 0 against OpenSSL,
// Crypto_Digest.EveryLengthAgainstOpenSsl; an empty tag, Crypto_Hmac.ShapeAndVerify)
TEST(CryptoBoundary_Tests, DigestsOfNothing) {
    EXPECT_EQ(to_hex(crypto::sha256::of(slice<const std::byte>())), to_hex(crypto::sha256::of(bytes())));
    EXPECT_EQ(to_hex(crypto::sha512::of(slice<const std::byte>())), to_hex(crypto::sha512::of(bytes())));
    EXPECT_EQ(to_hex(crypto::sha3_256::of(slice<const std::byte>())), to_hex(crypto::sha3_256::of(bytes())));
    auto d = crypto::digest(crypto::hash_id::sha384, slice<const std::byte>());
    EXPECT_EQ(to_hex(d), to_hex(crypto::sha384::of(bytes())));
    EXPECT_TRUE(crypto::shake256::of(slice<const std::byte>(), 0).empty());
    crypto::shake128 x;
    x.update(slice<const std::byte>());
    EXPECT_TRUE(x.read(0).empty());
    auto first = x.read(5);
    EXPECT_EQ(to_hex(first), to_hex(crypto::shake128::of(bytes(), 5)));
    EXPECT_THROW(x.update(bytes(1)), std::invalid_argument);
    // a key of nothing, null or empty, is the same key; a key of a block and one past it
    EXPECT_EQ(to_hex(crypto::hmac<crypto::sha256>::of(bytes(), slice<const std::byte>())),
              to_hex(crypto::hmac<crypto::sha256>::of(bytes(), bytes())));
    crypto::hmac<crypto::sha256> h{slice<const std::byte>()};
    EXPECT_FALSE(h.verify(slice<const std::byte>()));
    EXPECT_TRUE(h.verify(crypto::hmac<crypto::sha256>::of(bytes(), bytes())));
    bytes block(64, std::byte(0x11)), past(65, std::byte(0x11));
    EXPECT_NE(to_hex(crypto::hmac<crypto::sha256>::of(bytes(), block)), to_hex(crypto::hmac<crypto::sha256>::of(bytes(), past)));
    // a key longer than a block is its digest (RFC 2104)
    auto hashed = crypto::sha256::of(past);
    EXPECT_EQ(to_hex(crypto::hmac<crypto::sha256>::of(bytes(), past)), to_hex(crypto::hmac<crypto::sha256>::of(bytes(), hashed)));
}
