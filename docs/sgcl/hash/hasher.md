# sgcl::hash::mixin::hasher, sgcl::hash::req::hasher

```cpp
#include "sgcl/hash/mixin/hasher.h"   // or "sgcl/hash/hash.h"

namespace sgcl::hash {
    namespace mixin {
        template<class Derived> class hasher;   // the common shape over Derived::update(bytes)
    }
    namespace req {
        template<class H> concept hasher;       // H derives from mixin::hasher<H>
    }
}
```

The common shape of every hasher, as a [mixin](../core/mixin/README.md) over one primitive: `Derived::update(const slice<const byte>&)`, which takes bytes in and never fails. The mixin gives the rest of what the [module's page](README.md#one-shape-for-every-algorithm) lists — `update` for text, `of`, `copy_from` — and the class declares what only it knows: `digest_size`, `block_size`, `value()`, `digest()`, `reset()`. Every hasher of this module is one, and `crypto`'s will be.

## Members

```cpp
// From the mixin
void update(const string& text) noexcept;
void update(const slice<const char>& text) noexcept;
void update(const slice<char>& text) noexcept;
void update(const char (&text)[N]) noexcept;             // a literal or a char array: up to the first NUL or the end
void update(const char* text) noexcept;                  // a C string (char* too); an array never decays to it
void update(std::string_view text) noexcept;             // a std::string goes through its view
void update(const array<byte, N>& digest) noexcept; // another hasher's digest()
void update(std::span<const byte> bytes) noexcept;  // and std::span<byte>

static auto of(const auto& data) noexcept;               // whatever update() takes: T h; h.update(data); return h.value();
static auto of(const auto& data, const auto&... args) noexcept;   // with a seed or a key: only for a class with its own _of (below)

expected<size_t, io::error> copy_from(const io::reader& r);                       // r to its end: the bytes read, or r's error
async::task<expected<size_t, io::error>> async_copy_from(const io::reader& r);    // the same in a task

// From the class
static constexpr size_t digest_size;                   // bytes of digest()
static constexpr size_t block_size;
void update(const slice<const byte>& data) noexcept;
/* uint32_t, uint64_t or array<byte, 16> */ value() const noexcept;
array<byte, digest_size> digest() const noexcept;  // value() as bytes, the most significant first
void reset() noexcept;
```

The text overloads hash the bytes of the text where they lie, as UTF-8 and without normalizing. An array of `char` is read up to its first NUL or to its end, whichever comes first, so a buffer with no NUL in it is never read past, and a literal with a NUL inside is cut there, as a `std::string_view` made from it would be; the bytes of such a literal go in as a `std::string_view` with its length, or as bytes. two texts that read the same but are written differently hash differently, and [`txt::hash_normalized`](../txt/normalize.md) is the hash for that. The literal's own overload is also what keeps it from being ambiguous between the string and the slice.

A text goes to `update` as a slice with no owner: three words, no barrier and no registration of the thread. That is sound because the caller holds the text for the whole call and no hasher keeps a slice after it; it matters because a copy of the string's owner would cost a short key more than its hash.

`copy_from` reads `r` into one managed block of io's copy size (the same as [`io::copy`](../io/stream.md)'s) and hands each read to `update` straight from the block. When `r` fails, what was read before the failure has been hashed, and the error is returned as it came.

## Writing a hasher

A class derives from `mixin::hasher` with itself as the argument, names the mixin's overloads with `using hasher::update;` (its own `update` would hide them), and declares `update` for bytes, `value`, `digest`, `reset`, `digest_size` and `block_size`. The [Example](#example) below is one, `xor8`, in full.

## The requirement

`req::hasher` is nominal, as the [requirements of containers](../core/mixin/README.md) are: a type is a hasher because it said so by deriving from `mixin::hasher<T>` with itself as `T`, not because it happens to have an `update`. So a class derived from `crc32` is not one: it derives from `mixin::hasher<crc32>`, not from `mixin::hasher<itself>`, and a function asking for `req::hasher` does not take it; it takes the `crc32` it is made of, `static_cast<hash::crc32&>(d)`. A function over any hasher takes it that way:

```cpp
using namespace sgcl;

bool matches(hash::req::hasher auto h, const slice<const byte>& data, const slice<const byte>& expected) {
    h.update(data);
    auto d = h.digest();
    return expected.size() == d.size() && std::equal(d.begin(), d.end(), expected.begin());
}
```

## A hasher with a seed or a key

A class whose hash takes an argument — a seed, a key — gives `of` that argument after the data by declaring a one-shot of its own, a private static `_of(const slice<const byte>& data, arguments...)`, and making the mixin its friend. `of(data, arguments...)` then calls it with the bytes of whatever form the data came in, with no hasher made, and `of` with an argument exists for no other class: a CRC going on from a value is `crc32::resume(v)`, not a seed, and `crc32::of(data, v)` does not compile. [`xxh3_64`](xxh3.md), [`maphash`](maphash.md) and [`siphash`](siphash.md) are made so; `siphash` declares no `_of` without a key, so `siphash::of(data)` does not compile either.

## Example

```cpp
#include "sgcl/hash/crc32.h"
#include "sgcl/io/os.h"
#include "sgcl/txt/format.h"

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
    static_assert(hash::req::hasher<xor8>);
    io::stdout.write(txt::format("{:02x}\n", xor8::of("abc")));
    io::stdout.write(txt::format("{} {}\n", digest_size_of(xor8(), "abc"), digest_size_of(hash::crc32(), "abc")));
}
```

Output:

```text
60
1 4
```

## See also

[The module](README.md); [mixins](../core/mixin/README.md), the pattern this is one of.
