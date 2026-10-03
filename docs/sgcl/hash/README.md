[sgcl](../README.md) › hash

# sgcl::hash

```cpp
#include "sgcl/hash.h"   // namespace sgcl::hash
```

Checksums and hashes that are not cryptographic: what Go has in `hash/crc32`, `hash/crc64`, `hash/adler32`,
`hash/fnv` and `hash/maphash`, and XXH3 and SipHash-2-4, which Go leaves to other packages. The module depends on
[core](../core/README.md) (the `array` a digest is among its containers) and [io](../io/README.md) (only for
`copy_from` and `of_file`, which read a stream and a file); [compress](../compress/README.md) (CRC-32 and Adler-32
for gzip, zip and zlib) and [crypto](../crypto/README.md) (the same shape for SHA-2 and the rest) are built on it.
The index of the whole interface is [the modules](../README.md).

Every algorithm is one type, and every type has the same methods, so a reader learns them once: `update` with the
bytes or the text as they come, `value()` for the result in its natural type, `digest()` for the same as bytes,
`reset()`, and `T::of(data)` for the whole thing in one call. The shape is [mixin::hasher](mixin/hasher/README.md), which
every type of the module and of `crypto` carries, and [req::hasher](req/hasher.md) asks a type for it, so code
written over any hasher takes a CRC as it takes `crypto::sha256`.

A hasher is a plain value, trivially copyable, with no pointer inside and nothing for the collector: four bytes for
a CRC, sixteen for the widest FNV, 72 for SipHash, 552 for XXH3 and maphash, which keep a
buffer of four stripes and a seed's secret. It lives on the stack, in a field, in a managed object, and a copy of
it is a branch, Go's `Clone`: a common prefix hashed once, then two ways.

## The rules

1. **One shape.** `update` takes bytes as a [slice](../core/slice/README.md)`<const byte>` and text as its UTF-8 bytes
   ([update](mixin/hasher/update.md)); it never fails and never throws. `value()` ends nothing: `update` may go on
   after it, as with Go's `Sum`. `digest()` is most significant byte first, as Go's `Sum` writes it; gzip, zip and
   xz store their CRC the other way round, and a format writes `value()` in its own order. `of(data)` is
   `T h; h.update(data); return h.value();`, the only one-shot form: no free function per algorithm, no template
   over the algorithm. A type with a seed or a key takes it after the data (`xxh3_64::of(data, seed)`,
   `siphash::of(data, key)`), and only those types take a second argument. `digest_size` and `block_size` are the
   sizes Go calls `Size` and `BlockSize` and Python `digest_size` and `block_size`.
2. **Nothing in the module allocates,** except the task forms of [copy_from](mixin/hasher/copy_from.md) and
   [of_file](mixin/hasher/of_file.md), which take one managed block of io's copy size for the call (their reads may
   run on the pool; `copy_from` and `of_file` keep the block on their stack), and `of_file`, which opens the file
   as io does. Both read the stream to its end, `of_file` without the file held in memory, and return the error of the stream or of the file in an
   [expected](../core/expected/README.md). A hasher is not an `io::writer`: a writer is a managed object with a virtual
   `write` and a coroutine behind `write`, which an update of a few bytes would pay for on every call; `copy_from`
   is the bridge instead, Go's `io.Copy(h, r)`.
3. **Which one.** A format that names its checksum takes that one (gzip and zip a CRC-32, zlib Adler-32, xz a
   CRC-64). A hash written down or sent away, where speed matters, is `xxh3_64` or `xxh3_128`, whose values will not
   change. A hash table in memory takes `maphash`, or nothing at all when its keys are strings: a
   [string](../core/string/README.md) hashes itself with a key of the process and keeps the result. Keys chosen by someone
   who may also see the hashes take `siphash`, the one hash here with an argument that they cannot be aimed at.
   None of them is for integrity against an attacker or for passwords: that is [crypto](../crypto/README.md).
