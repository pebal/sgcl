[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::mapping

```cpp
static yaml mapping(std::initializer_list<member> members) noexcept;    // (1)
static yaml mapping(const vector<member>& members) noexcept;            // (2)
```

A mapping of the members in their order, keys of any kind. A key given twice is kept twice here; written and read
back it is a [parse](parse.md) error, so a program gives every key once.

1. Of a list written out, `{key, value}` each.
2. Of a [vector](../../core/vector/README.md) made in a loop.

## Parameters

| Parameter | Description |
|---|---|
| `members` | the keys and their values |

## Return value

The node.

## Complexity

Linear in the count of the members.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::yaml v = encoding::yaml::mapping({{"name", "app"}, {"replicas", 3},
                                                {encoding::yaml::sequence({1, 2}), "a key that is a sequence"}});
    print(v.to_string());
}
```

Output:

```text
name: app
replicas: 3
? - 1
  - 2
: a key that is a sequence
```

## See also

- [sequence](sequence.md)
- [sgcl::encoding::yaml](README.md)
