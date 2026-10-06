[sgcl](../../README.md) › [crypto](../README.md) › [argon2](README.md)

# sgcl::crypto::argon2::verify

```cpp
[[nodiscard]] static expected<void, error> verify(const slice<const byte>& password, const string& phc,
                                                  const slice<const byte>& secret = {}) noexcept;
```

Whether `password` is the one the PHC string `phc` was made from: the variant, the costs and the salt read from the
string, the hash computed again and compared with the string's in constant time. Any of the three variants is read,
of version 19 (0x13), with a salt and a hash of any length the format allows, so strings of other programs and of other
costs verify. `secret` is the pepper [generate](generate.md) was given, if any.

The result is success or the reason it is not: a wrong password is an error too, so that `if (!argon2::verify(…))`
rejects every case, and a check that is not one cannot be mistaken for a mismatch: `errc::authentication` is another
password, `errc::malformed` a string that is not Argon2's, `errc::unsupported` another version or costs past what a
stored string may ask (4 GiB of memory, 2^16 passes, 255 lanes), refused before anything is allocated.

## Parameters

| Parameter | Description |
|---|---|
| `password` | the password given, bytes or text |
| `phc` | the stored string |
| `secret` | the pepper the string was made with; empty by default |

## Return value

Success when the password matches; otherwise a [crypto::error](../error/README.md) with `errc::authentication`,
`errc::malformed` or `errc::unsupported`. `[[nodiscard]]`: a check whose result is dropped was never made.

## Complexity

As [derive](derive.md) with the string's costs.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string stored = "$argon2id$v=19$m=65536,t=2,p=1$c29tZXNhbHQ$CTFhFdXPJO1aFaMaO6Mm5c8y7cJHAph8ArZWb2GRPPc";
    println("{}", bool(crypto::argon2::verify("password", stored)));

    auto wrong = crypto::argon2::verify("passw0rd", stored);
    println("{}", wrong.error().message());

    auto broken = crypto::argon2::verify("password", "$argon2id$v=19$m=65536");
    println("{}", broken.error().message());
}
```

Output:

```text
true
sgcl::crypto::argon2: the password does not match
sgcl::crypto::argon2: not a PHC string of Argon2
```

## See also

- [generate](generate.md): the string
- [constant_time](../constant_time/README.md): the comparison it makes
- [sgcl::crypto::argon2](README.md)
