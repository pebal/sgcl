[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::crypto_category

```cpp
#include "sgcl/crypto/error.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    const std::error_category& crypto_category() noexcept;
}
```

Returns the `std::error_category` of the module's codes, one object for the whole program: its `name()` is
`"crypto"`, and its `message(c)` the words of the code [errc](errc.md) `c` — `"message authentication failed"`,
`"invalid key"`, `"invalid signature"`, `"malformed data"`, `"unsupported algorithm or parameter"`,
`"verification failed"`, and `"unknown crypto error"` for a value outside the list. It is what
[make_error_code](make_error_code.md) puts in a `std::error_code`, and what an [error](error.md)'s
[message](error/message.md) says when the error has no text of its own.

## Parameters

None.

## Return value

The category, a reference to an object that lives as long as the program.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    const std::error_category& category = crypto::crypto_category();
    println("{}", category.name());
    for (int c : range(1, 8)) {
        println("{}: {}", c, category.message(c));
    }
}
```

Output:

```text
crypto
1: message authentication failed
2: invalid key
3: invalid signature
4: malformed data
5: unsupported algorithm or parameter
6: verification failed
7: unknown crypto error
```

## See also

- [make_error_code](make_error_code.md): a code in this category
- [errc](errc.md): the codes
- [error](error.md): the error of the module
