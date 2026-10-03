[sgcl](../../README.md) › [async](../README.md) › [stop_source](../stop_source.md)

# sgcl::async::stop_source::stop_requested

```cpp
bool stop_requested() const noexcept;
```

Checks whether the stop of this source has been requested: by [request_stop](request_stop.md), by a deadline, or
by a parent's stop. The closed flag of the token's channel, one load, as the tokens' own
[stop_requested](../stop_token/stop_requested.md) reads it.

## Parameters

None.

## Return value

`true` when the stop has been requested, `false` otherwise.

## Complexity

Constant: one load.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::stop_source parent;
    async::stop_source child(parent.token());
    println("{}", child.stop_requested());
    parent.request_stop();
    println("{}", child.stop_requested());
}
```

Output:

```text
false
true
```

## See also

- [stop_token::stop_requested](../stop_token/stop_requested.md): the same look from a token
- [request_stop](request_stop.md): the stop
- [sgcl::async::stop_source](../stop_source.md)
