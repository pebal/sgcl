[sgcl](../../README.md) › [crypto](../README.md) › [bcrypt](README.md)

# sgcl::crypto::bcrypt::generate

```cpp
static expected<string, error> generate(const slice<const byte>& password, int cost = default_cost);
```

A hash of `password` for storage, Go's `bcrypt.GenerateFromPassword`: 16 random bytes of salt and 2^`cost` rounds,
written as `$2b$`, the cost in two digits, `$`, and the salt and the hash in bcrypt's base64, 60 characters in all.
Two hashes of one password differ by their salt. A password longer than 72 bytes is refused: bcrypt would read only
72 of it, and a longer password would match every other with the same start.

## Parameters

| Parameter | Description |
|---|---|
| `password` | the password, bytes or text, at most 72 bytes |
| `cost` | 4 to 31, 10 by default |

## Return value

The hash, or a [crypto::error](../error/README.md) with `errc::invalid_key` for a password longer than 72 bytes.

## Complexity

Linear in 2^`cost`.

## Exceptions

`std::invalid_argument` when `cost` is outside 4 to 31.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", string(crypto::bcrypt::generate("hunter2", 12)));

    string too_long(73, 'a');
    auto refused = crypto::bcrypt::generate(too_long);
    println("{}", refused.error().message());
}
```

Sample output:

```text
$2b$12$ahWndnwvnLNd/occplPHeOyzU4VN.8BsZhqhFcyCVa4mCQRQ0VGhe
sgcl::crypto::bcrypt: a password longer than 72 bytes
```

## See also

- [verify](verify.md): checks a password against the hash
- [sgcl::crypto::bcrypt](README.md)
