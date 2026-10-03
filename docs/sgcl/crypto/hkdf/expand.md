[sgcl](../../README.md) › [crypto](../README.md) › [hkdf](../hkdf.md)

# sgcl::crypto::hkdf\<H\>::expand

```cpp
/*(1)*/ static secret_bytes expand(const prk& key, const slice<const byte>& info, size_t n);
/*(2)*/ static secret_bytes expand(const slice<const byte>& key, const slice<const byte>& info,
                                   size_t n);
```

`n` bytes of output keying material from a pseudorandom key, bound to `info`, the second step of RFC 5869:
`T(1) || T(2) || ...` cut to `n`, where `T(i) = HMAC(PRK, T(i-1) || info || i)`. Two calls with different infos give
unrelated keys, so one PRK serves independent uses.

1. The PRK is what [extract](extract.md) gave.
2. The PRK is given as bytes: a secret a protocol computed otherwise, or one written down from
   [prk::bytes](../hkdf-prk/bytes.md). RFC 5869 asks for at least the digest's size, which is the caller's to keep.

The output is a [secret_bytes](../secret_bytes.md): up to 64 bytes in the object itself, past that in plain memory
zeroed when it goes, never in managed memory. The last block computed is zeroed before the call returns.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the pseudorandom key |
| `info` | what the output is for; may be empty |
| `n` | the number of bytes, at most `max_size` |

## Return value

The `n` bytes.

## Complexity

Linear in `n`: one HMAC of a block for every `H::digest_size` bytes.

## Exceptions

`std::invalid_argument` when `n` is greater than `max_size`, 255 blocks of the digest.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 5869, test case 1
    vector<byte> ikm(22, byte(0x0b));
    auto salt = encoding::hex::decode("000102030405060708090a0b0c");
    auto info = encoding::hex::decode("f0f1f2f3f4f5f6f7f8f9");
    auto master = crypto::hkdf_sha256::extract(salt, ikm);
    println(encoding::hex::encode(crypto::hkdf_sha256::expand(master, info, 42)));

    // the same PRK as bytes
    auto written = encoding::hex::decode(
        "077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5");
    println(encoding::hex::encode(crypto::hkdf_sha256::expand(written, info, 42)));

    try {
        crypto::hkdf_sha256::expand(master, info, crypto::hkdf_sha256::max_size + 1);
    } catch (const std::invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865
3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865
sgcl::crypto::hkdf: more than 255 blocks asked of expand
```

## See also

- [expand_to](expand_to.md): into a buffer of the caller's
- [extract](extract.md): the first step
- [sgcl::crypto::hkdf\<H\>](../hkdf.md)
