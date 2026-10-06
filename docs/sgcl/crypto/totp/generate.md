[sgcl](../../README.md) › [crypto](../README.md) › [totp](README.md)

# sgcl::crypto::totp::generate

```cpp
static string generate(const slice<const byte>& secret, const otp_options& o = {});                              // (1)
static string generate(const slice<const byte>& secret, const time::datetime& at, const otp_options& o = {});    // (2)
```

The TOTP code under `secret`: the [hotp](../hotp/README.md) code of the Unix time divided by `o.period`.

1. Of now, by the system's clock.
2. Of the time `at`.

## Parameters

| Parameter | Description |
|---|---|
| `secret` | the shared secret, bytes or text |
| `at` | the time, not before the Unix epoch |
| `o` | the algorithm, the digits and the period ([otp_options](../otp_options.md)) |

## Return value

The code, `o.digits` decimal digits.

## Complexity

Constant: one HMAC.

## Exceptions

`std::invalid_argument` when `o` is out of range, or `at` is before the Unix epoch.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", crypto::totp::generate("12345678901234567890").size());
    println("{}", crypto::totp::generate("12345678901234567890", time::datetime::from_unix(59), {.digits = 8}));
}
```

Output:

```text
6
94287082
```

## See also

- [verify](verify.md): the check
- [sgcl::crypto::totp](README.md)
