[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [name](../x509-name.md)

# sgcl::crypto::x509::operator==, operator!= (sgcl::crypto::x509::name)

```cpp
friend bool operator==(const name& a, const name& b) noexcept;
```

Compares two names: equal when they have the same attributes in the same order, each of the same OID, the same
value and the same kind. The values are compared as text, so a PrintableString and a UTF8String of the same characters
are equal, while a chain is built on the bytes of the names. `!=` is its negation, written by the compiler from `==`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the names to compare |

## Return value

`true` when the names are equal, `false` otherwise.

## Complexity

Linear in the length of the attributes.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto text = io::read_text("tests/net/tls_testdata/rsa.pem");  // a leaf of the tree's test CA
    crypto::x509::certificate cert = crypto::x509::certificate::from_pem(text);

    auto root_text = io::read_text("tests/net/tls_testdata/ca.pem");  // the tree's test CA
    crypto::x509::certificate root = crypto::x509::certificate::from_pem(root_text);

    println("{}", cert.issuer() == root.subject());
    println("{}", cert.subject() != root.subject());
}
```

Output:

```text
true
true
```

## See also

- [certificate::raw_subject](../x509-certificate/raw_subject.md): the bytes chains compare
- [sgcl::crypto::x509::name](../x509-name.md)
