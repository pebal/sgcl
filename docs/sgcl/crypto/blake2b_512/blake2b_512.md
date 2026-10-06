[sgcl](../../README.md) › [crypto](../README.md) › [blake2b_512](README.md)

# sgcl::crypto::blake2b_512::blake2b_512

```cpp
blake2b_512() noexcept;                            // (1)
explicit blake2b_512(const blake2_options& o);     // (2)
blake2b_512(const blake2b_512& other) noexcept;    // (3)
```

The same three for `blake2b_384`, `blake2b_256`, `blake2s_256` and `blake2s_128`.

1. Makes a hasher that has hashed nothing, with no key, salt or personalization: its `value()` is the digest of the
   empty message.
2. Makes a hasher with the key, the salt and the personalization of `o` ([blake2_options](../blake2_options.md)), any
   of them empty. A key is the first block of the message, so a keyed hasher of nothing gives the MAC of the empty
   message, not the plain digest. The bytes of `o` are read here and not kept.
3. A copy: the hasher goes on from where `other` stood, keyed if `other` was; each zeroes its own state when keyed.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the key (up to `max_key_size` bytes), the salt and the personalization (up to 16 bytes each, 8 for BLAKE2s) |
| `other` | the hasher to copy |

## Complexity

Constant.

## Exceptions

- (1), (3) None.
- (2) `std::invalid_argument` when the key, the salt or the personalization is longer than its field.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::hex::encode(crypto::blake2b_256().value()));

    crypto::blake2s_256 mac({.key = "secret key"});
    mac.update("hello");
    println(encoding::hex::encode(mac.value()));

    try {
        vector<byte> long_key(33);
        crypto::blake2s_256 too_long({.key = long_key});
    } catch (const std::invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
0e5751c026e543b2e8ab2eb06099daa1d1e5df47778f7787faab45cdf12fe3a8
08a0539a20f89304b748bfba8aba716952839d7f7266f18ba3036683344c38cc
sgcl::crypto::blake2: a key longer than the hash takes
```

## See also

- [blake2_options](../blake2_options.md): what (2) takes
- [reset](reset.md): back to what the constructor made
- [sgcl::crypto::blake2b_512](README.md)
