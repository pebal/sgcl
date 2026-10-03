[sgcl](../../README.md) › [slog](../README.md) › [value](../value.md)

# sgcl::slog::value::as_time

```cpp
time::datetime as_time() const;
```

Returns the value of a value of kind `time`, the `time::datetime` given, in a zone of its offset: a fixed zone of the offset it had, or UTC for none. The instant is the same; the zone's name and rules are not kept. A value of another kind is a mistake of the program: a `logic_error`, where Go panics.

## Parameters

None.

## Return value

The time, a [datetime](../../time/datetime.md).

## Complexity

Constant.

## Exceptions

`logic_error` when [type](type.md) is not `time`: `sgcl::slog::value::as_time: a value of another kind`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    slog::memory kept;
    auto at = time::datetime::from_unix(0, time::zone::fixed(std::chrono::hours(2)));
    slog::logger(kept).info("m", "at", at);
    auto v = (*kept.records()[0].begin()).value();
    println("{}", v.as_time().to_string());
    println("{}", v.as_time().unix() == at.unix());
}
```

Output:

```text
1970-01-01T02:00:00+02:00
true
```

## See also

- [type](type.md)
- [kind](../value-kind.md): `time`
- [sgcl::slog::value](../value.md)
