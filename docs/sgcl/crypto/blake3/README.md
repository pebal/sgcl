[sgcl](../../README.md) › [crypto](../README.md)

# sgcl::crypto::blake3

```cpp
#include "sgcl/crypto/blake3.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class blake3;
}
```

`sgcl::crypto::blake3` is BLAKE3, the hash of its 2021 specification: a hash, a MAC and a key derivation function in
one, with an output of any length read from any position. The input is cut into chunks of 1 KiB, each hashed on its
own, and the chunks' chaining values are joined two by two up a binary tree, so the chunks of a long input are hashed
side by side: four at a time on the processor's vector units here, which makes BLAKE3 the fastest digest of the
module on a long input. Neither Go's standard library nor `golang.org/x/crypto` has it; the C library of its authors is
the reference the tests hold it to, and a hasher has the shape of every hasher of the [hash
module](../../hash/README.md).

The three modes are three ways to make a hasher: plain, the hash; with a key of 32 bytes, `keyed_hash`, a MAC that
[verify](verify.md) checks; [for_derive_key](for_derive_key.md) with a context string, `derive_key`, a key derivation
function whose one-call form is [derive_key](derive_key.md). [value](value.md) is the first 32 bytes of the output,
[value_to](value_to.md) any number of them from any position.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **The shape of a hasher** of the [hash module](../../hash/README.md): `update` takes bytes and text, `value()` is the
  digest and ends nothing, `digest()` is the same bytes, `reset()` is as new, `of(data)` is the one-shot form,
  `copy_from(reader)` reads a stream to its end. A `crypto::blake3` goes wherever a `hash::req::hasher` is asked for.
- **A copy is a branch**: a common prefix hashed once, then two ways. A hasher is the key, the chunk being filled and
  the stack of the chaining values of the subtrees to its left, about 1.9 KB, nothing for the collector.
- **A keyed or derive_key hasher holds its key's equivalent** and zeroes its state in its destructor, every copy its
  own; a plain one is left as [sha256](../sha256/README.md) leaves its state. It belongs on the stack or in a
  `unique_ptr`, not in a managed object.
- **A key of another length than 32 bytes** is a broken contract: `std::invalid_argument`. Everything else is
  `noexcept`; only the mixin's `copy_from` and `of_file` wait, for a stream or a file, and return its error. One object
  is one thread's at a time, as any value.
- **A context of derive_key is not a variable**: a string fixed in the program, unique to the application and the
  purpose (`"example.com 2026-10-05 session tokens v1"`); what varies is the key material.
- **Not for passwords**: a digest is fast by design; a password wants a slow function.

## Member objects

| Constant | Value | Description |
|---|---|---|
| `digest_size` | `32` | the bytes of `value()`, `static constexpr size_t`; the output itself has any length |
| `block_size` | `64` | the bytes of a block, `static constexpr size_t` |
| `key_size` | `32` | the bytes of a key, `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](blake3.md) | a hasher of nothing yet, plain or keyed |
| `(destructor)` | zeroes the state of a keyed or derive_key hasher |

#### Hashing

| Function | Description |
|---|---|
| [update](update.md) | hashes bytes in |
| [value](value.md) | the first 32 bytes of the output; the hasher goes on |
| [value_to](value_to.md) | any number of bytes of the output, from any position |
| [digest](digest.md) | the same bytes as `value()`, under the name every hasher has |
| [reset](reset.md) | as new, the key and the mode kept |
| [verify](verify.md) | whether a tag is the start of the output, in constant time |
| [of](of.md) | the hash, or the keyed hash, of data in one call (static) |
| [derive_key](derive_key.md) | a key derived from key material under a context, in one call (static) |
| [for_derive_key](for_derive_key.md) | a hasher in the derive_key mode, for key material in pieces (static) |

#### From mixin::hasher

The forms every hasher has ([hash::mixin::hasher](../../hash/mixin/hasher/README.md)).

| Function | Description |
|---|---|
| `update` | text (a `string`, a text slice, a literal, a C string, a `std::string_view`), a digest, a `std::span` of bytes |
| `copy_from`, `async_copy_from` | a stream read to its end into the hasher |
| `of_file`, `async_of_file` | the hash of a whole file, or the file's error (static) |

## Complexity

Linear in the bytes hashed, one compression a block of 64 (seven rounds of eight mixes), and one more a chunk of
1 KiB for the tree. On arm64 the whole chunks an `update` is given go through NEON four at a time, a lane a chunk, and
so do the levels of parents above them and the blocks of a long output; on x86-64 the same four lanes run on SSE2.
Both are in the processors' minimum and need no question asked. A single block — the last chunk, the root, a short
output — is plain C++, which is the whole of the path with `SGCL_CRYPTO_PORTABLE` defined; the tests hold the paths
against each other. Every operation is an addition, a XOR or a rotation by a constant: the time depends on the
lengths and the position read, never on the key or the bytes.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::hex::encode(crypto::blake3::of("abc")));

    // 64 bytes of output: the first 32 are the hash
    crypto::blake3 h;
    h.update("abc");
    vector<byte> out(64);
    h.value_to(out);
    println(encoding::hex::encode(out));
}
```

Output:

```text
6437b3ac38465133ffb63b75273a8db548c558465d79db03fd359c6cd5bd9d85
6437b3ac38465133ffb63b75273a8db548c558465d79db03fd359c6cd5bd9d851fb250ae7393f5d02813b65d521a0d492d9ba09cf7ce7f4cffd900f23374bf0b
```

## See also

- [blake2b_512](../blake2b_512/README.md): BLAKE2, the hash BLAKE3 grew from
- [sha256](../sha256/README.md), [shake256](../shake256/README.md): the other digests and the other output of any length
- [hkdf](../hkdf/README.md): the key derivation of TLS
- [hash::mixin::hasher](../../hash/mixin/hasher/README.md): the shape every hasher shares
- [The module](../README.md)
