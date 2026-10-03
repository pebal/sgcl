[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml/README.md) › [reader](README.md)

# sgcl::encoding::xml::reader::depth

```cpp
uint32_t depth() const noexcept;
```

The number of elements open around the next token: 0 outside the root, 1 inside it. After a start, the next token
is inside the element that started.

## Parameters

None.

## Return value

The depth of the next token.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::reader r("<a><b><c/></b><d/></a>");
    while (r.peek()) {
        uint32_t depth = r.depth();
        auto t = r.next();
        if (t->type() == encoding::xml::token::kind::start_element) {
            println("{}{}", string(2 * depth, ' '), t->name());
        }
    }
}
```

Output:

```text
a
  b
    c
  d
```

## See also

- [offset](offset.md): the byte where the next token starts
- [sgcl::encoding::xml::reader](README.md)
