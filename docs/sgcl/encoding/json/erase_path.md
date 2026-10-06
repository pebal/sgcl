[sgcl](../../README.md) › [encoding](../README.md) › [json](README.md)

# sgcl::encoding::json::erase_path

```cpp
json erase_path(const string& pointer) const noexcept;
```

A new value without the one at the JSON Pointer, as RFC 6902's `remove` takes it: a member removed, an element
removed and the ones after it moved down. This value when there is none at the pointer, and for `""`, the whole.

## Parameters

| Parameter | Description |
|---|---|
| `pointer` | the JSON Pointer |

## Return value

The new value.

## Complexity

Linear in the size of the containers on the path.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto doc = encoding::json::parse(R"({"a": {"b": [1, 2, 3]}, "c/d": 4})").value();
    println(doc.erase_path("/a/b/1").to_string());
    println(doc.erase_path("/c~1d").to_string());
    println(doc.erase_path("/x").to_string());
}
```

Output:

```text
{"a":{"b":[1,3]},"c/d":4}
{"a":{"b":[1,2,3]}}
{"a":{"b":[1,2,3]},"c/d":4}
```

## See also

- [set_path](set_path.md), [at_path](at_path.md)
- [sgcl::encoding::json](README.md)
