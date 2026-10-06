[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::octet_string

```cpp
static asn1 octet_string(const slice<const byte>& bytes) noexcept;
```

An OCTET STRING of the bytes, copied: a digest, a nonce, an encrypted content. A string of any
size is written in one piece, as DER requires.

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | the content |

## Return value

The element.

## Complexity

Linear in the size of the bytes.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1 e = encoding::asn1::octet_string(string("abc"));
    println(encoding::hex::encode(e.bytes()));
    println("{} bytes", e.as_bytes()->size());
}
```

Output:

```text
0403616263
3 bytes
```

## See also

- [as_bytes](as_bytes.md): the bytes read
- [sgcl::encoding::asn1](README.md)
