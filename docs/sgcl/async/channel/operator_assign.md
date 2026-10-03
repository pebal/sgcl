[sgcl](../../README.md) › [async](../README.md) › [channel](../channel.md)

# sgcl::async::channel\<T\>::operator=

```cpp
channel& operator=(const channel& other) noexcept;    // (1)
channel& operator=(channel&& other) noexcept;         // (2)
```

Makes the handle one of the channel `other` refers to. The channel this handle referred to before is not touched:
it is not closed, and it lives on while another handle holds it.

1. A copy of the handle.
2. The same as (1): `other` still refers to the channel after.

The assignments of `channel<void>` are the same.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle of the channel to refer to |

## Return value

`*this`.

## Complexity

Constant: a store of one tracked word.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::channel<int> first(4), second(4);
    async::channel<int> current = first;
    current.send(1).wait();

    current = second;  // first is still open, and keeps its element
    current.send(2).wait();

    println("{} {}", *first.receive().wait(), *second.receive().wait());
    println("{}", current == second);
}
```

Output:

```text
1 2
true
```

## See also

- [(constructor)](channel.md): makes a channel, or another handle of one
- [operator==, operator!=](operator_cmp.md): whether two handles are of the same channel
- [sgcl::async::channel\<T\>](../channel.md)
