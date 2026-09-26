# sgcl::hash

Checksums and hashes that are not cryptographic: what Go has in `hash/crc32`, `hash/crc64`, `hash/adler32`, `hash/fnv` and `hash/maphash`, and XXH3 and SipHash-2-4, which Go leaves to other packages. `#include "sgcl/hash/hash.h"` brings the module in; it depends on [`core`](../core/README.md) (the `array` a digest is among its containers) and [`io`](../io/README.md) (only for `copy_from`, which reads a stream), and `compress` (CRC-32 and Adler-32 for gzip, zip and zlib) and `crypto` (the same shape for SHA-2 and the rest) are to be built on it. The index of the whole interface is [`docs/sgcl/`](../README.md).

## One shape for every algorithm

Every algorithm is one type, and every type has the same methods, so a reader learns them once:

```cpp
using namespace sgcl;

uint32_t c = hash::crc32::of("123456789");        // 0xCBF43926: the whole thing in one call

hash::crc32 h;                                     // or in pieces, as they come
for (auto& chunk : chunks) {
    h.update(chunk);                               // bytes (slice<const byte>) or text
}
uint32_t sum = h.value();                          // the result; h goes on if more comes
auto bytes = h.digest();                           // the same as array<byte, 4>, most significant first
h.reset();                                         // as new
```

- **`update`** takes bytes as a [`slice<const byte>`](../core/slice.md) — a buffer of io, a `vector<byte>`, a `std::span` of bytes (const or not), a stack array, the digest of another hasher (`array<byte, N>`) — and text as a [`string`](../core/string.md), a `slice<const char>`, a literal or an array of `char`, a C string or a `std::string_view`, each hashed as its UTF-8 bytes where they lie. An array of `char` is read up to its first NUL or its end, so a literal with a NUL inside is cut there, as a `std::string_view` made from it would be. It never fails and never throws.
- **`value()`** is the result in its natural type: `uint32_t` or `uint64_t` where it fits a number, `array<byte, 16>` for the 128-bit FNVs. It ends nothing: `update` may go on after it, as with Go's `Sum`.
- **`digest()`** is the result as bytes, most significant first, as Go's `Sum` writes it, for code that takes any hasher and for the future `crypto::sha256`. gzip, zip and xz store their CRC the other way round; a format writes `value()` in its own order.
- **`of(data)`** is `T h; h.update(data); return h.value();`. A type with a seed or a key takes it after the data — `xxh3_64::of(data, seed)`, `siphash::of(data, key)` — and hashes in one call with no hasher made. It is the only one-shot form: no free function per algorithm, no template over the algorithm; and only those types take a second argument, so a CRC does not go on from a value through `of` (`resume(v)` does that; a one-argument constructor is a seed, for the types that have one).
- **A copy is a branch**, Go's `Clone`: a common prefix hashed once, then two ways.
- `digest_size` and `block_size` are the sizes Go calls `Size` and `BlockSize` and Python `digest_size` and `block_size`.

A hasher is a plain value, trivially copyable, no pointer inside and nothing for the collector: four bytes for a CRC, sixteen for the widest FNV, eight words for SipHash, about 550 bytes for XXH3 and maphash, which keep a buffer of four stripes and a seed's secret. It lives on the stack, in a field, in a managed object. Nothing in the module allocates, except `copy_from`, which takes one block of io for the call.

## The algorithms

| type | algorithm | `value()` | where it is found |
|---|---|---|---|
| [`crc32`](crc32.md) | CRC-32/ISO-HDLC | `uint32_t` | zlib, gzip, zip, PNG, Ethernet; Go's `crc32.IEEE` |
| [`crc32c`](crc32.md) | CRC-32/ISCSI (Castagnoli) | `uint32_t` | iSCSI, ext4, Btrfs, SCTP, LevelDB, RocksDB, gRPC; Go's `crc32.Castagnoli` |
| [`crc64`](crc64.md) | CRC-64/XZ | `uint64_t` | xz, 7z; Go's `crc64.ECMA` |
| [`crc64_iso`](crc64.md) | CRC-64/GO-ISO | `uint64_t` | Go's `crc64.ISO` |
| [`adler32`](adler32.md) | Adler-32 (RFC 1950) | `uint32_t` | zlib streams |
| [`fnv32`, `fnv32a`, `fnv64`, `fnv64a`](fnv.md) | FNV-1 and FNV-1a | `uint32_t`, `uint64_t` | data somebody already hashed that way |
| [`fnv128`, `fnv128a`](fnv.md) | FNV-1 and FNV-1a, 128 bits | `array<byte, 16>` | the same |
| [`xxh3_64`](xxh3.md) | XXH3, 64 bits, with a seed | `uint64_t` | a fast hash whose values are fixed: files, caches on disk, protocols; xxhsum `-H3` |
| [`xxh3_128`](xxh3.md) | XXH3, 128 bits, with a seed | `array<byte, 16>` | the same where 64 bits collide too often (content identifiers); xxhsum `-H128` |
| [`maphash`](maphash.md) | seeded once per process; not promised (XXH3-64 today) | `uint64_t` | a hash table in memory; Go's `hash/maphash` |
| [`siphash`](siphash.md) | SipHash-2-4, a key of 128 bits | `uint64_t` | a table keyed by an adversary who may see its hashes |

