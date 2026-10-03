[sgcl](../../README.md) › [async](../README.md) › [once](../once.md)

# sgcl::async::once::once

```cpp
once() noexcept = default;     // (1)
once(const once&) = delete;    // (2)
```

1. A once not yet called: its channel open, no exception kept.
2. A once is not copyable, and not movable: it is an object of one place, which tasks reach through the object
   that holds it.

## Parameters

None.

## Complexity

Constant: the channel inside the object, made with it.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

struct Service {
    async::once started;  // a member of a managed object
};

int main() {
    async::once local;  // on the stack
    tracked_ptr service = make_tracked<Service>();
    println("{} {}", local.called(), service->started.called());
    println("{}", std::is_copy_constructible_v<async::once>);
}
```

Output:

```text
false false
false
```

## See also

- [call](call.md): runs the function once
- [sgcl::async::once](../once.md)
