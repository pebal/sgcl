[sgcl](../../README.md) › [async](../README.md) › [event](README.md)

# sgcl::async::event::is_set

```cpp
bool is_set() const noexcept;
```

Checks whether the event is set: whether its channel is closed. Once `true`, it stays `true`. After a wait for the
event has ended, by [wait](wait.md), `co_await` or a select's [on_set](on_set.md) case, it is `true`, for the
events of the reactor and the timers too, since an event is set by the close of its channel alone.

## Parameters

None.

## Return value

`true` when the event has been set, `false` otherwise.

## Complexity

Constant: one atomic load.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::event loaded;
    println("{}", loaded.is_set());
    loaded.set();
    println("{}", loaded.is_set());
}
```

Output:

```text
false
true
```

## See also

- [set](set.md): sets the event
- [wait, operator co_await](wait.md): waits for the set
- [sgcl::async::event](README.md)
