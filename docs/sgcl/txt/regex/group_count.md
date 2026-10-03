[sgcl](../../README.md) › [txt](../README.md) › [regex](README.md)

# sgcl::txt::regex::group_count

```cpp
size_t group_count() const noexcept;
```

Returns the number of capturing groups of the pattern, named or not; `(?: )` does not capture and the whole match
is not counted. A [match](../match/README.md) of the pattern has groups `1` to `group_count()`.

## Parameters

None.

## Return value

The number of capturing groups.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{}", txt::regex("(?<y>\\d{4})-(\\d{2})(?:-\\d{2})?").group_count());
    println("{}", txt::regex("\\w+").group_count());
}
```

Output:

```text
2
0
```

## See also

- [group_index](group_index.md): the number of a named group
- [match::group](../match/group.md): what a group matched
- [sgcl::txt::regex](README.md)
