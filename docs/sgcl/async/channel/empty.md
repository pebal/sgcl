[sgcl](../../README.md) › [async](../README.md) › [channel](README.md)

# sgcl::async::channel\<T\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether there is nothing to receive: no element in the buffer and no sender waiting with one. With other
threads at work the answer may have changed by the time it is read. The same for `channel<void>`.

## Parameters

None.

## Return value

`true` when the buffer is empty and no sender waits, `false` otherwise.

## Complexity

Constant: the ring's head and tail, and the count of the waiting senders kept beside their list; on a rendezvous,
which keeps no count, a look at the head of the list.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::channel<int> numbers(2);
    println("{}", numbers.empty());
    numbers.send(1).wait();
    println("{}", numbers.empty());
    numbers.try_receive();
    println("{}", numbers.empty());
}
```

Output:

```text
true
false
true
```

## See also

- [size](size.md): the number of elements in the buffer alone
- [closed](closed.md): whether the stream has ended
- [sgcl::async::channel\<T\>](README.md)
