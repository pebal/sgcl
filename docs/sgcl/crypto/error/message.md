[sgcl](../../README.md) › [crypto](../README.md) › [error](README.md)

# sgcl::crypto::error::message

```cpp
string message() const noexcept;
```

Returns the error as a short text in English, for a person: the text the error was made with when it has one, the
code's own words otherwise (those of [crypto_category](../crypto_category.md)), with `offset N: ` in front when the
[offset](offset.md) is not 0.

## Parameters

None.

## Return value

The text: `"message authentication failed"`, `"offset 17: malformed data"`, `"offset 4: DER: length past the end"`.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 8439's key and nonce; the last byte of the tag changed
    crypto::chacha20_poly1305 aead(encoding::hex::decode(
        "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f"));
    auto nonce = encoding::hex::decode("070000004041424344454647");
    auto sealed = aead.seal(nonce, "attack at dawn");
    sealed.back() ^= std::byte{1};
    auto opened = aead.open(nonce, sealed);
    println("{}", opened.error().message());

    auto pem = crypto::x25519::private_key::from_pem("no key here");
    println("{}", pem.error().message());
}
```

Output:

```text
message authentication failed
PEM: no private key block
```

## See also

- [code](code.md): the code, for a program
- [offset](offset.md): where in the input
- [sgcl::crypto::error](README.md)
