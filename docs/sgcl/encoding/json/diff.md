[sgcl](../../README.md) › [encoding](../README.md) › [json](README.md)

# sgcl::encoding::json::diff

```cpp
static json diff(const json& from, const json& to) noexcept;
```

A JSON Patch that turns `from` into `to`: [patch](patch.md) of it applied to `from` gives a value equal to `to`.
Objects member by member (a member removed, added, or its value compared further), arrays element by element with
their tails removed or added, and a value of another kind replaced; nothing for values that are equal.

## Parameters

| Parameter | Description |
|---|---|
| `from` | the value as it was |
| `to` | the value as it is |

## Return value

The patch, an array of operations.

## Complexity

Linear in the sizes of the values, and in the members of the objects for each lookup.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto before = encoding::json::parse(R"({"a": 1, "b": [1, 2, 3], "c": true})").value();
    auto after = encoding::json::parse(R"({"a": 2, "b": [1, 2], "d": null})").value();
    auto ops = encoding::json::diff(before, after);
    println(ops.to_string(encoding::json::pretty));
    println(before.patch(ops).value() == after);
}
```

Output:

```text
[
  {
    "op": "remove",
    "path": "/c"
  },
  {
    "op": "add",
    "path": "/d",
    "value": null
  },
  {
    "op": "replace",
    "path": "/a",
    "value": 2
  },
  {
    "op": "remove",
    "path": "/b/2"
  }
]
true
```

## See also

- [patch](patch.md)
- [sgcl::encoding::json](README.md)
