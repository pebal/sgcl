[sgcl](../../README.md) › [encoding](../README.md) › [json](../json/README.md) › [builder](README.md)

# sgcl::encoding::json::builder::size

```cpp
size_t size() const noexcept;
```

The number of elements or members added since the builder was made or last built. A key set twice counts twice,
though the value [build](build.md) makes has it once.

## Parameters

None.

## Return value

The number of `push_back` or `set` calls so far.

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
    encoding::json::builder b;
    b.set("a", 1).set("b", 2).set("a", 3);
    println(b.size());
    encoding::json built = b.build();
    println("{} {}", built.size(), b.size());
}
```

Output:

```text
3
2 0
```

## See also

- [build](build.md): the value
- [sgcl::encoding::json::builder](README.md)
