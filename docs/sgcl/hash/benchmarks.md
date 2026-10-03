[sgcl](../README.md) › [hash](README.md)

# Benchmarks: hash

The setup, the machine, the environments and how the timers are read are described with
[the benchmarks of the engine](../../garbage_collector/benchmarks.md). `bench_hash` (`benchmarks/hash/hash.cpp`) and
its Go counterpart `benchmarks/go/hash` hold the same cases under the same names, over the same bytes and with the
same timing: one case and one length a process, a call `type::of(data)` over `length` bytes of splitmix64 output,
the calls independent of each other (the result summed into a sink, the pointer read through a volatile so that
nothing is hoisted), run for about two seconds after a quarter of a second thrown away. Each prints the
nanoseconds per call and the gigabytes per second. `benchmarks/compare.sh` runs them side by side (`CASES=hash`)
at the lengths 16, 64, 1024, 65536 and 1048576 bytes.

## The cases

| Case | What is measured |
|---|---|
| `adler32` | Adler-32; Go's `adler32.Checksum` |
| `combine` | `crc32::combine` over a second piece of `length` bytes: one multiplication modulo the polynomial per set bit of the length (SGCL only) |
| `crc32`, `crc32c`, `crc64`, `crc64_iso` | the CRCs on the path the build has; Go's `crc32.Checksum` and `crc64.Checksum` with the table made once |
| `crc32-portable`, `crc64-portable` | slicing by eight, the portable path, in the same build (SGCL only) |
| `fnv32`, `fnv32a`, `fnv64`, `fnv64a`, `fnv128`, `fnv128a` | FNV; Go's hasher made once and `Reset` before each call, `Write` and `Sum`, which is how Go offers it |
| `maphash` | the process's seed, its secret made once; Go's `maphash.Bytes` with a seed made once |
| `siphash` | SipHash-2-4 with a fixed key; Go's `github.com/dchest/siphash` |
| `string-hash` | the keyed hash a [string](../core/string/README.md) of core keeps of its bytes, one call, for comparing with `maphash` (SGCL only; 16 to 65536 bytes) |
| `xxh3_64`, `xxh3_128` | XXH3 with the seed 0, the default secret; Go's `github.com/zeebo/xxh3`, which has NEON assembly on arm64 |
| `xxh3_64-seeded` | XXH3 with a seed: past 240 bytes the seed's secret is made in each call |

## The paths

The CRCs. On arm64 they fold the data 64 bytes at a time with carry-less multiplication (PMULL), four
independent lanes, and use the CRC-32 instructions of ARMv8 for inputs under 128 bytes. The processor is asked
once whether it has both (a constant on macOS, whose default target promises them), so no build flag is needed
and a program may mix files built with different `-march`. On x86-64 the same folding runs on PCLMULQDQ for inputs
of 128 bytes and more, where the processor has it (asked once); the 16 bytes the fold leaves and the tail go by
slicing by eight, as the CRC-32 instruction of SSE 4.2 knows Castagnoli's polynomial alone. The portable path is
slicing by eight: tables the compiler computes from the polynomial, 8 KB for a 32-bit CRC and 16 KB for a 64-bit
one. `crc32-portable` and `crc64-portable` measure it on any machine; it is the path x86-64 takes under 128 bytes.
Folding wins even where there is an instruction: the CRC-32 instruction takes eight bytes and needs the register
the last one left, and four lanes of sixteen do not wait for each other. There is no CRC-64 instruction at all:
the 64-bit CRCs fold inputs of 128 bytes and more as the 32-bit ones do, and slicing by eight does the rest, over
tables of 16 KB per polynomial. The paths give the same result for every input; the tests hold them against each
other and against Go.

Adler-32 goes in blocks of 32 bytes. Over a block, `a` grows by the sum of its bytes and `b` by 32 times the `a` it
came in with plus the bytes weighted 32 down to 1, and both sums are independent of each other and of the running
values, so the inner loop is two plain reductions the compiler vectorizes by itself; nothing there is an
intrinsic. The modulo is taken once every 5536 bytes, the largest multiple of 32 within zlib's bound of 5552, where
every sum still fits 32 bits even with every byte 0xFF (a `static_assert` in the header works the worst case out).
It is cheaper than a CRC where there is no CRC instruction, at the price of weak checking on short inputs.

FNV is one multiplication a byte by its definition, each waiting for the last, and no path makes it faster; the
128-bit FNVs multiply by their prime as the state times `0x13b` plus the state shifted by 88 bits. SipHash is
rounds of additions, rotations and XORs, one word at a time, with nothing to vectorize: it costs several times
what `maphash` costs on a short key, and more on a long one, the price of a hash whose buckets cannot be aimed at.

XXH3. Inputs up to 240 bytes take one of six paths by length, a multiplication or a few; longer ones go through
eight lanes of 64 bits, a stripe of 64 bytes at a time. On arm64 the lanes are four NEON vectors: the compiler
keeps the loop's eight products scalar, and the vectors are markedly faster (measured side by side with the plain
loop). On x86-64 they are four SSE2 vectors, or two AVX2 ones where the processor has AVX2 (asked once; the AVX2
road compiled, unverified until the x86 machine). Past 240 bytes XXH3 reads a secret of 192 bytes made from the
seed: a hasher makes it once, when its first stripe goes in, and `of` with a seed makes it on its stack in each call
(24 additions), which `xxh3_64-seeded` measures. With `SGCL_HASH_PORTABLE` defined, the CRCs and XXH3 take the
plain loops; the tests hold the paths against each other and all of them against the oracles.

## See also

- [sgcl::hash](README.md)
- [Benchmarks: crypto](../crypto/benchmarks.md): the cryptographic hashes
