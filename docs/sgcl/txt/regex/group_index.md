[sgcl](../../README.md) › [txt](../README.md) › [regex](README.md)

# sgcl::txt::regex::group_index

```cpp
optional<size_t> group_index(const string& name) const noexcept;
```

Returns the number of the group of that name, `(?<name> )` or `(?P<name> )`: groups are numbered by their opening
parenthesis, from 1, named or not.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the group |

## Return value

The number of the group, or an empty `optional` when the pattern has no group of that name.

## Complexity

Linear in the number of named groups.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::regex date("(\\d{2})\\.(?<m>\\d{2})\\.(?P<y>\\d{4})");
    println("{} {} {}", *date.group_index("m"), *date.group_index("y"),
            date.group_index("d").has_value());
}
```

Output:

```text
2 3 false
```

## See also

- [group_count](group_count.md): the number of groups
- [match::group](../match/group.md): a group by number or by name
- [sgcl::txt::regex](README.md)
