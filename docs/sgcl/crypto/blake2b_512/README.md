[sgcl](../../README.md) › [crypto](../README.md)

# sgcl::crypto::blake2b_512

```cpp
#include "sgcl/crypto/blake2.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class blake2b_512;
    class blake2b_384;
    class blake2b_256;
    class blake2s_256;
    class blake2s_128;
}
```

`sgcl::crypto::blake2b_512` is BLAKE2b of RFC 7693 with a digest of 64 bytes, Go's `blake2b.New512` and `Sum512` of
`golang.org/x/crypto/blake2b`: a digest from the ChaCha family, faster than SHA-2 and SHA-3 where the processor has
no instructions for them, and its own MAC when made with a key. BLAKE2b runs on 64-bit words in blocks of 128 bytes;
BLAKE2s, its sibling on 32-bit words in blocks of 64, is the hash of WireGuard and of the Noise protocols. The digest
length is a parameter of the function, not a cut of a longer digest, so each length in use is a type of its own:
`blake2b_512`, `blake2b_384` and `blake2b_256` (BLAKE2b with 64, 48 and 32 bytes), `blake2s_256` and `blake2s_128`
(BLAKE2s with 32 and 16). Everything below is the same for the five but the sizes, which the table of member objects
gives.

A hasher is made plain, or from a [blake2_options](../blake2_options.md) with a key (up to 64 bytes for BLAKE2b, 32
for BLAKE2s), a salt and a personalization (up to 16 bytes each, 8 for BLAKE2s), the parameters of RFC 7693's
parameter block. With a key the digest is a MAC, which [verify](verify.md) checks in constant time; a
personalization keeps the digests of two uses of one program apart. Where Go's hashers are a `hash.Hash` behind an
interface, these have the shape of every hasher of the [hash module](../../hash/README.md). The five are written from
RFC 7693; the tests hold them against its examples and its self-test, against OpenSSL's BLAKE2 digests and MACs on
random data fed at once and in pieces, and against Python's `hashlib` for the salt and the personalization.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **The shape of a hasher** of the [hash module](../../hash/README.md): `update` takes bytes and text, `value()` is the
  digest and ends nothing, `digest()` is the same bytes, `reset()` is as new, `of(data)` is the one-shot form,
  `copy_from(reader)` reads a stream to its end. A `crypto::blake2b_512` goes wherever a `hash::req::hasher` is asked
  for, and each of the five is a digest [hmac](../hmac/README.md), [hkdf](../hkdf/README.md) and
  [pbkdf2](../pbkdf2/README.md) take (`hmac<blake2s_256>`, Noise's and WireGuard's HMAC).
- **A copy is a branch**: a common prefix hashed once, then two ways. A hasher is the chaining words, a block's
  buffer, the counter and what `reset()` goes back to, about 400 bytes for BLAKE2b and 200 for BLAKE2s, nothing for
  the collector.
- **A keyed hasher holds its key's equivalent** and zeroes its state in its destructor, every copy its own; an
  unkeyed one is left as [sha256](../sha256/README.md) leaves its state. It belongs on the stack or in a `unique_ptr`,
  not in a managed object, where it would stay in memory until the collector found the object dead.
- **A key, a salt or a personalization longer than its field** is a broken contract: `std::invalid_argument` from
  the constructor. Everything else is `noexcept`; only the mixin's `copy_from` and `of_file` wait, for a stream or a
  file, and return its error. One object is one thread's at a time, as any value.
- **Not for passwords**: a digest is fast by design; a password wants a slow function.

## Member objects

| Constant | Value | Description |
|---|---|---|
| `digest_size` | `64`, `48`, `32` (BLAKE2b); `32`, `16` (BLAKE2s) | the bytes of the digest, `static constexpr size_t` |
| `block_size` | `128` (BLAKE2b), `64` (BLAKE2s) | the bytes of a block, what [hmac](../hmac/README.md) pads its key to, `static constexpr size_t` |
| `max_key_size` | `64` (BLAKE2b), `32` (BLAKE2s) | the longest key, `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](blake2b_512.md) | a hasher of nothing yet, plain or with a key, a salt and a personalization |
| `(destructor)` | zeroes the state of a keyed hasher |

#### Hashing

| Function | Description |
|---|---|
| [update](update.md) | hashes bytes in |
| [value](value.md) | the digest of everything so far; the hasher goes on |
| [digest](digest.md) | the same bytes, under the name every hasher has |
| [reset](reset.md) | as new, the key kept |
| [verify](verify.md) | whether a tag is the digest of the message, in constant time |
| [of](of.md) | the digest of data in one call, plain or with options (static) |

#### From mixin::hasher

The forms every hasher has ([hash::mixin::hasher](../../hash/mixin/hasher/README.md)).

| Function | Description |
|---|---|
| `update` | text (a `string`, a text slice, a literal, a C string, a `std::string_view`), a digest, a `std::span` of bytes |
| `copy_from`, `async_copy_from` | a stream read to its end into the hasher |
| `of_file`, `async_of_file` | the digest of a whole file, or the file's error (static) |

## Complexity

Linear in the bytes hashed: one compression a block, 12 rounds of eight mixes for BLAKE2b, 10 for BLAKE2s. One path,
plain C++ with the rounds unrolled, on every processor: NEON was written for both and measured slower, since the
sixteen mixes of one block are a chain each step of which waits for the last, and a vector operation's latency is
longer than a scalar one's. Every operation is an addition, a XOR or a rotation by a constant, so a keyed hash
takes the same time for every key and message of one length.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::hex::encode(crypto::blake2b_512::of("abc")));
    println(encoding::hex::encode(crypto::blake2s_256::of("abc")));

    // a MAC of 16 bytes under a key
    vector<byte> key = encoding::hex::decode("000102030405060708090a0b0c0d0e0f");
    println(encoding::hex::encode(crypto::blake2s_128::of("message", {.key = key})));
}
```

Output:

```text
ba80a53f981c4d0d6a2797b69f12f6e94c212f14685ac4b74b12bb6fdbffa2d17d87c5392aab792dc252d5de4533cc9518d38aa8dbf1925ab92386edd4009923
508c5e8c327c14e2e1a72ba34eeb452f37458b209ed63a294d999b4c86675982
37d0934cac1b83cf742574ea32dd9481
```

## See also

- [blake2_options](../blake2_options.md): the key, the salt and the personalization
- [sha256](../sha256/README.md), [sha512](../sha512/README.md), [sha3_256](../sha3_256/README.md): the other digests
- [hmac](../hmac/README.md): a tag over any digest of the module
- [hash::mixin::hasher](../../hash/mixin/hasher/README.md): the shape every hasher shares
- [The module](../README.md)
