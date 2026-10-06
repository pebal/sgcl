[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [authentication_results](README.md)

# sgcl::net::smtp::authentication_results::authentication_results

```cpp
authentication_results() = default;                      // (1)
explicit authentication_results(const string& value);    // (2)
```

1. No receiver and no results.
2. The value a literal spells, as [parse](parse.md) reads it; what parse refuses is thrown (DESIGN 234).

## Parameters

| Parameter | Description |
|---|---|
| `value` | a field's value |

## Complexity

- (1) Constant.
- (2) Linear in the value.

## Exceptions

- (1) None.
- (2) `bad_expected_access<io::error>` with [parse](parse.md)'s error.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    net::smtp::authentication_results ar("mx.example.org; spf=pass smtp.mailfrom=example.com");
    println("{} {}", ar.authserv_id, ar.results[0].method);
}
```

Output:

```text
mx.example.org spf
```

## See also

- [parse](parse.md)
- [authentication_results](README.md)
