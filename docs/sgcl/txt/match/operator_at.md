[sgcl](../../README.md) › [txt](../README.md) › [match](README.md)

# sgcl::txt::match::operator[]

```cpp
slice<const char> operator[](size_t n) const noexcept;
```

Returns what group `n` matched, with a group that took no part reading as empty: the short form of
[group](group.md)`(n)` for the caller who does not care about the difference. `m[0]` is the whole match.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of the group; `0` for the whole match |

## Return value

The bytes the group matched; an empty slice when it took no part or the pattern has no group `n`.

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
    txt::regex size("(\\d+)x(\\d+)(?:@(\\d+)x)?");
    for (const auto& m : size.all("1920x1080, 2560x1440@2x")) {
        println("{} by {}, scale [{}]", m[1], m[2], m[3]);
    }
}
```

Output:

```text
1920 by 1080, scale []
2560 by 1440, scale [2]
```

## See also

- [group](group.md): a group, or nothing when it took no part
- [sgcl::txt::match](README.md)
