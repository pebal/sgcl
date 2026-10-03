[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md)

# sgcl::encoding::json::members

```cpp
slice<const member> members() const noexcept;
```

The [members](../json-member.md) of an object in the order of the input, as a [slice](../../core/slice.md) of the
object's own buffer: nothing is copied, and the slice keeps the buffer alive as long as it is kept. A member is
taken apart as `[key, value]`. For a value that is not an object, an empty slice.

## Parameters

None.

## Return value

The members, or an empty slice when the value is not an object.

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
    encoding::json doc = encoding::json::parse(R"({"z": 1, "a": [true], "m": {"x": null}})");
    for (auto& [key, value] : doc.members()) {
        println("{} is {}", key, value.to_string());
    }
    println(doc["a"].members().size());
}
```

Output:

```text
z is 1
a is [true]
m is {"x":null}
0
```

## See also

- [elements](elements.md): the elements of an array
- [operator[]](operator_at.md), [contains](contains.md): one member by its key
- [member](../json-member.md): a key and a value
- [sgcl::encoding::json](../json.md)
