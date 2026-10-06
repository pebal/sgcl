[sgcl](../../README.md) › [encoding](../README.md) › [content_line](README.md)

# sgcl::encoding::content_line::as_recurrence

```cpp
optional<recurrence> as_recurrence() const noexcept;
```

The value read as a [recurrence](../recurrence/README.md) rule (an RRULE's).

## Parameters

None.

## Return value

The rule, or `nullopt` for one that does not read.

## Complexity

Linear in the size of the value.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto rule = encoding::content_line("RRULE", "FREQ=WEEKLY;BYDAY=MO,WE;COUNT=4").as_recurrence();
    println("{} {}", rule->count(), rule->to_string());
}
```

Output:

```text
4 FREQ=WEEKLY;COUNT=4;BYDAY=MO,WE
```

## See also

- [recurrence](../recurrence/README.md)
- [sgcl::encoding::content_line](README.md)
