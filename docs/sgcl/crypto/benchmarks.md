[sgcl](../README.md) › [crypto](README.md)

# Benchmarks: crypto

The setup, the machine and how the timers are read are described with [the benchmarks of the engine](../../garbage_collector/benchmarks.md). The module's cases are in `benchmarks/crypto/` (`bench_crypto`, `bench_curve25519`, `bench_ecc`, `bench_mlkem`, `bench_rsa`, `bench_x509`), each against OpenSSL (the system's libcrypto, 3.6.3, linked by the benchmarks only), one case and one side a process; Go's counterparts, where Go has them, are in `benchmarks/go/`. This page has ML-KEM so far.

## ML-KEM (FIPS 203)

`bench_mlkem <case> <sgcl|openssl>` and `benchmarks/go/mlkem` (Go 1.27.1's `crypto/mlkem`, which has no ML-KEM-512), the same seed on every side; nanoseconds per operation, each process timed for about two seconds after a quarter of a second thrown away; three rounds, the sides alternated case by case, the median and the smallest and largest of the three (Apple silicon, 2026-09-29, a load of about 3 from the user's own programs; the run of 2026-09-27 on a quiet machine gave every side 3 to 4 % less, the ratios the same). The SGCL column is from a run on 4 October 2026 at `-O3`, three processes a case in a clean environment (`env -i`; the shell of the earlier runs set `MallocNanoZone=0`, which turns macOS's nano allocator off); the OpenSSL and Go columns are from the run of 2026-09-29, not run again, and the ratios set the new medians against theirs:

- `keygen`: a decapsulation key from its seed of 64 bytes, and its encapsulation key (`decapsulation_key::from_seed` and `encapsulation_key()`, against `EVP_PKEY_generate` with the "seed" parameter and `EVP_PKEY_get_raw_public_key`, against `NewDecapsulationKey768`);
- `import`: an encapsulation key read from its bytes (`encapsulation_key::from_bytes`, its check of §7.2 and the matrix Â it makes, against `EVP_PKEY_new_raw_public_key_ex`, against `NewEncapsulationKey768`);
- `encaps`: an encapsulation to a key read once, 32 random bytes from the system each time on every side;
- `decaps`: the decapsulation of one ciphertext.

| Case | SGCL | OpenSSL | Go | SGCL / OpenSSL | SGCL / Go |
|---|---|---|---|---|---|
| keygen512 | 13 927 (13 873–13 931) | 19 948 (19 924–20 025) | — | 0.70 | — |
| keygen768 | 21 121 (21 111–21 182) | 30 750 (30 686–30 809) | 35 342 (35 178–35 380) | 0.69 | 0.60 |
| keygen1024 | 32 453 (32 402–32 900) | 44 692 (44 608–44 857) | 54 245 (53 947–54 563) | 0.73 | 0.60 |
| import512 | 5 920 (5 920–5 929) | 6 680 (6 471–6 708) | — | 0.89 | — |
| import768 | 11 684 (11 679–11 692) | 11 258 (11 043–11 402) | 10 760 (10 758–10 993) | 1.04 | 1.09 |
| import1024 | 19 275 (19 210–19 344) | 17 606 (17 574–17 706) | 18 179 (18 095–18 249) | 1.09 | 1.06 |
| encaps512 | 9 371 (9 335–9 395) | 14 878 (14 860–14 878) | — | 0.63 | — |
| encaps768 | 12 535 (12 532–12 778) | 21 475 (21 440–21 660) | 30 371 (29 936–30 949) | 0.58 | 0.41 |
| encaps1024 | 16 925 (16 829–16 932) | 29 094 (28 862–29 919) | 47 556 (47 419–47 793) | 0.58 | 0.36 |
| decaps512 | 14 380 (14 378–14 387) | 23 495 (23 406–23 544) | — | 0.61 | — |
| decaps768 | 19 210 (19 170–19 261) | 33 556 (33 183–33 568) | 45 338 (45 038–45 505) | 0.57 | 0.42 |
| decaps1024 | 25 661 (25 636–25 687) | 44 924 (44 819–45 112) | 72 068 (71 869–72 076) | 0.57 | 0.36 |

The code is the portable one: plain C++ that the compiler vectorizes where it can, no intrinsics in the transform (NEON for it is deferred: the table above leaves it little to win, and intrinsics would need the constant-time review again). SHAKE and SHA-3 run on the processor's SHA-3 instructions.

**Where the time of an encapsulation went.** A first version made the matrix Â — k² polynomials sampled from SHAKE128 — in every encapsulation and every decapsulation, as the standard writes it; a profile of ML-KEM-1024's encapsulation gave that sampling 43 % of the time, and the encapsulation was 5 to 21 % slower than OpenSSL's. OpenSSL and Go make the matrix once, when a key is read, and keep it in the key; so do the keys here now (the encapsulation key is 2, 4.5 or 8 KB for that). The same series before and after:

| Case | Before | After | After / before |
|---|---|---|---|
| keygen512 / 768 / 1024 | 14 681 / 22 254 / 33 729 | 14 888 / 22 660 / 34 234 | 1.01 / 1.02 / 1.01 |
| import512 / 768 / 1024 | 2 523 / 3 426 / 4 439 | 6 086 / 11 335 / 18 759 | 2.41 / 3.31 / 4.23 |
| encaps512 / 768 / 1024 | 15 119 / 23 012 / 34 024 | 11 584 / 15 013 / 19 741 | 0.77 / 0.65 / 0.58 |
| decaps512 / 768 / 1024 | 19 310 / 29 074 / 42 372 | 15 738 / 21 144 / 28 201 | 0.82 / 0.73 / 0.67 |

The cost moved to reading a key, where OpenSSL and Go pay it too: for a key used once, reading it and encapsulating costs what it did (ML-KEM-768 26.4 µs before, 26.3 µs after; ML-KEM-1024 38.5 µs both); every further encapsulation to the same key is the gain.

**The random bytes.** An encapsulation takes 32 bytes from [random](random/README.md). When that was a call into the system each time, fixing the message instead (the derandomized form the tests have) made an encapsulation 1.3 to 1.5 µs faster in every set (512: 15 146 → 13 640 ns, 768: 23 064 → 21 551, 1024: 34 027 → 32 749, measured before the matrix was kept). Since 2026-09-27 `random` is a ChaCha20 generator in the process, one per thread, seeded from the system (DESIGN 273), as OpenSSL's and Go's are: in the run of 2026-09-29 the encapsulations were 4 to 10 % under the run of 2026-09-27 while every other case was 3 to 4 % over it with the load.

## random

[random](random/README.md) is a ChaCha20 generator in the process, one per thread, seeded from the system. A request of 32
bytes costs about 21 ns, where a system call cost 1.5 µs and OpenSSL's `RAND_bytes` takes 200 ns, so a key, a nonce,
an ML-KEM encapsulation or an ECDSA signature no longer pays a trip to the kernel. Large requests run at the speed of
ChaCha20, about 2.4 GB/s on one core.

## The AEADs without the instructions

On the portable road AES and GHASH are computed bitsliced and on integer products, both in constant time, and two
orders of magnitude slower than on the processor's instructions; ChaCha20-Poly1305 needs no instructions of its own,
so on a machine without AES instructions [chacha20_poly1305](chacha20_poly1305/README.md) is the faster AEAD by far.