4. **The CRCs carry the names of the [CRC catalogue](https://reveng.sourceforge.io/crc-catalogue/),** not Go's,
   where the two part: Go's `crc64.ECMA` is the catalogue's CRC-64/XZ, and the catalogue has a CRC-64/ECMA-182 as
   well, with another result for the same bytes. So the type is `crc64`, what xz, 7z and Go compute, and nobody
   checking the catalogue is sent to the wrong one. The four named CRCs are the only ones: Go's
   `crc32.MakeTable(poly)` with any polynomial has no counterpart, though the engine under them takes any reflected
   polynomial and a type for another is added when one is asked for.
5. **Two pieces, one checksum.** The CRCs and Adler-32 have `combine`: the checksum of A followed by B from the
   checksum of A, the checksum of B and the length of B, zlib's `crc32_combine` and `adler32_combine`, so a large
   buffer is hashed on several tasks and the pieces joined, as pigz and a zip written from many threads do. A
   checksum saved earlier is a start too: `resume(saved)` goes on from it, Go's `crc32.Update`. The state of the
   CRCs, Adler-32 and the FNVs is their value, so Go's `MarshalBinary` and `UnmarshalBinary` are a copy within a
   process and `resume(value)` across processes.
6. **Two names meet names from elsewhere.** A file with both `using namespace std;` and `using namespace sgcl;`
   that writes a bare `hash<int>` finds the namespace `sgcl::hash` beside the template `std::hash` and is
   ambiguous; `std::hash<int>` is not, and the programs of these pages use `sgcl` alone. And `crc32` and `adler32`
   are the names of zlib's functions: `hash::crc32` qualified does not collide with them, a bare `crc32` under
   `using namespace sgcl::hash;` in a file that includes `zlib.h` does.

## Classes

| Class | Header | Description |
|---|---|---|
| [adler32](adler32/README.md) | `adler32.h` | Adler-32 of RFC 1950, a `uint32_t`: zlib streams; `combine`, `resume` |
| [crc32](crc32/README.md) | `crc32.h` | CRC-32/ISO-HDLC, a `uint32_t`: zlib, gzip, zip, PNG, Ethernet; Go's `crc32.IEEE`; `combine`, `resume` |
| [crc32c](crc32c/README.md) | `crc32.h` | CRC-32/ISCSI (Castagnoli), a `uint32_t`: iSCSI, ext4, Btrfs, SCTP, LevelDB, RocksDB, gRPC; Go's `crc32.Castagnoli` |
| [crc64](crc64/README.md) | `crc64.h` | CRC-64/XZ, a `uint64_t`: xz, 7z; Go's `crc64.ECMA` |
| [crc64_iso](crc64_iso/README.md) | `crc64.h` | CRC-64/GO-ISO, a `uint64_t`: Go's `crc64.ISO` |
| [fnv128](fnv128/README.md) | `fnv.h` | FNV-1 of 128 bits, an `array<byte, 16>`: data somebody already hashed that way |
| [fnv128a](fnv128a/README.md) | `fnv.h` | FNV-1a of 128 bits, an `array<byte, 16>` |
| [fnv32](fnv32/README.md) | `fnv.h` | FNV-1 of 32 bits, a `uint32_t` |
| [fnv32a](fnv32a/README.md) | `fnv.h` | FNV-1a of 32 bits, a `uint32_t` |
| [fnv64](fnv64/README.md) | `fnv.h` | FNV-1 of 64 bits, a `uint64_t` |
| [fnv64a](fnv64a/README.md) | `fnv.h` | FNV-1a of 64 bits, a `uint64_t` |
| [maphash](maphash/README.md) | `maphash.h` | a hash for a table in memory, seeded once per process, its algorithm not promised (XXH3-64 today), a `uint64_t`; Go's `hash/maphash` |
| [siphash](siphash/README.md) | `siphash.h` | SipHash-2-4 with a key of 128 bits, a `uint64_t`: a table keyed by an adversary who may see its hashes |
| [xxh3_128](xxh3_128/README.md) | `xxh3.h` | XXH3 of 128 bits with a seed, an `array<byte, 16>`: where 64 bits collide too often (content identifiers); xxhsum `-H128` |
| [xxh3_64](xxh3_64/README.md) | `xxh3.h` | XXH3 of 64 bits with a seed, a `uint64_t`: a fast hash whose values are fixed, for files, caches on disk, protocols; xxhsum `-H3` |

## Mixins

| Mixin | Header | Description |
|---|---|---|
| [hasher](mixin/hasher/README.md) | `mixin/hasher.h` | the shape every hasher shares: the text overloads of `update`, `of`, `copy_from`, `of_file` and their task forms |

## Requirements

| Requirement | Header | Description |
|---|---|---|
| [hasher](req/hasher.md) | `mixin/hasher.h` | a type that carries `mixin::hasher`: every type of the module and crypto's digests |

## See also

- [Benchmarks](benchmarks.md): every algorithm against Go's packages, and the paths each takes on arm64 and x86-64
- [crypto](../crypto/README.md): the cryptographic hashes, of the same shape
- [compress](../compress/README.md): gzip, zip and zlib, which carry these checksums
- [The modules](../README.md)
