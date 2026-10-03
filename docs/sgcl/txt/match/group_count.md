[sgcl](../../README.md) › [txt](../README.md) › [match](../match.md)

# sgcl::txt::match::group_count

```cpp
size_t group_count() const noexcept;
```

Returns the number of capturing groups of the pattern the match was found by, the whole match not counted:
[group](group.md)`(1)` to `group(group_count())` are the ones there are, whether they took part or not. It is
[regex::group_count](../regex/group_count.md) of that pattern.

## Parameters

None.

## Return value

The number of groups; 0 for a pattern without groups and for an empty match made by the default constructor.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto m = txt::regex("(\\w+)=(\\w+)?").find("klucz=");
    for (size_t i : range(size_t(1), m->group_count() + 1)) {
        println("{}: {}", i, m->group(i).has_value());
    }
}
```

Output:

```text
1: true
2: false
```

## See also

- [group](group.md): what a group matched
- [sgcl::txt::match](../match.md)
