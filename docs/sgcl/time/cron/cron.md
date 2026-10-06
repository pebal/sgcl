[sgcl](../../README.md) › [time](../README.md) › [cron](README.md)

# sgcl::time::cron::cron

```cpp
explicit cron(const string& expression, const zone& z = zone::local());    // (1)
cron(const cron&) = default;                                               // (2)
cron(cron&&) noexcept = default;                                           // (3)
```

1. The cron of an expression the program itself writes, its times read in `z`, the local zone by default: what
   [parse](parse.md) reads, or `bad_expected_access<time::error>` with parse's message, as the module's constructors
   from a text throw. An expression that comes from outside, a setting or the user, is parsed.
2. A copy: the same times in the same zone.
3. The same, taken from the other.

## Parameters

| Parameter | Description |
|---|---|
| `expression` | the cron expression |
| `z` | the zone whose clock the times are read on |

## Complexity

Linear in the length of the expression.

## Exceptions

- (1) `bad_expected_access<time::error>` when the expression does not read.
- (2) None, but out of memory.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::cron reports("0 9 * * MON-FRI", time::zone::utc());
    println("{}", reports.to_string());
    try {
        time::cron wrong("0 25 * * *");
    } catch (const bad_expected_access<time::error>& e) {
        println("{}", e.error().message());
    }
}
```

Output:

```text
0 9 * * MON-FRI
a value out of the cron field's range
```

## See also

- [parse](parse.md): an expression from outside
- [sgcl::time::cron](README.md)
