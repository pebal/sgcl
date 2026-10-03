[sgcl](../README.md) › [hash](README.md)

# sgcl::hash::siphash

```cpp
#include "sgcl/hash/siphash.h"   // or "sgcl/hash.h"

namespace sgcl::hash {
    class siphash;  // SipHash-2-4, a key of 128 bits
}
```

`sgcl::hash::siphash` is SipHash-2-4 (Aumasson and Bernstein, [SipHash: a fast short-input
PRF](https://eprint.iacr.org/2012/351), 2012): a keyed hash with an argument behind it. To whoever does not have
the key its values are as good as random, even to one who picks the inputs and sees the outputs, so the buckets of
a table hashed with it cannot be aimed at. It is the one hash of the module for keys that come from an adversary
who may also see hashes; Python, Rust and Perl hash their tables' strings with it.

It is not a hash for integrity or for passwords: 64 bits of output are a table's hash, not a message authentication
code worth the name, and there is nothing slow in it. Those are [crypto](../crypto/README.md)'s. Go's standard
library has no SipHash (the runtime's is internal); the Go column below is `github.com/dchest/siphash`.

## Rules

- **The key is mandatory**: sixteen bytes, and no hasher is made without it. `siphash()` does not exist, nor
  `siphash::of(data)`, nor `of_file`, which needs a hasher made without arguments: a file goes in through
  [copy_from](mixin/hasher/copy_from.md). A program makes a key once from a source of randomness,
  [crypto::random](../crypto/random.md), and keeps it secret.
- **The key's bytes** are read as two little-endian words, as the paper and the reference implementation read them,
  so a key written down as bytes gives the reference's values.
- **The value** is the 64-bit result, as the paper writes it; `digest()` is, as every digest of the module, that
  number the most significant byte first. The reference implementation and Go's `dchest/siphash` write the same
  number the least significant byte first: to compare with their bytes, compare `value()`.
- **`reset()` keeps the key.** A hasher is 72 bytes, a plain value: the four words of the state, the key's two, the
  length, and the bytes of a word not yet complete with their count. A copy is a branch.
- **Nothing fails** but `copy_from`, which reads a stream and returns its error.

### From code written for Go

| With Go | With sgcl::hash |
|---|---|
| `siphash.New(key)` | `hash::siphash h(key);`, a key of sixteen bytes |
| `siphash.Hash(k0, k1, p)` | `siphash::of(p, key)`: the key as bytes, `k0` its first eight, little-endian |
| `h.Sum64()` | `h.value()` |
| `h.Sum(nil)` | `h.digest()`: the same number the other way round, big-endian here as every digest of the module |
| `siphash.New128`, `Hash128` | —: the 128-bit variant is not here |

## Member objects

| Constant | Value | Description |
|---|---|---|
| `digest_size` | `8` | the size of [digest](siphash/digest.md) in bytes, Go's `Size()`; `static constexpr size_t` |
| `block_size` | `8` | a word, the bytes the algorithm takes at a time, Go's `BlockSize()`; `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](siphash/siphash.md) | a hasher of no bytes yet, under a key |

#### Modifiers

| Function | Description |
|---|---|
| [update](siphash/update.md) | hashes bytes |
| [reset](siphash/reset.md) | puts the hasher back as it was made, with its key |

#### Observers

| Function | Description |
|---|---|
| [value](siphash/value.md) | the hash of the bytes so far |
| [digest](siphash/digest.md) | the hash as bytes, the most significant first |

#### From mixin::hasher

The rest of the shape every hasher shares ([mixin::hasher](mixin/hasher.md)).

| Function | Description |
|---|---|
| [update](mixin/hasher/update.md) | hashes a text, a digest or a std::span of bytes |
| [copy_from, async_copy_from](mixin/hasher/copy_from.md) | hashes a stream to its end |
| [of](mixin/hasher/of.md) | the hash of bytes or a text under a key, in one call (static) |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // the key and the message of the paper's appendix: 00 01 … 0f, and 00 01 … 0e
    array<byte, 16> key;
    for (int i : range(16)) {
        key[i] = byte(i);
    }
    array<byte, 15> message;
    for (int i : range(15)) {
        message[i] = byte(i);
    }
    println("{:016x}", hash::siphash::of(message, key));

    // in pieces, the same value
    hash::siphash h(key);
    h.update("hello, ");
    h.update("world");
    println("{}", h.value() == hash::siphash::of("hello, world", key));
}
```

Output:

```text
a129ca6149be45e5
true
```

## See also

- [maphash](maphash.md): the fast hash of a table whose hashes stay in the process
- [xxh3_64](xxh3_64.md): a hash for values written down
- [mixin::hasher](mixin/hasher.md): the shape every hasher shares
- [sgcl::hash](README.md)
