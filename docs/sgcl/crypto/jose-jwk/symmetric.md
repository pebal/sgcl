[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk](README.md)

# sgcl::crypto::jose::jwk::symmetric

```cpp
static jwk symmetric(const slice<const byte>& key);
static jwk symmetric(const slice<const byte>& key, const options& o);
```

A symmetric key (`"kty":"oct"`) of the program's octets, copied into plain memory: an HMAC key of HS256, the key of
an AES-GCM key wrap, a `dir` content key. Its use follows from its length and `o.alg`: a key of 16, 24 or 32 bytes
encrypts with the AES key wrap of its size unless `o.alg` says `dir`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the octets, bytes or text |
| `o` | `kid`, `alg` and `use` ([options](../jose-jwk-options.md)) |

## Return value

The key.

## Complexity

Linear in the length of the key.

## Exceptions

`std::invalid_argument` for an empty key.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::jose::jwk::symmetric("a secret of thirty-two bytes ...");
    auto token = crypto::jose::jws::sign("hello", key);
    println("{}", token);
}
```

Output:

```text
eyJhbGciOiJIUzI1NiJ9.aGVsbG8.O4ynl7k9pP-PlbGt96lFBpFMJOYAJZZKGB2u4BJR_Tc
```

## See also

- [generate](generate.md): random octets of an algorithm's length
- [sgcl::crypto::jose::jwk](README.md)
