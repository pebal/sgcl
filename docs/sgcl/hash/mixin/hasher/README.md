[sgcl](../../../README.md) › [hash](../../README.md) › mixin

# sgcl::hash::mixin::hasher\<Derived\>

```cpp
#include "sgcl/hash/mixin/hasher.h"   // or "sgcl/hash.h"

namespace sgcl::hash::mixin {
    template<class Derived>
    class hasher;
}
```

`sgcl::hash::mixin::hasher<Derived>` is the shape every hasher shares, a [mixin](../../../core/mixin/README.md) over
one primitive of the class that carries it: `Derived::update(const slice<const byte>&)`, which takes bytes in and
never fails. The mixin gives the rest, the same for every algorithm, so that a reader learns it once:
[update](update.md) for text and for the other forms bytes come in, [of](of.md), the hash of data in
one call, [copy_from](copy_from.md), a stream read to its end into the hasher, and
[of_file](of_file.md), the hash of a whole file. The class declares what only it knows: `digest_size`,
`block_size`, `value()`, `digest()` and `reset()`.

By deriving from it a class also says that it is a hasher, which is what [req::hasher](../../req/hasher.md) asks.
Every hasher of this module carries it, and so does every digest of [crypto](../../../crypto/README.md) (`sha256`,
`sha3_256`, `hmac<sha256>`…), so a function written over `hash::req::hasher` takes a CRC and a SHA-256 alike. It is
what Go's interface `hash.Hash` is (with `hash.Hash32` and `hash.Hash64`), made static: no virtual function, no
state of its own, its constructor and destructor protected so that it exists only as a base.

## Rules

