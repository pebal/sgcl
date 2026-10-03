[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md)

# sgcl::encoding::json::size

```cpp
size_t size() const noexcept;
```

The number of elements of an array or of members of an object; 0 for any other value, a string included, whose
length is its [string](../../core/string.md)'s.

## Parameters

None.

## Return value

The number of elements or members, or 0.

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
    encoding::json doc = encoding::json::parse(
        R"({"tags": ["a", "b", "c"], "name": "Ala", "none": {}})");
    println("{} {} {} {}", doc.size(), doc["tags"].size(), doc["name"].size(), doc["none"].size());
}
```

Output:

```text
3 3 0 0
```

## See also

- [empty](empty.md): whether there are none
- [elements](elements.md), [members](members.md): the elements, the members
- [sgcl::encoding::json](../json.md)
