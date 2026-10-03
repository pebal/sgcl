[sgcl](../../README.md) › [compress](../README.md) › [error](../error.md)

# sgcl::compress::error::code

```cpp
errc code() const noexcept;
```

Returns what went wrong, one of the [errc](../errc.md) codes: what a program branches on, since the message is for
people.

## Parameters

None.

## Return value

The code of the error.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string sample = "hello world";
    auto packed = compress::zlib::compress("hello", {.dictionary = slice<const byte>(sample)});
    auto back = compress::zlib::decompress(packed);
    if (!back && back.error().code() == compress::errc::dictionary_required) {
        println("a dictionary is needed");
    }
}
```

Output:

```text
a dictionary is needed
```

## See also

- [errc](../errc.md): the codes
- [sgcl::compress::error](../error.md)