- **What the class declares.** A class derives from `mixin::hasher` with itself as the argument, names the mixin's
  overloads with `using hasher::update;` (its own `update` would hide them), and declares `update` for bytes,
  `value`, `digest`, `reset`, `digest_size` and `block_size`. The [Example](#example) is one, `xor8`, in full.
- **`value()`** is the result in its natural type: `uint32_t` or `uint64_t` where it fits a number,
  `array<byte, 16>` for 128 bits. It ends nothing: `update` may go on after it, as with Go's `Sum`.
- **`digest()`** is the result as bytes, the most significant first, as Go's `Sum` writes it, for code that takes
  any hasher. A format that stores a checksum the other way round (gzip, zip and xz do) writes `value()` in its own
  order.
- **`digest_size` and `block_size`** are the sizes Go calls `Size` and `BlockSize` and Python `digest_size` and
  `block_size`, static constants of the class.
- **A seed or a key.** A class whose hash takes an argument gives `of` that argument after the data by declaring a
  one-shot of its own, a private static `_of(const slice<const byte>& data, arguments...)`, and making the mixin
  its friend. `of(data, arguments...)` then calls it with the bytes of whatever form the data came in, with no
  hasher made, and `of` with an argument exists for no other class. [xxh3_64](../../xxh3_64/README.md),
  [xxh3_128](../../xxh3_128/README.md), [xxh32](../../xxh32/README.md), [xxh64](../../xxh64/README.md), [maphash](../../maphash/README.md) and [siphash](../../siphash/README.md) are made so; `siphash` declares
  no `_of` without a key and cannot be made without one, so `siphash::of(data)` and `siphash::of_file(path)` do not
  compile.
- **A plain value.** A hasher of the module is trivially copyable, with no pointer inside and nothing for the
  collector: it lives on a stack, in a field, in a managed object. A copy is a branch, Go's `Clone`: a common
  prefix hashed once, then two ways.
- **Not a writer.** A hasher is not an [io::writer](../../../io/writer/README.md): a writer is a managed object with a
  virtual `write` and a coroutine behind it, which an update of a few bytes would pay for on every call.
  `copy_from` is the bridge to streams instead.
- **Nothing allocates** but `async_copy_from`, which takes a managed block of io's copy size for the task, and
  `of_file` and `async_of_file`, which open the file as io does.
- **Everything is `noexcept`** but `copy_from` and `of_file`, which read through a stream or a file and pass on
  what a read throws; a failure of the stream or the file is an `expected`, never an exception.
- **The name of the module.** A file with both `using namespace std;` and `using namespace sgcl;` that writes a
  bare `hash<int>` finds the namespace `sgcl::hash` beside the template `std::hash` and is ambiguous;
  `std::hash<int>` is not, and the pages of this module use `sgcl` alone.

### From code written for Go

| With Go | With sgcl::hash |
|---|---|
| `hash.Hash`, `hash.Hash32`, `hash.Hash64` | `mixin::hasher<Derived>`, asked for as `req::hasher`: a value with `update`, `value`, `digest`, `reset`; not a writer, `copy_from` reads a stream |
| `h.Write(p)` | `h.update(p)`: bytes or text; never fails |
| `h.Sum32()`, `h.Sum64()` | `h.value()`, in the natural type; `array<byte, 16>` for 128 bits |
| `h.Sum(nil)` | `h.digest()`, the same big-endian bytes as Go's |
| `h.Reset()`, `h.Size()`, `h.BlockSize()` | `h.reset()`, `digest_size`, `block_size` |
| `Clone` (`hash.Cloner`) | a copy |
| `io.Copy(h, r)` | `h.copy_from(r)`, `co_await h.async_copy_from(r)`: any stream, a file, a connection, a lambda |
| `MarshalBinary`, `UnmarshalBinary` | a copy within the process; across processes `resume(v)` of the CRCs, Adler-32 and the FNVs, whose state is their value |

## Template parameters

| Parameter | Description |
|---|---|
| `Derived` | The class that carries the mixin and names itself as the argument (`class crc32 : public hash::mixin::hasher<crc32>`). It declares `update(const slice<const byte>&)`, `value()`, `digest()`, `reset()`, `digest_size` and `block_size`. |

## Member functions

| Function | Description |
|---|---|
| `(constructor)`, `(destructor)` | protected: the mixin exists only as a base |

#### Hashing

| Function | Description |
|---|---|
| [update](update.md) | hashes a text, a digest or a std::span of bytes |
| [copy_from, async_copy_from](copy_from.md) | hashes a stream to its end |

#### One call

| Function | Description |
|---|---|
| [of](of.md) | the hash of bytes or a text in one call (static) |
| [of_file, async_of_file](of_file.md) | the hash of a whole file (static) |

## Example

```cpp
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

class xor8 : public hash::mixin::hasher<xor8> {
public:
    using hasher::update;

    static constexpr size_t digest_size = 1;
    static constexpr size_t block_size = 1;

    void update(const slice<const byte>& data) noexcept {
        for (auto b : data) {
            _x ^= uint8_t(b);
        }
    }

    uint8_t value() const noexcept {
        return _x;
    }

    array<byte, 1> digest() const noexcept {
        return {byte(_x)};
    }

    void reset() noexcept {
        _x = 0;
    }

private:
    uint8_t _x = 0;
};

// Any hasher: how many bytes its digest has
size_t digest_size_of(hash::req::hasher auto h, const string& text) {
    h.update(text);
    return h.digest().size();
}

int main() {
    println("{}", hash::req::hasher<xor8>);
    println("{:02x}", xor8::of("abc"));
    println("{} {}", digest_size_of(xor8(), "abc"), digest_size_of(hash::crc32(), "abc"));
}
```

Output:

```text
true
60
1 4
```

## See also

- [req::hasher](../../req/hasher.md): what a function asks for to take any hasher
- [crc32](../../crc32/README.md), [xxh3_64](../../xxh3_64/README.md), [siphash](../../siphash/README.md): hashers that carry it
- [the mixins of core](../../../core/mixin/README.md): the pattern this is one of
- [sgcl::hash](../../README.md)
