[sgcl](../../README.md) › [async](../README.md) › [channel](README.md)

# sgcl::async::channel\<T\>::size

```cpp
size_type size() const noexcept;
```

The number of elements in the buffer: Go's `len(ch)`. The elements held by waiting senders are not counted, so the
size of a rendezvous is 0 but for the moment an element passes through its ring. With other threads at work the
number may have changed by the time it is read. The same for `channel<void>`, which counts its signals.

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
    async::channel<int> numbers(8);
    numbers.send(1).wait();
    numbers.send(2).wait();
    println("{}", numbers.size());

    numbers.try_receive();
    println("{}", numbers.size());

    async::channel<void> ticks(4);
    ticks.send().wait();
    println("{}", ticks.size());
}
```

Output:

```text
2
1
1
```

## See also

- [empty](empty.md): also counts the waiting senders
- [capacity](capacity.md): the number of elements the buffer holds
- [sgcl::async::channel\<T\>](README.md)
