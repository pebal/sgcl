[sgcl](../../README.md) › [crypto](../README.md) › [bcrypt](README.md)

# sgcl::crypto::bcrypt::verify

```cpp
[[nodiscard]] static expected<void, error> verify(const slice<const byte>& password,
                                                  const string& hash) noexcept;
```

Whether `password` is the one `hash` was made from, Go's `bcrypt.CompareHashAndPassword`: the cost and the salt read
from the hash, the password hashed again and the two compared in constant time. `$2a$`, `$2b$` and `$2y$` are read,
at any cost from 4 to 31; the first 72 bytes of the password count, as in every implementation.

The result is success or the reason it is not: a wrong password is an error too, so that `if (!bcrypt::verify(…))`
rejects every case — `errc::authentication` for another password, `errc::malformed` for a string that is not a bcrypt
hash, `errc::unsupported` for `$2$` and `$2x$`.

## Parameters

| Parameter | Description |
|---|---|
| `password` | the password given, bytes or text |
| `hash` | the stored hash |

## Return value

Success when the password matches; otherwise a [crypto::error](../error/README.md) with `errc::authentication`,
`errc::malformed` or `errc::unsupported`. `[[nodiscard]]`: a check whose result is dropped was never made.

## Complexity

Linear in 2^cost of the hash.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string stored = "$2a$05$CCCCCCCCCCCCCCCCCCCCC.E5YPO9kmyuRGyh0XouQYb4YMJKvyOeW";
    println("{}", bool(crypto::bcrypt::verify("U*U", stored)));
    println("{}", crypto::bcrypt::verify("U*U*", stored).error().message());
    println("{}", crypto::bcrypt::verify("U*U", "$2a$05$").error().message());
}
```

Output:

```text
true
sgcl::crypto::bcrypt: the password does not match
sgcl::crypto::bcrypt: not a bcrypt hash
```

## See also

- [generate](generate.md): the hash
- [cost](cost.md): the cost of a hash
- [sgcl::crypto::bcrypt](README.md)