Which one: a format that names its checksum takes that one (gzip and zip a CRC-32, zlib Adler-32, xz a CRC-64). A hash written down or sent away, where speed matters, is `xxh3_64` or `xxh3_128`, whose values will not change. A hash table in memory takes `maphash`, or nothing at all when its keys are strings: a [`string`](../core/string.md) hashes itself with a key of the process and keeps the result. Keys chosen by someone who may also see the hashes take `siphash`, the one hash here with an argument that they cannot be aimed at. None of them is for integrity against an attacker or for passwords: that is `crypto`.

The CRCs carry the names of the [CRC catalogue](https://reveng.sourceforge.io/crc-catalogue/), not Go's, where the two part: Go's `crc64.ECMA` is the catalogue's CRC-64/XZ, and the catalogue has a CRC-64/ECMA-182 as well, with another result for the same bytes. So the type is `crc64`, what xz, 7z and Go compute, and nobody checking the catalogue is sent to the wrong one.

Two names meet names from elsewhere. A file with both `using namespace std;` and `using namespace sgcl;` that writes a bare `hash<int>` finds the namespace `sgcl::hash` beside the template `std::hash` and is ambiguous; `std::hash<int>` is not, and the examples here use `sgcl` alone. And `crc32` and `adler32` are the names of zlib's functions: `hash::crc32` qualified does not collide with them, a bare `crc32` under `using namespace sgcl::hash;` in a file that includes `zlib.h` does.

## Two pieces, one checksum

The CRCs and Adler-32 have `combine`: the checksum of A followed by B from the checksum of A, the checksum of B and the length of B. With it a large buffer is hashed on several tasks and the pieces joined, as pigz and a zip written from many threads do:

```cpp
using namespace sgcl;

async::task<uint32_t> parallel_crc(slice<const byte> data) {
    constexpr size_t parts = 4;
    size_t part = data.size() / parts;
    auto length = [&](size_t i) { return i + 1 == parts ? data.size() - i * part : part; };
    vector<async::task<uint32_t>> work;
    for (auto i : range(parts)) {
        auto piece = data.subslice(i * part, length(i));
        work.push_back(async::spawn([piece]() -> async::task<uint32_t> { co_return hash::crc32::of(piece); }));
    }
    auto sums = co_await async::when_all(std::move(work));
    uint32_t crc = sums[0];
    for (auto i : range(size_t(1), parts)) {
        crc = hash::crc32::combine(crc, sums[i], length(i));
    }
    co_return crc;
}
```

`combine` is static and works on values, since a CRC does not count its own length: counting it would cost every `update` an addition for the sake of a call made once per piece. A CRC saved earlier is also a start: `hash::crc32::resume(saved)` goes on from it, Go's `crc32.Update`.

## A stream to its end

```cpp
using namespace sgcl;

auto f = io::open("archive.bin");
if (!f) { /* f.error().message() */ }
hash::crc32 h;
auto n = h.copy_from(**f);                   // the bytes read, or the stream's error
if (n) {
    auto line = txt::format("{:08x}", h.value());
}

// in a task
auto m = co_await h.async_copy_from(**f);
```

`copy_from` is Go's `io.Copy(h, r)`: the stream read to its end in blocks of io's copy size, each handed to `update` where it lies. A hasher is not an [`io::writer`](../io/stream.md): a writer is a managed object with a virtual `write` and a coroutine behind `write`, which an update of a few bytes would pay for on every call. `copy_from` is the bridge instead.

## Paths

On arm64 the CRCs fold the data 64 bytes at a time with carry-less multiplication (PMULL), four independent lanes, and use the CRC-32 instructions of ARMv8 for inputs under 128 bytes; the default target of Apple's arm64 has both, so there is nothing to detect at run time and no flag to set. The portable path, and the one x86 takes today, is slicing by eight: tables the compiler computes from the polynomial, 8 KB for a 32-bit CRC and 16 KB for a 64-bit one. The x86 instructions (PCLMULQDQ and SSE 4.2's `crc32`) and run-time detection on Linux on arm64 come with the machines to test them on; until then x86 takes the portable path. Which path a program takes is decided per file from the target's flags, so a program is built for one target throughout: on Linux on arm64, a file built with `-march=armv8-a+crc+crypto` beside one built without would give the same inline function two bodies. Adler-32 is a loop over blocks of 32 bytes the compiler vectorizes; FNV is one multiplication a byte by its definition, and no path makes it faster. XXH3's long inputs go through eight lanes of 64 bits, which on arm64 are four NEON vectors: the compiler leaves that loop scalar, and the vectors are markedly faster (measured side by side with the plain loop). SipHash is rounds of additions, rotations and XORs, one word at a time, with nothing to vectorize.

