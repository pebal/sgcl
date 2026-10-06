[sgcl](../../README.md) › [time](../README.md) › [date_format](README.md)

# sgcl::time::date_format::pattern

```cpp
const string& pattern() const noexcept;
```

Returns the pattern of CLDR's letters the format writes: the one resolved from the styles or the skeleton, or the
one given.

## Parameters

None.

## Return value

The pattern.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{}", time::date_format(txt::locale("pl"), time::style::medium, time::style::brief)
                      .pattern());
    println("{}", time::date_format::from_skeleton(txt::locale("en"), "yMMMdjm").pattern());
}
```

Output:

```text
d MMM y, HH:mm
MMM d, y, h:mm a
```

## See also

- [from_skeleton](from_skeleton.md)
- [sgcl::time::date_format](README.md)
