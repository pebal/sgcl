[sgcl](../../README.md) › [encoding](../README.md) › [recurrence](README.md)

# sgcl::encoding::recurrence::to_string

```cpp
string to_string() const;
```

The rule's text in RFC 5545's order: FREQ, UNTIL or COUNT, INTERVAL when not 1, BYSECOND, BYMINUTE, BYHOUR,
BYDAY, BYMONTHDAY, BYYEARDAY, BYWEEKNO, BYMONTH, BYSETPOS, WKST when not MO.

## Parameters

None.

## Return value

The text.

## Complexity

Linear in the size of the rule.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::recurrence("bymonth=1;freq=yearly;interval=1;wkst=MO").to_string());
}
```

Output:

```text
FREQ=YEARLY;BYMONTH=1
```

## See also

- [parse](parse.md)
- [sgcl::encoding::recurrence](README.md)
