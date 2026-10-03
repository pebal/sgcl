[sgcl](../../README.md) › [core](../README.md) › [number_error](README.md)

# sgcl::operator== (sgcl::number_error)

```cpp
friend bool operator==(const number_error&, const number_error&) noexcept = default;
```

Compares two errors: equal when their reasons and their offsets are. `!=` is its negation. A hidden friend, found by
the arguments' type.

## Parameters

| Parameter | Description |
|---|---|
| (unnamed) | the errors to compare |

## Return value

`true` when the reasons and the offsets are equal.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    number_error empty(number_error::reason::empty, 0);
    println("{}", parse<int>("").error() == empty);
    println("{}", parse<int>("x").error() != empty);
}
```

Output:

```text
true
true
```

## See also

- [sgcl::number_error](README.md)
