[sgcl](../../README.md) › [crypto](../README.md) › [aes_kw](README.md)

# sgcl::crypto::aes_kw::unwrap

```cpp
[[nodiscard]] expected<secret_bytes, error> unwrap(const slice<const byte>& wrapped) const;
```

Unwraps a key [wrap](wrap.md) wrapped: the six passes run back, and the register compared in constant time with the
value it started as. A wrapped key under another key-encryption key, or with any byte changed, does not come out.

## Parameters

| Parameter | Description |
|---|---|
| `wrapped` | the wrapped key, 24 bytes or more and a multiple of 8 |

## Return value

The key, `wrapped.size() - 8` bytes, as a [secret_bytes](../secret_bytes/README.md); or an
[error](../error/README.md): `errc::malformed` for a length that cannot be a wrapped key, `errc::authentication`
when the check does not come out. `[[nodiscard]]`: an unwrap whose result is dropped hides the check.

## Complexity

Linear in `wrapped.size()`: 6 n AES decryptions.

## Exceptions

`logic_error` when the object was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::aes_kw wrapper(encoding::hex::decode("000102030405060708090a0b0c0d0e0f"));
    vector<byte> wrapped = encoding::hex::decode("1fa68b0a8112b447aef34bd8fb5a7b829d3e862371d2cfe5");
    println(encoding::hex::encode(wrapper.unwrap(wrapped).value()));

    wrapped[10] ^= byte(1);
    println("{}", wrapper.unwrap(wrapped).error().message());
}
```

Output:

```text
00112233445566778899aabbccddeeff
sgcl::crypto::aes_kw: the wrapped key does not check: another key-encryption key, or changed
```

## See also

- [wrap](wrap.md): the wrapped key
- [unwrap_padded](unwrap_padded.md): RFC 5649's
- [sgcl::crypto::aes_kw](README.md)
