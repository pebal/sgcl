[sgcl](../../README.md) › [async](../README.md) › [stopped](../stopped.md)

# sgcl::async::operator== (sgcl::async::stopped)

```cpp
friend bool operator==(const stopped&, const stopped&) noexcept = default;
```

Compares two `stopped` errors: always equal, as the class carries nothing but its kind. It lets an
`expected<T, stopped>` be compared with an `unexpected(stopped())`. A hidden friend, found by the argument's type
alone; `!=` is rewritten to it by the compiler.

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
    expected<int, async::stopped> result = unexpected(async::stopped());
    println("{}", result == unexpected(async::stopped()));
    println("{}", async::stopped() != async::stopped());
}
```

Output:

```text
true
false
```

## See also

- [message](message.md): the text of the error
- [sgcl::async::stopped](../stopped.md)
