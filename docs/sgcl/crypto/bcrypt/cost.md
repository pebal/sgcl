[sgcl](../../README.md) › [crypto](../README.md) › [bcrypt](README.md)

# sgcl::crypto::bcrypt::cost

```cpp
static expected<int, error> cost(const string& hash) noexcept;
```

The cost `hash` was made with, Go's `bcrypt.Cost`: what tells a program that a stored hash is of a cost below the one
it uses now, so that it makes a new hash from the password at the next successful login.

## Parameters

| Parameter | Description |
|---|---|
| `hash` | the stored hash |

## Return value

The cost, 4 to 31, or the error [verify](verify.md) would give for a string that is not a hash: `errc::malformed` or
`errc::unsupported`.

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
    string stored = "$2a$05$CCCCCCCCCCCCCCCCCCCCC.E5YPO9kmyuRGyh0XouQYb4YMJKvyOeW";
    if (crypto::bcrypt::verify("U*U", stored) && crypto::bcrypt::cost(stored).value() < 12) {
        stored = string(crypto::bcrypt::generate("U*U", 12));
    }
    println("{}", crypto::bcrypt::cost(stored).value());
}
```

Output:

```text
12
```

## See also

- [generate](generate.md): a hash of a chosen cost
- [sgcl::crypto::bcrypt](README.md)
