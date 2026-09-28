# Benchmarks: the crypto module

The setup, the machine and how the timers are read are described with [the benchmarks of the engine](../../garbage_collector/benchmarks.md). The module's cases are in `benchmarks/crypto/` (`bench_crypto`, `bench_curve25519`, `bench_ecc`, `bench_mlkem`, `bench_rsa`, `bench_x509`), each against OpenSSL (the system's libcrypto, 3.6.3, linked by the benchmarks only), one case and one side a process; Go's counterparts, where Go has them, are in `benchmarks/go/`. This page has ML-KEM so far.

## ML-KEM (FIPS 203)

`bench_mlkem <case> <sgcl|openssl>` and `benchmarks/go/mlkem` (Go 1.27.1's `crypto/mlkem`, which has no ML-KEM-512), the same seed on every side; nanoseconds per operation, each process timed for about two seconds after a quarter of a second thrown away; three rounds, the sides alternated case by case, the median and the smallest and largest of the three (Apple silicon, 2026-09-27, the machine quiet):

- `keygen`: a decapsulation key from its seed of 64 bytes, and its encapsulation key (`decapsulation_key::from_seed` and `encapsulation_key()`, against `EVP_PKEY_generate` with the "seed" parameter and `EVP_PKEY_get_raw_public_key`, against `NewDecapsulationKey768`);
- `import`: an encapsulation key read from its bytes (`encapsulation_key::from_bytes`, its check of §7.2 and the matrix Â it makes, against `EVP_PKEY_new_raw_public_key_ex`, against `NewEncapsulationKey768`);
- `encaps`: an encapsulation to a key read once, 32 random bytes from the system each time on every side;
- `decaps`: the decapsulation of one ciphertext.

| case | SGCL | OpenSSL | Go | SGCL / OpenSSL | SGCL / Go |
|---|---|---|---|---|---|
| keygen512 | 14 888 (14 868–14 888) | 19 122 (19 106–19 219) | — | 0.78 | — |
| import512 | 6 086 (6 062–6 094) | 6 440 (6 430–6 443) | — | 0.95 | — |
| encaps512 | 11 584 (11 560–11 588) | 14 387 (14 373–14 394) | — | 0.81 | — |
| decaps512 | 15 738 (15 738–15 745) | 22 656 (22 655–22 682) | — | 0.69 | — |
| keygen768 | 22 660 (22 647–22 704) | 29 391 (29 386–29 413) | 34 101 (34 038–34 242) | 0.77 | 0.66 |
| import768 | 11 335 (11 280–12 022) | 10 870 (10 827–11 494) | 10 391 (10 379–10 499) | 1.04 | 1.09 |
| encaps768 | 15 013 (15 013–15 053) | 20 670 (20 652–20 698) | 29 911 (29 793–29 914) | 0.73 | 0.50 |
| decaps768 | 21 144 (21 049–21 147) | 32 210 (32 206–32 213) | 43 779 (43 766–43 912) | 0.66 | 0.48 |
| keygen1024 | 34 234 (34 203–34 249) | 43 027 (42 983–43 055) | 52 048 (52 027–52 084) | 0.80 | 0.66 |
| import1024 | 18 759 (18 754–18 768) | 16 907 (16 902–16 947) | 17 452 (17 439–17 595) | 1.11 | 1.07 |
| encaps1024 | 19 741 (19 672–19 811) | 28 233 (28 216–28 288) | 45 703 (45 693–45 774) | 0.70 | 0.43 |
| decaps1024 | 28 201 (28 175–28 212) | 43 359 (43 328–43 364) | 69 047 (69 026–69 052) | 0.65 | 0.41 |

The code is the portable one: plain C++ that the compiler vectorizes where it can, no intrinsics in the transform (NEON for it is deferred: the table above leaves it little to win, and intrinsics would need the constant-time review again). SHAKE and SHA-3 run on the processor's SHA-3 instructions.

**Where the time of an encapsulation went.** A first version made the matrix Â — k² polynomials sampled from SHAKE128 — in every encapsulation and every decapsulation, as the standard writes it; a profile of ML-KEM-1024's encapsulation gave that sampling 43 % of the time, and the encapsulation was 5 to 21 % slower than OpenSSL's. OpenSSL and Go make the matrix once, when a key is read, and keep it in the key; so do the keys here now (the encapsulation key is 2, 4.5 or 8 KB for that). The same series before and after:

| case | before | after | after / before |
|---|---|---|---|
| keygen512 / 768 / 1024 | 14 681 / 22 254 / 33 729 | 14 888 / 22 660 / 34 234 | 1.01 / 1.02 / 1.01 |
| import512 / 768 / 1024 | 2 523 / 3 426 / 4 439 | 6 086 / 11 335 / 18 759 | 2.41 / 3.31 / 4.23 |
| encaps512 / 768 / 1024 | 15 119 / 23 012 / 34 024 | 11 584 / 15 013 / 19 741 | 0.77 / 0.65 / 0.58 |
| decaps512 / 768 / 1024 | 19 310 / 29 074 / 42 372 | 15 738 / 21 144 / 28 201 | 0.82 / 0.73 / 0.67 |

The cost moved to reading a key, where OpenSSL and Go pay it too: for a key used once, reading it and encapsulating costs what it did (ML-KEM-768 26.4 µs before, 26.3 µs after; ML-KEM-1024 38.5 µs both); every further encapsulation to the same key is the gain.

**The system's random bytes.** An encapsulation takes 32 bytes from [`random`](random.md), a call into the system each time: with the message fixed instead (the test's derandomized form, `detail::mlkem::Access::encapsulate_with`) an encapsulation was 1.3 to 1.5 µs faster in every set (512: 15 146 → 13 640 ns, 768: 23 064 → 21 551, 1024: 34 027 → 32 749, measured before the matrix was kept). OpenSSL and Go draw theirs from a generator in the process, seeded from the system; a generator of that kind for `random` is a separate topic, not yet done.
