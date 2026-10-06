[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwe](README.md)

# sgcl::crypto::jose::jwe::parse

```cpp
static expected<jwe, error> parse(const string& compact) noexcept;
```

A JWE read in the compact serialization: five parts of strict base64url, the first a JSON object with `alg` and
`enc`. Nothing is decrypted.

## Parameters

| Parameter | Description |
|---|---|
| `compact` | the JWE |

## Return value

The JWE, or an error: `errc::malformed` for other than five parts, a part that is not base64url, a header that is
not a JSON object or lacks `alg` or `enc`; `errc::unsupported` for a header with `crit`.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", crypto::jose::jwe::parse("a.b.c.d").error().message());
}
```

Output:

```text
sgcl::crypto::jose: JWE: a compact JWE is five parts
```

## See also

- [decrypt](decrypt.md)
- [sgcl::crypto::jose::jwe](README.md)
