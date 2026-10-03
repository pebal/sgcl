[sgcl](../../README.md) › [crypto](../README.md) › [error](README.md)

# sgcl::crypto::error::error

```cpp
error() = default;                                                   // (1)
explicit error(errc code) noexcept;                                  // (2)
error(errc code, const string& detail) noexcept;                     // (3)
error(x509::reason why, const string& detail) noexcept;              // (4)
error(errc code, uint64_t offset) noexcept;                          // (5)
error(errc code, uint64_t offset, const string& detail) noexcept;    // (6)
```

Constructs an error. The module makes its errors itself; a program makes one for a function of its own that answers
in the module's terms, or to compare with one it was given.

1. `errc::malformed`, at no offset, with no text: what an error is before anything is assigned to it.
2. The code alone: [message](message.md) is the code's own words.
3. The code, and a text that [message](message.md) says in place of the code's own words.
4. A certificate chain that does not verify: `errc::verification`, the [reason](../x509-reason.md) why, and a text
   that names the certificate at fault.
5. The code at a byte of an encoded input (DER, PEM).
6. The code at a byte of an encoded input, and a text said in place of the code's own words.

The copy and the move are the implicit ones: an error is a value.

## Parameters

| Parameter | Description |
|---|---|
| `code` | what went wrong |
| `detail` | the text said in place of the code's own words |
| `why` | why the chain does not verify |
| `offset` | the byte of the encoded input where the error was found, from 0 |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", crypto::error().message());
    println("{}", crypto::error(crypto::errc::invalid_signature).message());
    println("{}", crypto::error(crypto::errc::unsupported, "a key of 1024 bits").message());
    crypto::error chain(crypto::x509::reason::expired, "expired: CN=example.com");
    println("{}", chain.message());
    println("{}", crypto::error(crypto::errc::malformed, 17).message());
    crypto::error at_byte(crypto::errc::malformed, 4, "DER: length past the end");
    println("{}", at_byte.message());
}
```

Output:

```text
malformed data
invalid signature
a key of 1024 bits
expired: CN=example.com
offset 17: malformed data
offset 4: DER: length past the end
```

## See also

- [message](message.md): the error as a text
- [errc](../errc.md): the codes
- [sgcl::crypto::error](README.md)
