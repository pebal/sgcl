[sgcl](../../README.md) › [async](../README.md) › [broadcast](../broadcast.md)

# sgcl::async::broadcast\<T\>::subscribers

```cpp
size_type subscribers() const noexcept;
```

The number of subscriptions alive: made by [subscribe](subscribe.md) and not destroyed or assigned over yet. A send
takes this count with its position, and its value is held for that many subscriptions. With other threads at work
the number may have changed by the time it is read.

## Parameters

None.

## Return value

The number of subscriptions alive, at most 4095.

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
    async::broadcast<int> events(4);
    auto a = events.subscribe();
    {
        auto b = events.subscribe();
        println("{}", events.subscribers());
    }
    println("{}", events.subscribers());  // b counted itself off
}
```

Output:

```text
2
1
```

## See also

- [subscribe](subscribe.md): makes a subscription
- [subscription](../broadcast-subscription.md): what counts itself off when it is destroyed
- [sgcl::async::broadcast\<T\>](../broadcast.md)
