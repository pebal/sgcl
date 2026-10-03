[sgcl](../../README.md) › [async](../README.md) › [receive_channel](../receive_channel.md)

# sgcl::async::receive_channel\<T\>::operator=

```cpp
receive_channel& operator=(const receive_channel&) noexcept = default;    // (1)
receive_channel& operator=(receive_channel&&) noexcept = default;         // (2)
```

Makes the handle one of the channel another handle refers to. The channel this handle referred to before is not
touched: it lives on while another handle holds it. A `channel<T>` on the right converts first, through the
[constructor](receive_channel.md).

1. A copy of the handle.
2. The same as (1): the source still refers to the channel after.

The assignments of `receive_channel<void>` are the same.

## Parameters

| Parameter | Description |
|---|---|
| `const receive_channel&`, `receive_channel&&` | the handle of the channel to refer to |

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
    async::channel<int> first(2), second(2);
    first.send(1).wait();
    second.send(2).wait();
    async::receive_channel<int> in = first;
    println("{}", *in.receive().wait());
    in = second;
    println("{}", *in.receive().wait());
}
```

Output:

```text
1
2
```

## See also

- [(constructor)](receive_channel.md): the receiving end of a channel
- [operator==, operator!=](operator_cmp.md): whether two handles are of the same channel
- [sgcl::async::receive_channel\<T\>](../receive_channel.md)
