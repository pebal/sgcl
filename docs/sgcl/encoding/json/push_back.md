[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md)

# sgcl::encoding::json::push_back

```cpp
json push_back(const json& value) const noexcept;
```

A new value: the array with `value` appended. On a value that is not an array, an array of that one element. This
value stays as it was.

Each call copies the elements: an array made in a loop is made by a [builder](../json-builder.md).

## Parameters

| Parameter | Description |
|---|---|
| `value` | the element to append |

## Return value

The new array.

## Complexity

Linear in the number of elements, which are copied as handles.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json tags = encoding::json::parse(R"(["a", "b"])");
    auto more = tags.push_back("c");
    println("{} {}", tags.to_string(), more.to_string());
    println(encoding::json().push_back(1).to_string());
}
```

Output:

```text
["a","b"] ["a","b","c"]
[1]
```

## See also

- [set](set.md): an element replaced
- [builder](../json-builder.md): an array made in a loop
- [set_path](set_path.md): `-` appends deeper down
- [sgcl::encoding::json](../json.md)