## Pages

| page | header | what it is |
|---|---|---|
| [hasher](hasher.md) | `sgcl/hash/mixin/hasher.h` | `mixin::hasher<D>`: the text overloads of `update`, `of`, `copy_from`; `req::hasher` |
| [crc32](crc32.md) | `sgcl/hash/crc32.h` | `crc32`, `crc32c` |
| [crc64](crc64.md) | `sgcl/hash/crc64.h` | `crc64`, `crc64_iso` |
| [adler32](adler32.md) | `sgcl/hash/adler32.h` | `adler32` |
| [fnv](fnv.md) | `sgcl/hash/fnv.h` | `fnv32`, `fnv32a`, `fnv64`, `fnv64a`, `fnv128`, `fnv128a` |
| [xxh3](xxh3.md) | `sgcl/hash/xxh3.h` | `xxh3_64`, `xxh3_128` |
| [maphash](maphash.md) | `sgcl/hash/maphash.h` | `maphash` |
| [siphash](siphash.md) | `sgcl/hash/siphash.h` | `siphash` |

## SGCL and Go

| Go | sgcl::hash | note |
|---|---|---|
| `hash.Hash`, `hash.Hash32`, `hash.Hash64` | `mixin::hasher<D>`, `req::hasher` | a value with `update`, `value`, `digest`, `reset`; not a writer, `copy_from` reads a stream |
| `h.Write(p)` | `h.update(p)` | bytes or text; never fails |
| `h.Sum32()`, `h.Sum64()` | `h.value()` | the natural type; `array<byte, 16>` for 128 bits |
| `h.Sum(nil)` | `h.digest()` | big-endian bytes, the same as Go's |
| `h.Reset()`, `h.Size()`, `h.BlockSize()` | `h.reset()`, `digest_size`, `block_size` | |
| `Clone` (`hash.Cloner`) | a copy | |
| `crc32.ChecksumIEEE(p)`, `crc32.Checksum(p, crc32.MakeTable(crc32.Castagnoli))` | `crc32::of(p)`, `crc32c::of(p)` | |
| `crc32.Update(crc, tab, p)`, `crc64.Update` | `auto h = crc32::resume(crc); h.update(p)` | |
| `crc64.Checksum(p, crc64.MakeTable(crc64.ECMA))`, `crc64.ISO` | `crc64::of(p)`, `crc64_iso::of(p)` | the catalogue's names |
| `adler32.Checksum(p)` | `adler32::of(p)` | |
| `fnv.New32`, `New32a`, `New64`, `New64a`, `New128`, `New128a` | `fnv32`, `fnv32a`, `fnv64`, `fnv64a`, `fnv128`, `fnv128a` | |
| `maphash.Hash`, `maphash.Bytes(seed, b)`, `maphash.String` | `maphash`, `maphash::of(b)`, `maphash::of(b, seed)` | one seed a process, not one a hasher; [`maphash`](maphash.md#sgcl-and-go) has the rest |
| `maphash.Comparable`, `WriteComparable` | `maphash::update_value(v)` | for types every byte of which is the value |
| `xxh3.Hash`, `HashSeed`, `Hash128`, `Hash128Seed` (`github.com/zeebo/xxh3`) | `xxh3_64::of(b)`, `xxh3_64::of(b, seed)`, `xxh3_128::of(b)`, `xxh3_128::of(b, seed)` | the 128-bit value as sixteen bytes, the high half first |
| `siphash.Hash(k0, k1, p)` (`github.com/dchest/siphash`) | `siphash::of(p, key)` | `digest()` big-endian, where `Sum` is little-endian |
| `io.Copy(h, r)` | `h.copy_from(r)`, `co_await h.async_copy_from(r)` | any stream: a file, a connection, a lambda |
| — | `crc32::combine`, `crc32c::combine`, `crc64::combine`, `crc64_iso::combine`, `adler32::combine` | zlib's `crc32_combine` and `adler32_combine`, for all four CRCs |
| `crc32.MakeTable(poly)` with any polynomial | — | the four named CRCs only; the engine takes any reflected polynomial, a type for others when one is asked for |
| `MarshalBinary` / `UnmarshalBinary` | a copy in a process; the constructor from a value, for every type | the state of these algorithms is their value |
