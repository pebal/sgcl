[sgcl](../../README.md) › [crypto](../README.md) › [otp_key](README.md)

# sgcl::crypto::otp_key::verify

```cpp
[[nodiscard]] optional<uint64_t> verify(const string& code) const;
```

Whether `code` is a code of the key: [totp::verify](../totp/verify.md) at now for a TOTP key, the time step that
matched; [hotp::verify](../hotp/verify.md) from `counter` for an HOTP key, the counter that matched, which the
server stores plus one into `counter`. In constant time, with the key's secret and options.

## Parameters

| Parameter | Description |
|---|---|
| `code` | the code given |

## Return value

The time step or the counter that matched, or `nullopt`. `[[nodiscard]]`: a check whose result is dropped was never
made.

## Complexity

Linear in `options.skew`.

## Exceptions

`std::invalid_argument` when the key's options are out of range.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::otp_key key("otpauth://hotp/x?secret=GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ&counter=0");
    if (auto match = key.verify("755224")) {
        key.counter = *match + 1;
    }
    println("{}", key.counter);
}
```

Output:

```text
1
```

## See also

- [code](code.md): the code
- [sgcl::crypto::otp_key](README.md)
