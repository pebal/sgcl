[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md)

# sgcl::encoding::json::elements

```cpp
slice<const json> elements() const noexcept;
```

The elements of an array, in their order, as a [slice](../../core/slice.md) of the array's own buffer: nothing is
copied, and the slice keeps the buffer alive as long as it is kept, as a Go slice keeps its array. For a value
that is not an array, an empty slice.

## Parameters

None.

## Return value

The elements, or an empty slice when the value is not an array.

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
    encoding::json doc = encoding::json::parse(R"({"tags": ["a", "b", "c"], "name": "Ala"})");
    for (auto& tag : doc["tags"].elements()) {
        println(tag.as_string(""));
    }
    slice<const encoding::json> tags = doc["tags"].elements();
    println("{} {}", tags.size(), doc["name"].elements().size());
}
```

Output:

```text
a
b
c
3 0
```

## See also

- [members](members.md): the members of an object
- [operator[]](operator_at.md): one element by its index
- [size](size.md): the number of elements
- [sgcl::encoding::json](../json.md)
