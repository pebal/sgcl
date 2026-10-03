[sgcl](../../README.md) › [async](../README.md) › [receive_channel](../receive_channel.md)

# sgcl::async::receive_channel\<T\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether there is nothing to receive: no element in the buffer and no sender waiting with one, as the
channel's [empty](../channel/empty.md) says. With other threads at work the answer may have changed by the time it
is read. The same for `receive_channel<void>`.

## Parameters

None.

## Return value

`true` when the buffer is empty and no sender waits, `false` otherwise.

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
    async::channel<int> numbers(2);
    async::receive_channel<int> in = numbers;
    println("{}", in.empty());
    numbers.send(1).wait();
    println("{}", in.empty());
}
```

Output:

```text
true
false
```

## See also

- [size](size.md): the number of elements in the buffer alone
- [sgcl::async::receive_channel\<T\>](../receive_channel.md)
