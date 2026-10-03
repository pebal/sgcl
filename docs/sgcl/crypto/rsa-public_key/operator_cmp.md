[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [public_key](README.md)

# sgcl::crypto::rsa::operator==, operator!= (sgcl::crypto::rsa::public_key)

```cpp
friend bool operator==(const public_key& a, const public_key& b);
```

Compares two keys: equal when their moduli and their exponents are equal, wherever the keys came from (a
certificate, an encoding, a private key). `!=` is its negation, written by the compiler from `==`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the keys to compare |

## Return value

`true` when the keys are the same key, `false` otherwise.

## Complexity

Linear in the bits of the modulus.

## Exceptions

`logic_error` when either key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto pem = crypto::read_secret("tests/net/tls_testdata/rsa.key");  // the tree's test key
    crypto::rsa::private_key key = crypto::rsa::private_key::from_pem(pem);

    auto text = io::read_text("tests/net/tls_testdata/rsa.pem");  // the test key's certificate
    auto cert = crypto::x509::certificate::from_pem(text);
    println("{}", cert->public_key().rsa() == key.public_key());
    auto other = crypto::rsa::private_key::generate(2048);
    println("{}", other.public_key() != key.public_key());
}
```

Output:

```text
true
true
```

## See also

- [modulus](modulus.md), [exponent](exponent.md): the numbers compared
- [sgcl::crypto::rsa::public_key](README.md)
