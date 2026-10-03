[sgcl](../../README.md) › [async](../README.md) › [timed_out](../timed_out.md)

# sgcl::async::operator== (sgcl::async::timed_out)

```cpp
friend bool operator==(const timed_out&, const timed_out&) noexcept = default;
```

Compares two `timed_out` errors: always equal, as the class carries nothing but its kind. It lets an
`expected<T, timed_out>` be compared with an `unexpected(timed_out())`. A hidden friend, found by the argument's
type alone; `!=` is rewritten to it by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| (unnamed) | the errors to compare |

## Return value

`true`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    expected<int, async::timed_out> result = unexpected(async::timed_out());
    println("{}", result == unexpected(async::timed_out()));
    println("{}", async::timed_out() != async::timed_out());
}
```

Output:

```text
true
false
```

## See also

- [message](message.md): the text of the error
- [sgcl::async::timed_out](../timed_out.md)
