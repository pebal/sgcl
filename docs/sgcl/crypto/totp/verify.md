[sgcl](../../README.md) › [crypto](../README.md) › [totp](README.md)

# sgcl::crypto::totp::verify

```cpp
[[nodiscard]] static optional<uint64_t> verify(const slice<const byte>& secret, const string& code,     // (1)
                                               const otp_options& o = {});
[[nodiscard]] static optional<uint64_t> verify(const slice<const byte>& secret, const string& code,     // (2)
                                               const time::datetime& at, const otp_options& o = {});
```

The time step whose code `code` is, among the step of now (1) or of `at` (2) and the `o.skew` steps either side of
it; or nothing. A server keeps the step it accepted and refuses the same code, or an older one, again: a code is good
once. Every code of the window is computed and compared in constant time, with no early exit.

## Parameters

| Parameter | Description |
|---|---|
| `secret` | the shared secret |
| `code` | the code given |
| `at` | the time to check at |
| `o` | the algorithm, the digits, the period and the skew ([otp_options](../otp_options.md)) |

## Return value

The time step that matched, the Unix time divided by the period, or `nullopt` when none did, when `code` is not
`o.digits` decimal digits, or when `at` is before the Unix epoch. `[[nodiscard]]`: a check whose result is dropped was
never made.

## Complexity

Linear in `o.skew`: one HMAC a step of the window.

## Exceptions

`std::invalid_argument` when `o` is out of range.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    time::datetime now = time::datetime::from_unix(1111111111);
    time::datetime late = time::datetime::from_unix(1111111111 + 30);
    string code = crypto::totp::generate("12345678901234567890", now);
    println("{}", crypto::totp::verify("12345678901234567890", code, late).value());  // one period late: still good
    println("{}", crypto::totp::verify("12345678901234567890", code, late, {.skew = 0}).has_value());
}
```

Output:

```text
37037037
false
```

## See also

- [generate](generate.md): the code
- [sgcl::crypto::totp](README.md)
