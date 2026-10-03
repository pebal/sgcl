[sgcl](../../README.md) › [encoding](../README.md) › [error](README.md)

# sgcl::encoding::operator== (sgcl::encoding::error)

```cpp
friend bool operator==(const error& a, const error& b) noexcept;
```

Compares two errors by everything they say: the code, the offset (and whether the error has a place in a text at all),
the line, the column, the path, the words of the detail, and the stream's error, by its code as [io::error](../../io/error/README.md) compares. `!=` is made from it by the
compiler. A test compares an error with the one it expects.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the errors compared |

## Return value

`true` when all of it is equal.

## Complexity

Linear in the length of the details and the paths.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto a = encoding::base64::standard.decode("QU*D").error();
    auto b = encoding::base64::standard.decode("AB*D").error();
    println("{}", a == b);
    encoding::error expected(encoding::errc::invalid_character, 2, "invalid character '*'");
    println("{}", a == expected);
    println("{}", a == encoding::error(encoding::errc::invalid_character, 2));
}
```

Output:

```text
true
true
false
```

## See also

- [code](code.md): the code alone, for a test of the kind
- [sgcl::encoding::error](README.md)
