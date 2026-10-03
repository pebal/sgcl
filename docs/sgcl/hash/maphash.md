[sgcl](../README.md) › [hash](README.md)

# sgcl::hash::maphash

```cpp
#include "sgcl/hash/maphash.h"   // or "sgcl/hash.h"

namespace sgcl::hash {
    class maphash;  // seeded once per process
}
```

`sgcl::hash::maphash` is the hash of a hash table in memory, Go's `hash/maphash`: seeded with a random seed drawn
once per process, so that which keys share a bucket is not known outside the process and is different in the next
run. Its values are for the process that made them. The algorithm behind them is not promised — today it is
[XXH3-64](xxh3_64.md) with the process's seed — and may change in any version, so a value written to a file or sent
away is the job of `xxh3_64`, whose values are fixed.

A seeded hash is the defence Go's and Rust's tables have: collisions cannot be computed once and sent to every
process. It is not a proof. Values that leave the process next to their keys can give the seed away, and a table
whose hashes an adversary may see takes [siphash](siphash.md), which is built to hold under that.

A [string](../core/string.md) keeps a keyed hash of its own, the one `std::hash<sgcl::string>` and the library's
containers take, and it is not `maphash`: a string hashes itself in one call and keeps the result in its header,
which a hasher fed in pieces cannot do. A table keyed by strings needs nothing from this page. `maphash` is for a
key the library has no hash for — a struct of numbers and names — and for a key hashed in pieces.

## Rules

- **One seed a process**, not one a hasher (Go gives each zero `maphash.Hash` a seed of its own): every `maphash`
  of a process agrees with every other, with no seed passed around, and `maphash::of(key)` is a function of the key
  alone for the life of the process.
- **Where the seed comes from**: the key of the library's own hash of strings (a [string](../core/string.md) hashes
  its bytes with a key drawn once from `std::random_device`), through one folded product of two of its words, so
  that the key is not read back from the seed. One draw of entropy a process; the environment variable
  `SGCL_HASH_SEED`, a number, fixes that key and so this seed too: the same values, and the same order of a table,
  in every run.
- **An explicit seed**, `maphash(seed)` and `maphash::of(data, seed)`, gives the same values in every run and every
  process, for a test; the values are today's algorithm's.
- **`update_value(v)`** hashes the bytes of a value as they lie in memory, for a key that is not text. It is here
  and nowhere else in the module, because a value of `maphash` never leaves the process, where the order of bytes
  would matter.
- **`reset()` keeps the seed.** A hasher is 552 bytes, as an `xxh3_64` is, a plain value; `of` makes none.
- **Nothing fails** but `copy_from` and `of_file`, which read a stream and a file and return its error.

### From code written for Go

| With Go | With sgcl::hash |
|---|---|
| `var h maphash.Hash`, a random seed of its own | `hash::maphash h;`, the process's seed: every hasher agrees |
| `maphash.MakeSeed()`, `h.SetSeed(s)` | `hash::maphash h(seed);`: a seed is a number; there is no random seed per hasher |
| `maphash.Bytes(seed, b)`, `maphash.String(seed, s)` | `maphash::of(b, seed)`, `maphash::of(s, seed)`; with the process's seed `maphash::of(b)`, `maphash::of(s)` |
| `maphash.Comparable(seed, v)`, `h.WriteComparable(v)` | `h.update_value(v)`, only for types every byte of which is the value |
| `h.Sum64()`, `h.Reset()` | `h.value()`, `h.reset()` |

## Member objects

| Constant | Value | Description |
|---|---|---|
| `digest_size` | `8` | the size of [digest](maphash/digest.md) in bytes, Go's `Size()`; `static constexpr size_t` |
| `block_size` | `64` | a stripe of XXH3, the algorithm of today, Go's `BlockSize()`; `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](maphash/maphash.md) | a hasher of no bytes yet, with the process's seed or a seed of its own |

#### Modifiers

| Function | Description |
|---|---|
| [update](maphash/update.md) | hashes bytes |
| [update_value](maphash/update_value.md) | hashes the bytes of a value as they lie in memory |
| [reset](maphash/reset.md) | puts the hasher back as it was made, with its seed |

#### Observers

| Function | Description |
|---|---|
| [value](maphash/value.md) | the hash of the bytes so far |
| [digest](maphash/digest.md) | the hash as bytes, the most significant first |

#### From mixin::hasher

The rest of the shape every hasher shares ([mixin::hasher](mixin/hasher.md)).

| Function | Description |
|---|---|
| [update](mixin/hasher/update.md) | hashes a text, a digest or a std::span of bytes |
| [copy_from, async_copy_from](mixin/hasher/copy_from.md) | hashes a stream to its end |
| [of](mixin/hasher/of.md) | the hash of bytes or a text in one call, with the process's seed or a seed (static) |
| [of_file, async_of_file](mixin/hasher/of_file.md) | the hash of a whole file (static) |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

struct point {
    int32_t x;
    int32_t y;
    string name;

    bool operator==(const point&) const = default;
};

// A key of numbers and a name: the numbers as they lie, the name as its bytes
struct point_hash {
    size_t operator()(const point& p) const noexcept {
        hash::maphash h;
        h.update_value(p.x);
        h.update_value(p.y);
        h.update(p.name);
        return h.value();
    }
};

int main() {
    map<point, int, point_hash> heights;
    heights[point{3, 4, "hill"}] = 120;
    heights[point{3, 5, "peak"}] = 870;
    println("{}", heights[point{3, 5, "peak"}]);

    // every maphash of the process agrees, in pieces or in one call
    hash::maphash h;
    h.update("hel");
    h.update("lo");
    println("{}", h.value() == hash::maphash::of("hello"));
}
```

Output:

```text
870
true
```

## See also

- [xxh3_64](xxh3_64.md): the fixed hash beneath it today
- [siphash](siphash.md): keys from an adversary
- [string](../core/string.md): whose own hash is keyed the same way
- [mixin::hasher](mixin/hasher.md): the shape every hasher shares
- [sgcl::hash](README.md)
