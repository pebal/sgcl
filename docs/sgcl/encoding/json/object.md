[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md)

# sgcl::encoding::json::object

```cpp
static json object(std::initializer_list<member> members) noexcept;
```

An object of the given [members](../json-member.md), in their order: `json::object({{"name", "Ala"}, {"age", 30}})`.
A key given twice keeps its last member, in that member's place. `object({})` is the empty object, `{}`.

## Parameters

| Parameter | Description |
|---|---|
| `members` | the members, each a key and a value |

## Return value

The object.

## Complexity

Linear in the number of members.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto user = encoding::json::object({
        {"name", "Ala"},
        {"age", 30},
        {"tags", encoding::json::array({"a", "b"})},
        {"age", 31},
    });
    println(user.to_string());
    println(encoding::json::object({}).to_string());
}
```

Output:

```text
{"name":"Ala","tags":["a","b"],"age":31}
{}
```

## See also

- [array](array.md): an array of elements
- [builder](../json-builder.md): an object made in a loop
- [set](set.md): the object with a member set
- [sgcl::encoding::json](../json.md)
