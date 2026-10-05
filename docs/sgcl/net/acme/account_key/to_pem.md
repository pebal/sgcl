[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [account_key](README.md)

# sgcl::net::acme::account_key::to_pem

```cpp
crypto::secret_bytes to_pem() const;
```

The key as PEM, `PRIVATE KEY` over its PKCS #8, as Go and OpenSSL write it: a `secret_bytes`, never managed memory,
for `io::write_file` with permissions 0600.

## Parameters

None.

## Return value

The PEM text's bytes.

## Complexity

Linear in the size of the key.

## Exceptions

None but running out of memory.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::account_key key;
    auto pem = key.to_pem();
    auto bytes = pem.as_slice();
    std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    println("{}", text.substr(0, 27));
}
```

Output:

```text
-----BEGIN PRIVATE KEY-----
```

## See also

- [from_pem](from_pem.md)
- [sgcl::net::acme::account_key](README.md)
