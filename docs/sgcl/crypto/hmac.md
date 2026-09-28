# sgcl::crypto::hmac

```cpp
#include "sgcl/crypto/hmac.h"   // or "sgcl/crypto/crypto.h"

namespace sgcl::crypto {
    template<class H> class hmac;        // HMAC (FIPS 198-1, RFC 2104) over a digest of the module
    using hmac_sha256 = hmac<sha256>;
    using hmac_sha512 = hmac<sha512>;
}
```

HMAC, Go's `crypto/hmac`: a tag over a message under a secret key, which only a holder of the key can make and check — the integrity of a cookie, a webhook's signature, a request signed as AWS SigV4 signs it, the core of HKDF, PBKDF2 and TLS 1.3's key schedule. The digest is a parameter of the template, since it is known in the code (Go's `hmac.New(sha256.New, key)`): `hmac<sha256>`, `hmac<sha3_256>`, any digest of the module. Written from RFC 2104 and FIPS 198-1, tested against RFC 4231's vectors and against OpenSSL on keys of every length 0–300 and random messages fed in pieces.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **A key of any length**, bytes or text. One longer than the digest's block is hashed first, a shorter one padded with zeros, as the standard has it; an empty key is allowed and is no secret. RFC 2104 asks for at least the digest's size of random bytes ([`random::bytes(32)`](random.md)).
- **Check a tag with `verify`**, never with `==`: it compares in constant time ([`constant_time::equal`](constant_time.md)), so the time a check takes does not tell an attacker how many leading bytes of a forgery were right. A tag of another length is false. `verify` is `[[nodiscard]]`: a check whose result is dropped was never made.
- **A secret, so no copy.** The object holds the digest's state after the key XOR ipad and after the key XOR opad (the key's equivalent) and the running inner state; it is move-only, `clone()` makes a second one under the same key at the same point of its message (a hasher's branch, asked for by name), a move leaves the object moved from zeroed, and the destructor zeroes all three states with stores the compiler cannot drop ([`secure_zero`](secure_zero.md)). Keep a key object on the stack or in a `unique_ptr`: in a managed object it stays in memory until the cycle that finds it dead. The tag returned by value (`array<byte, N>`) is a copy on the caller's stack and is not zeroed.
- **Otherwise a hasher** of the [hash module](../hash/README.md): `update` takes bytes and text, `value()` is the tag and the hmac goes on, `reset()` drops the message and keeps the key, `copy_from(reader)` reads a stream into it; `hash::req::hasher<hmac<sha256>>` holds.
- **`of(data, key)`** is the one-shot form, the data first and the key after it, as every keyed type of the hash module has it (`siphash::of(data, key)`); the constructor takes the key alone.

## Members

```cpp
static constexpr size_t digest_size = H::digest_size;
static constexpr size_t block_size = H::block_size;

explicit hmac(const slice<const byte>& key) noexcept;           // bytes or text
hmac(hmac&& other) noexcept;                      // other zeroed
hmac& operator=(hmac&& other) noexcept;
hmac(const hmac&) = delete;
~hmac();                                          // the states zeroed
hmac clone() const noexcept;

void update(const slice<const byte>& data) noexcept;    // and the text forms of the mixin
array<byte, H::digest_size> value() const noexcept;     // the tag of the message so far
array<byte, H::digest_size> digest() const noexcept;
void reset() noexcept;                            // the message dropped, the key kept
[[nodiscard]] bool verify(const slice<const byte>& tag) const noexcept;

static array<byte, H::digest_size> of(const slice<const byte>& data, const slice<const byte>& key) noexcept;
expected<size_t, io::error> copy_from(const io::reader& r);  async::task<expected<size_t, io::error>> async_copy_from(const io::reader& r);
```

## Example

```cpp
#include "sgcl/crypto/hmac.h"
#include "sgcl/encoding/hex.h"
#include "sgcl/io/print.h"

using namespace sgcl;

int main() {
    auto tag = crypto::hmac_sha256::of("The quick brown fox jumps over the lazy dog", "key");
    println(encoding::hex::encode(tag));

    // a received tag, checked in constant time
    crypto::hmac_sha256 mac("key");
    mac.update("The quick brown fox ");
    mac.update("jumps over the lazy dog");
    println(mac.verify(tag) ? "authentic" : "forged");
    tag[0] ^= byte(1);
    println(mac.verify(tag) ? "authentic" : "forged");
}
```

Output:

```text
f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8
authentic
forged
```

## See also

[`hkdf`](hkdf.md) and [`pbkdf2`](pbkdf2.md), built on it; [`constant_time`](constant_time.md); [`sha256`](sha256.md), [`sha512`](sha512.md), [`sha3`](sha3.md).
