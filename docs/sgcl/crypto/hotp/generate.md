[sgcl](../../README.md) › [crypto](../README.md) › [hotp](README.md)

# sgcl::crypto::hotp::generate

```cpp
static string generate(const slice<const byte>& secret, uint64_t counter, const otp_options& o = {});
```

The HOTP code of `counter` under `secret`: the HMAC of the counter as 8 bytes big-endian, truncated at the offset its
last byte gives to 31 bits, modulo 10^`digits`, written with its leading zeros.

## Parameters

| Parameter | Description |
|---|---|
| `secret` | the shared secret, bytes or text |
| `counter` | the counter |
| `o` | the algorithm and the digits ([otp_options](../otp_options.md)) |

## Return value

The code, `o.digits` decimal digits.

## Complexity

Constant: one HMAC.

## Exceptions

`std::invalid_argument` when `o` is out of range.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", crypto::hotp::generate("12345678901234567890", 1));
    println("{}", crypto::hotp::generate("12345678901234567890", 1, {.digits = 8}));
}
```

Output:

```text
287082
94287082
```

## See also

- [verify](verify.md): the check
- [sgcl::crypto::hotp](README.md)
