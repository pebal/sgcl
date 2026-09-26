# sgcl::hash::siphash

```cpp
#include "sgcl/hash/siphash.h"   // or "sgcl/hash/hash.h"

namespace sgcl::hash {
    class siphash;   // SipHash-2-4 with a key of 128 bits     value(): uint64_t
}
```

SipHash-2-4 (Aumasson and Bernstein, [*SipHash: a fast short-input PRF*](https://eprint.iacr.org/2012/351), 2012): a keyed hash with an argument behind it. For whoever does not have the key, its values are as good as random — even for one who picks the inputs and sees the outputs — so the buckets of a table hashed with it cannot be aimed at. It is the one hash of the module for keys that come from an adversary who may also see hashes; Python, Rust and Perl hash their tables' strings with it. It pays for that in speed: several times what [`maphash`](maphash.md) costs on a short key, and more on a long one.

It is not a hash for integrity or for passwords: 64 bits of output are a table's hash, not a message authentication code worth the name, and there is nothing slow in it. Those are `crypto`'s. Go's standard library has no SipHash (the runtime's is internal); the column Go below is `github.com/dchest/siphash`.

## Rules

- **The key is mandatory**: sixteen bytes, and no hasher is made without it — `siphash()` does not exist, and neither does `siphash::of(data)`. A program makes a key once from a source of randomness and keeps it secret; until `crypto` brings its generator, `std::random_device` is that source:

  ```cpp
  array<byte, 16> key;
  std::random_device device;
  for (auto& b : key) {
      b = byte(device());
  }
  ```

- **The key's bytes** are read as two little-endian words, as the paper and the reference implementation read them, so a key written down as bytes gives the reference's values.
- **The value** is the 64-bit result, as the paper writes it; `digest()` is, as every digest of the module, that number most significant byte first. The reference implementation and Go's `dchest/siphash` write the same number least significant byte first: to compare with their bytes, compare `value()`.
- **`reset()` keeps the key.** A hasher is eight words: the four of the state, the key, the length and the bytes of a word not yet complete.

## Members

```cpp
static constexpr size_t digest_size = 8;
static constexpr size_t block_size = 8;

explicit siphash(const array<byte, 16>& key) noexcept;

void update(const slice<const byte>& data) noexcept;    // and the text forms of the mixin
uint64_t value() const noexcept;
array<byte, 8> digest() const noexcept;      // the value, the most significant byte first
void reset() noexcept;                            // as new, with the same key

static uint64_t of(/* bytes or text */, const array<byte, 16>& key) noexcept;

expected<size_t, io::error> copy_from(const io::reader& r);  async::task<expected<size_t, io::error>> async_copy_from(const io::reader& r);
```

## Example

```cpp
#include "sgcl/core/range.h"
#include "sgcl/hash/siphash.h"
#include "sgcl/io/os.h"
#include "sgcl/txt/format.h"

using namespace sgcl;

int main() {
    // the key and the message of the paper's appendix: 00 01 … 0f, and 00 01 … 0e
    array<byte, 16> key;
    for (auto i : range(16)) {
        key[i] = byte(i);
    }
    array<byte, 15> message;
    for (auto i : range(15)) {
        message[i] = byte(i);
    }
    io::stdout.write(txt::format("{:016x}\n", hash::siphash::of(message, key)));

    // in pieces, the same value
    hash::siphash h(key);
    h.update("hello, ");
    h.update("world");
    io::stdout.write(txt::format("{}\n", h.value() == hash::siphash::of("hello, world", key)));
}
```

Output:

```text
a129ca6149be45e5
true
```

## SGCL and Go

| Go (`github.com/dchest/siphash`) | sgcl::hash | note |
|---|---|---|
| `siphash.New(key)` | `siphash(key)` | a key of sixteen bytes |
| `siphash.Hash(k0, k1, p)` | `siphash::of(p, key)` | the key as bytes: k0 its first eight, little-endian |
| `h.Sum64()` | `h.value()` | |
| `h.Sum(nil)` | `h.digest()` | the same number, the other way round: big-endian here, as every digest of the module |
| `siphash.New128`, `Hash128` | — | the 128-bit variant is not here |

## See also

[The module](README.md); [`maphash`](maphash.md), the fast hash for a table whose hashes stay in the process; [`xxh3_64`](xxh3.md) for values written down.
