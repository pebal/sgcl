[sgcl](../../README.md) › [crypto](../README.md)

# sgcl::crypto::hmac\<H\>

```cpp
#include "sgcl/crypto/hmac.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    template<class H>
    class hmac;

    using hmac_sha256 = hmac<sha256>;
    using hmac_sha512 = hmac<sha512>;
}
```

`sgcl::crypto::hmac<H>` is HMAC of FIPS 198-1 and RFC 2104, Go's `crypto/hmac`: a tag over a message under a secret
key, which only a holder of the key can make and check — the integrity of a cookie, a webhook's signature, a request
signed as AWS SigV4 signs it, the core of [hkdf](../hkdf/README.md), [pbkdf2](../pbkdf2/README.md) and TLS 1.3's key schedule. The digest
is a parameter of the template, since it is known in the code (Go's `hmac.New(sha256.New, key)`): `hmac<sha256>`,
`hmac<sha3_256>`, any digest of the module; `hmac_sha256` and `hmac_sha512` name the two most used. Written from
RFC 2104 and FIPS 198-1, tested against RFC 4231's vectors and against OpenSSL on keys of every length 0–300 and
random messages fed in pieces.

Where Go's HMAC is a `hash.Hash` like any other and keeps its key's pads in memory the collector frees, an `hmac`
is a secret: it cannot be copied, a move zeroes what it leaves, and its destructor zeroes its states. Otherwise it has
the shape of a hasher of the [hash module](../../hash/README.md).

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **A key of any length**, bytes or text. One longer than the digest's block is hashed first, a shorter one padded
  with zeros, as the standard has it; an empty key is allowed and is no secret. RFC 2104 asks for at least the
  digest's size of random bytes ([random](../random/README.md): `random::secret(32)`).
- **Check a tag with [verify](verify.md)**, never with `==`: it compares in constant time
  ([constant_time](../constant_time/README.md)), so the time a check takes does not tell an attacker how many leading bytes of
  a forgery were right. A tag of another length is false. `verify` is `[[nodiscard]]`: a check whose result is
  dropped was never made.
- **A secret, so no copy.** The object holds the digest's state after the key XOR ipad and after the key XOR opad
  (the key's equivalent) and the running inner state, never the key; it is move-only, [clone](clone.md) makes a
  second one under the same key at the same point of its message (a hasher's branch, asked for by name), a move
  leaves the object moved from zeroed, and the destructor zeroes all three states with stores the compiler cannot
  drop ([secure_zero](../secure_zero.md)). Keep a key object on the stack or in a `unique_ptr`: in a managed object it
  stays in memory until the cycle that finds it dead. The tag returned by value (`array<byte, N>`) is a copy on the
  caller's stack and is not zeroed.
- **Otherwise a hasher** of the [hash module](../../hash/README.md): `update` takes bytes and text, `value()` is the tag
  and the hmac goes on, `reset()` drops the message and keeps the key, `copy_from(reader)` reads a stream into it;
  `hash::req::hasher<hmac<sha256>>` holds.
- **[of](of.md)`(data, key)`** is the one-shot form, the data first and the key after it, as every keyed type of
  the hash module has it (`siphash::of(data, key)`); the constructor takes the key alone. With a key to give, there
  is no `of_file`: a file goes in through `copy_from`.
- **Nothing fails but a stream**: every member is `noexcept` but the mixin's `copy_from`, which waits for a stream
  and returns its error.

## Template parameters

| Parameter | Description |
|---|---|
| `H` | the digest: `sha1`, `sha224`, `sha256`, `sha384`, `sha512`, `sha512_256`, `sha3_224`, `sha3_256`, `sha3_384` or `sha3_512`. Any other type — a SHAKE, a hasher of the hash module, an `hmac` — is rejected at compile time. |

## Member objects

| Constant | Value | Description |
|---|---|---|
| `digest_size` | `H::digest_size` | the bytes of the tag, `static constexpr size_t` |
| `block_size` | `H::block_size` | the block of the digest, what the key is padded to, `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](hmac.md) | an hmac under a key; the move constructor |
| `(destructor)` | zeroes the three states |
| [operator=](operator_assign.md) | the move assignment |
| [clone](clone.md) | a second hmac under the same key, at the same point of its message |

#### Hashing

| Function | Description |
|---|---|
| [update](update.md) | hashes bytes of the message in |
| [value](value.md) | the tag of the message so far; the hmac goes on |
| [digest](digest.md) | the same bytes, under the name every hasher has |
| [reset](reset.md) | the message dropped, the key kept |
| [verify](verify.md) | checks a received tag in constant time |
| [of](of.md) | the tag of data under a key in one call (static) |

#### From mixin::hasher

The forms every hasher has ([hash::mixin::hasher](../../hash/mixin/hasher/README.md)).

| Function | Description |
|---|---|
| `update` | text (a `string`, a text slice, a literal, a C string, a `std::string_view`), a digest, a `std::span` of bytes |
| `copy_from`, `async_copy_from` | a stream read to its end into the hmac |

## Complexity

Linear in the bytes of the message, one run of the digest over them. The constructor hashes the key's two pads once
(and the key itself first when it is longer than a block), and every message after it, and after every `reset()`,
starts from the states they left. `value()` finishes the inner digest and runs the outer one over its result: a few
compressions.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

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

- [hkdf](../hkdf/README.md), [pbkdf2](../pbkdf2/README.md): built on it
- [constant_time](../constant_time/README.md): how `verify` compares
- [sha256](../sha256/README.md), [sha512](../sha512/README.md), [sha3_256](../sha3_256/README.md): the digests it takes
- [hash::mixin::hasher](../../hash/mixin/hasher/README.md): the shape it shares
- [The module](../README.md)
