[sgcl](../../README.md) › [encoding](../README.md) › [recurrence](README.md)

# sgcl::encoding::recurrence::parse

```cpp
static expected<recurrence, error> parse(const string& text) noexcept;
```

A rule's text: parts `NAME=value` separated by `;`, in any order, names in any case, each once; `X-` parts passed
over. By the [rules](README.md#rules).

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, an RRULE's value |

## Return value

The rule, or the [error](../error/README.md) `syntax` with its place: no FREQ, an unknown part, a part twice, a value
out of its range, a part the frequency forbids, COUNT with UNTIL, BYSETPOS alone.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto rule = encoding::recurrence::parse("FREQ=YEARLY;BYMONTH=11;BYDAY=4TH").value();
    println("{} {}", rule.by_day()[0].ordinal, rule.to_string());
    for (const char* bad : {"COUNT=3", "FREQ=WEEKLY;BYDAY=1MO", "FREQ=DAILY;COUNT=1;UNTIL=20300101", "FREQ=DAILY;BYHOUR=24"}) {
        println(encoding::recurrence::parse(bad).error().message());
    }
}
```

Output:

```text
4 FREQ=YEARLY;BYDAY=4TH;BYMONTH=11
1:1: a rule without FREQ
1:1: a numbered BYDAY in a rule but MONTHLY and YEARLY, or with BYWEEKNO
1:1: a rule of both UNTIL and COUNT
1:19: a BYHOUR past 0 to 23
```

## See also

- [to_string](to_string.md)
- [sgcl::encoding::recurrence](README.md)
