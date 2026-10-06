[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::null

```cpp
static asn1 null() noexcept;
```

A NULL, `05 00`: the parameters of RSA's algorithm identifier, which X.509 writes as NULL.

## Parameters

None.

## Return value

The element.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1 rsa = encoding::asn1::sequence({
        encoding::asn1::object_identifier(encoding::asn1::oid("1.2.840.113549.1.1.1")), encoding::asn1::null()});
    println(encoding::hex::encode(rsa.bytes()));
    println(rsa[1].is(encoding::asn1::type::null));
}
```

Output:

```text
300d06092a864886f70d0101010500
true
```

## See also

- [is](is.md): whether an element is a NULL
- [sgcl::encoding::asn1](README.md)
