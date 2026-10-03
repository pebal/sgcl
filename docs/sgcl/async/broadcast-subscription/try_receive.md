[sgcl](../../README.md) › [async](../README.md) › [broadcast](../broadcast/README.md) › [subscription](README.md)

# sgcl::async::broadcast\<T\>::subscription::try_receive

```cpp
optional<T> try_receive() noexcept(std::is_nothrow_copy_constructible_v<T> &&
                                   std::is_nothrow_move_constructible_v<T> &&
                                   std::is_nothrow_move_assignable_v<T>);
```

Receives the next value if it is there, without waiting: the value at the cursor when its position is committed,
copied out of the ring, the cursor moved on. A cursor the ring has lapped moves to the oldest value still there, and
[lagged](lagged.md) counts the ones it passed over.

## Parameters

None.

## Return value

A copy of the next value, or `nullopt` when none is there yet. `nullopt` does not tell a closed broadcast from an
open one: [closed](closed.md) does.

## Complexity

Constant: a copy of the value and an atomic decrement on its node.

## Exceptions

What the copy constructor, the move constructor and the move assignment of `T` throw; none when they are noexcept.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::broadcast<string> news(4);
    auto s = news.subscribe();
    news.send("first");
    news.send("second");

    while (auto n = s.try_receive()) {
        println("{}", *n);
    }
    println("{}", s.try_receive());
}
```

Output:

```text
first
second
nullopt
```

## See also

- [receive](receive.md): waits for a value
- [sgcl::async::broadcast\<T\>::subscription](README.md)
