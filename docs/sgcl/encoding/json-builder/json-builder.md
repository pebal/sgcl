[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md) › [builder](../json-builder.md)

# sgcl::encoding::json::builder::builder

```cpp
builder() noexcept;                   // (1)
builder(const builder& other);        // (2)
builder(builder&& other) noexcept;    // (3)
```

1. Constructs an empty builder, of no kind yet: its first [push_back](push_back.md) makes it a builder of an array,
   its first [set](set.md) one of an object. Nothing is allocated until then.
2. A copy: the same elements or members, of the same kind, going on apart. The assignment is the same.
3. Takes the elements or the members of `other` and its kind, and leaves `other` empty, as [build](build.md)
   leaves it: either kind may begin it again. The assignment is the same; a builder moved into itself stays as it
   was.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the builder copied or moved from |

## Complexity

- (1), (3) Constant.
- (2) Linear in the elements or the members.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json::builder tags;
    println("{} {}", tags.size(), tags.build().to_string());

    tags.push_back("a");
    encoding::json::builder more = tags;
    more.push_back("b");
    println("{} {}", tags.build().to_string(), more.build().to_string());
}
```

Output:

```text
0 []
["a"] ["a","b"]
```

## See also

- [push_back](push_back.md), [set](set.md): the first element or member
- [sgcl::encoding::json::builder](../json-builder.md)
