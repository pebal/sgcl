[sgcl](../../README.md) › [async](../README.md) › [stop_token](README.md)

# sgcl::async::operator== (sgcl::async::stop_token)

```cpp
friend bool operator==(const stop_token& a, const stop_token& b) noexcept;
```

Checks whether `a` and `b` belong to the same source: whether they share the stop's state. Two tokens made by
default are equal, both with no source. A hidden friend, found by the argument's type alone; `a != b` is
rewritten to it by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the tokens to compare |

## Return value

`true` when both tokens have the same source, or both none; `false` otherwise.

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
    async::stop_source source;
    async::stop_source copy = source;  // the same state
    async::stop_source child(source.token());
    println("{}", source.token() == copy.token());
    println("{}", source.token() != child.token());
    println("{}", async::stop_token() == async::stop_token());
}
```

Output:

```text
true
true
true
```

## See also

- [stop_source::token](../stop_source/token.md): the token of a source
- [sgcl::async::stop_token](README.md)
