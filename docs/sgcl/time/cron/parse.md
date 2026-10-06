[sgcl](../../README.md) › [time](../README.md) › [cron](README.md)

# sgcl::time::cron::parse

```cpp
static expected<cron, error> parse(const string& expression, const zone& z = zone::local()) noexcept;
```

Reads a cron expression, its times read in `z`: five fields or six, or a name, in the syntax of the
[class page](README.md). White space before, after and between the fields is any number of spaces and tabs. A text
that does not read is an [error](../error/README.md) with a sentence and the byte of the expression where the field
or the item that failed starts.

## Parameters

| Parameter | Description |
|---|---|
| `expression` | the cron expression, from a setting, the user, a file |
| `z` | the zone whose clock the times are read on; the local zone by default |

## Return value

The cron, or an [error](../error/README.md):

- `a cron expression has 5 or 6 fields`: too few or too many;
- `a value out of the cron field's range`, `a cron range backwards`, `a cron step of zero`;
- `a number expected in a cron field`, `a number or a name expected in a cron field`, `a number too large for a cron
  field`, `an unexpected character in a cron field`, `an empty item in a cron field`;
- `L-n takes n from 0 to 30`, `nW takes a day from 1 to 31`, `nL takes a day of the week`, `n#k takes a day of the
  week and k from 1 to 5`;
- `an unknown cron name`, `a named cron expression is one word`.

## Complexity

Linear in the length of the expression.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    for (const char* text : {"*/10 * * * *", "0 0 L * *", "0 0 * * 9", "@fortnightly"}) {
        auto c = time::cron::parse(text, time::zone::utc());
        if (c) {
            println("{}: fine", text);
        } else {
            println("{}: {} at byte {}", text, c.error().message(), c.error().offset());
        }
    }
}
```

Output:

```text
*/10 * * * *: fine
0 0 L * *: fine
0 0 * * 9: a value out of the cron field's range at byte 8
@fortnightly: an unknown cron name at byte 0
```

## See also

- [(constructor)](cron.md): an expression written in the program
- [sgcl::time::cron](README.md)
