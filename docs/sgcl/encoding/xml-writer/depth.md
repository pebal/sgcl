[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml/README.md) › [writer](README.md)

# sgcl::encoding::xml::writer::depth

```cpp
size_t depth() const noexcept;
```

The number of elements open: started and not yet ended. A program closes every element still open by calling
[end](end.md) until the depth is 0.

## Parameters

None.

## Return value

The elements open.

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
    encoding::xml::writer w(io::stdout);
    w.start("a").start("b").start("c");
    println(w.depth());
    while (w.depth()) {
        w.end();
    }
    w.flush().value();
    println();
}
```

Output:

```text
3
<a><b><c/></b></a>
```

## See also

- [start](start.md), [end](end.md)
- [sgcl::encoding::xml::writer](README.md)
