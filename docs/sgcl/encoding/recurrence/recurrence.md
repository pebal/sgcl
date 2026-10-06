[sgcl](../../README.md) › [encoding](../README.md) › [recurrence](README.md)

# sgcl::encoding::recurrence::recurrence

```cpp
recurrence() noexcept;                      // (1)
explicit recurrence(const string& text);    // (2)
```

1. `FREQ=DAILY`, nothing else: every day at the start's time.
2. The rule of a text written in the program: [parse](parse.md)`(text).value()`.

The copy and the move are the implicit ones and copy the handle.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the rule's text |

## Complexity

(1) Constant; (2) linear in the length of the text.

## Exceptions

- (1) None.
- (2) `bad_expected_access<encoding::error>` for a text that is no rule.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::recurrence().to_string());
    println(encoding::recurrence("freq=weekly;byday=mo,fr").to_string());
}
```

Output:

```text
FREQ=DAILY
FREQ=WEEKLY;BYDAY=MO,FR
```

## See also

- [parse](parse.md)
- [sgcl::encoding::recurrence](README.md)
