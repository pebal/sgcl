# sgcl::hash::maphash

```cpp
#include "sgcl/hash/maphash.h"   // or "sgcl/hash/hash.h"

namespace sgcl::hash {
    class maphash;   // the hash of a table in the process, seeded once per process     value(): uint64_t
}
```

The hash for a hash table in memory, Go's `hash/maphash`: fast on short keys and long ones, and seeded with a random seed drawn once per process, so which keys share a bucket is not known outside the process and is different in the next run. Its values are for the process that made them. The algorithm behind them is not promised — today it is [XXH3-64](xxh3.md) with the process's seed — and may change in any version, so a value written to a file or sent away is the job of `xxh3_64`, whose values are fixed.

A seeded fast hash is the defence Go's and Rust's tables have: collisions cannot be computed once and sent to every process. It is not a proof. Values that leave the process next to their keys can give the seed away, and a table whose hashes an adversary may see takes [`siphash`](siphash.md), which is built to hold under that.

## Rules

- **One seed a process**, not one a hasher (Go gives each zero `maphash.Hash` a seed of its own): every `maphash` in a process agrees with every other, with no seed passed around, and `maphash::of(key)` is a function of the key alone for the life of the process.
- **Where the seed comes from**: the key of the library's own hash of strings ([`string`](../core/string.md) hashes its bytes with a key drawn once from `std::random_device`), through one folded product of two of its words, so that the key is not read back from the seed. One draw of entropy a process; the environment variable `SGCL_HASH_SEED`, a number, fixes that key and so fixes this seed too: the same values, and the same order of a table, in every run.
- **An explicit seed**, `maphash(seed)` and `maphash::of(data, seed)`, gives the same values in every run and every process, for a test.
- **`update_value(v)`** hashes the bytes of a value as they lie in memory, in the machine's order: the parts of a key that are not text, with nothing written out first. It is here and nowhere else in the module, because a maphash value never leaves the process, where the order of bytes would matter. It takes only a type every byte of which is its value (`std::has_unique_object_representations`): not a struct with padding, whose padding bytes are anything, and not a `float` or `double`, whose `+0` and `−0` are equal and differ in their bytes. Such a key goes in field by field.
- **`reset()` keeps the seed.** A hasher is about 550 bytes, as an `xxh3_64` is; `of` makes none.

## Members

```cpp
static constexpr size_t digest_size = 8;
static constexpr size_t block_size = 64;

maphash() noexcept;                               // the process's seed
explicit maphash(uint64_t seed) noexcept;         // this seed, in every run

void update(const slice<const byte>& data) noexcept;    // and the text forms of the mixin
template<class T> void update_value(const T& value) noexcept;   // T with unique object representations
uint64_t value() const noexcept;
array<byte, 8> digest() const noexcept;
void reset() noexcept;                            // as new, with the seed it was made with

static uint64_t of(/* bytes or text */) noexcept;                   // the process's seed
static uint64_t of(/* bytes or text */, uint64_t seed) noexcept;

expected<size_t, io::error> copy_from(const io::reader& r);  async::task<expected<size_t, io::error>> async_copy_from(const io::reader& r);
```

## Beside a string's own hash

[`string`](../core/string.md) keeps a keyed hash of its own, the one `std::hash<sgcl::string>` and the library's containers take, and it is not `maphash`: a string hashes itself in one call and keeps the result in its header, which a hasher fed in pieces cannot do. A table keyed by strings needs nothing from this page. `maphash` is for a key the library has no hash for — a struct of numbers and names — and for a key hashed in pieces.

## Example

```cpp
#include "sgcl/core/map.h"
#include "sgcl/core/string.h"
#include "sgcl/hash/maphash.h"
#include "sgcl/io/os.h"
#include "sgcl/txt/format.h"

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
    io::stdout.write(txt::format("{}\n", heights[point{3, 5, "peak"}]));

    // every maphash of the process agrees, in pieces or in one call
    hash::maphash h;
    h.update("hel");
    h.update("lo");
    io::stdout.write(txt::format("{}\n", h.value() == hash::maphash::of("hello")));

    // an explicit seed: the same number in every run
    io::stdout.write(txt::format("{:016x}\n", hash::maphash::of("hello", 42)));
}
```

Output:

```text
870
true
bafa072f07db7937
```

The last line is what XXH3-64 gives today; a later version may print another number, as the algorithm is not promised.

## SGCL and Go

| Go | sgcl::hash | note |
|---|---|---|
| `var h maphash.Hash` (a random seed of its own) | `maphash h` | the process's seed: every hasher agrees |
| `maphash.MakeSeed()`, `h.SetSeed(s)` | `maphash(seed)` | a seed is a number; there is no random seed per hasher |
| `maphash.Bytes(seed, b)`, `maphash.String(seed, s)` | `maphash::of(b, seed)`, `maphash::of(s)` | |
| `maphash.Comparable(seed, v)`, `WriteComparable` | `update_value(v)` | only for types every byte of which is the value |
| `h.Sum64()`, `h.Reset()` | `value()`, `reset()` | |

## See also

[The module](README.md); [`xxh3_64`](xxh3.md), the fixed hash beneath it today; [`siphash`](siphash.md) for keys from an adversary; [`string`](../core/string.md), whose own hash is keyed the same way.
