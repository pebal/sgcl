[sgcl](../../README.md) › [txt](../README.md) › [regex_error](README.md)

# sgcl::txt::regex_error::offset

```cpp
size_t offset() const noexcept;
```

Returns the byte of the pattern where it went wrong: the construct that is refused, the bracket without its pair,
the escape that is not known. The [message](message.md) says the same number in words.

## Parameters

None.

## Return value

The offset in bytes from the start of the pattern.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (const char* p : {"(ab", "ab)", "a\\q", "x(?<=y)"}) {
        println("{}: {}", p, txt::regex::compile(p).error().offset());
    }
}
```

Output:

```text
(ab: 0
ab): 2
a\q: 1
x(?<=y): 1
```

## See also

- [message](message.md): the sentence
- [sgcl::txt::regex_error](README.md)
