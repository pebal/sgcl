[sgcl](../../README.md) › [async](../README.md) › [receive_channel](../receive_channel.md)

# sgcl::async::receive_channel\<T\>::capacity

```cpp
size_type capacity() const noexcept;
```

The number of elements the channel's buffer holds, as given to its constructor: Go's `cap(ch)`, the channel's
[capacity](../channel/capacity.md). The same for `receive_channel<void>`.

## Parameters

None.

## Return value

The capacity of the channel; 0 for a rendezvous.

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
    async::receive_channel<void> stop = source.token().channel();
    async::channel<int> numbers(8);
    async::receive_channel<int> in = numbers;
    println("{} {}", stop.capacity(), in.capacity());
}
```

Output:

```text
0 8
```

## See also

- [size](size.md): the number of elements in the buffer now
- [sgcl::async::receive_channel\<T\>](../receive_channel.md)
