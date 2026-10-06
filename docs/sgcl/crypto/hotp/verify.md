[sgcl](../../README.md) › [crypto](../README.md) › [hotp](README.md)

# sgcl::crypto::hotp::verify

```cpp
[[nodiscard]] static optional<uint64_t> verify(const slice<const byte>& secret, uint64_t counter,
                                               const string& code, const otp_options& o = {});
```

The counter whose code `code` is, among `counter` (the one the server expects next) and the `o.skew` after it, RFC
4226 §7.4's look-ahead; or nothing. The server stores the match plus one as its next counter, so that no code is
accepted twice. Every code of the window is computed and compared in constant time, with no early exit.

## Parameters

| Parameter | Description |
|---|---|
| `secret` | the shared secret |
| `counter` | the counter the server expects |
| `code` | the code given |
| `o` | the algorithm, the digits and the look-ahead ([otp_options](../otp_options.md)) |

## Return value

The counter that matched, or `nullopt` when none did, or when `code` is not `o.digits` decimal digits.
`[[nodiscard]]`: a check whose result is dropped was never made.

## Complexity

Linear in `o.skew`: one HMAC a counter of the window.

## Exceptions

`std::invalid_argument` when `o` is out of range.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    uint64_t expected = 0;
    string code = crypto::hotp::generate("12345678901234567890", 2);  // the token is two ahead
    auto match = crypto::hotp::verify("12345678901234567890", expected, code, {.skew = 3});
    if (match) {
        expected = *match + 1;
    }
    println("{}", expected);
}
```

Output:

```text
3
```

## See also

- [generate](generate.md): the code
- [sgcl::crypto::hotp](README.md)
