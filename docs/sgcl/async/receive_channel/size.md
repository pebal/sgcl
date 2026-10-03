[sgcl](../../README.md) › [async](../README.md) › [receive_channel](README.md)

# sgcl::async::receive_channel\<T\>::size

```cpp
size_type size() const noexcept;
```

The number of elements in the buffer, Go's `len(ch)`, as the channel's [size](../channel/size.md) counts them: the
elements held by waiting senders are not counted. With other threads at work the number may have changed by the time
it is read. The same for `receive_channel<void>`, which counts the signals.

## Parameters

None.

## Return value

The number of elements in the buffer.

## Complexity

Constant: two atomic loads, the ring's head and tail.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::channel<int> numbers(4);
    async::receive_channel<int> in = numbers;
    numbers.send(1).wait();
    numbers.send(2).wait();
    println("{}", in.size());
}
```

Output:

```text
2
```

## See also

- [capacity](capacity.md): the number the buffer holds
- [empty](empty.md): whether there is nothing to receive
- [sgcl::async::receive_channel\<T\>](README.md)
