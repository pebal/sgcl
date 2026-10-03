[sgcl](../../README.md) › [crypto](../README.md) › [hmac](../hmac.md)

# sgcl::crypto::hmac\<H\>::update

```cpp
void update(const slice<const byte>& data) noexcept;
```

Hashes the bytes of `data` into the message, after everything hashed before: a message fed in pieces of any length
has the tag of the whole. The slice takes what bytes come in: a `vector<byte>`, an `array<byte, N>`, a
`secret_bytes`, a slice of them. The bytes go into the inner digest, which started from the key's inner pad.

The text forms — a `string`, a text slice, a literal, a C string, a `std::string_view` — and a `std::span` of bytes
are the mixin's ([hash::mixin::hasher](../../hash/mixin/hasher.md)), each hashing the UTF-8 bytes where they lie.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes of the message |

## Return value

None.

## Complexity

Linear in `data.size()`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 4231, test case 1: a key of twenty bytes 0x0b
    vector<byte> key(20, byte(0x0b));
    crypto::hmac_sha256 mac(key);
    mac.update("Hi ");
    mac.update("There");
    println(encoding::hex::encode(mac.value()));
}
```

Output:

```text
b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7
```

## See also

- [value](value.md): the tag of what was hashed in
- [hash::mixin::hasher](../../hash/mixin/hasher.md): the text forms, `copy_from` for a stream
- [sgcl::crypto::hmac\<H\>](../hmac.md)
