[sgcl](../../README.md) › [async](../README.md) › [channel](README.md)

# sgcl::async::channel\<T\>::capacity

```cpp
size_type capacity() const noexcept;
```

The number of elements the buffer holds, as given to the constructor: Go's `cap(ch)`. The ring under it may have
more slots (a power of two, at least 8); a send waits at the capacity all the same. The same for `channel<void>`.

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
    async::channel<int> meeting;
    async::channel<int> three(3);
    println("{} {}", meeting.capacity(), three.capacity());

    for (int i : {1, 2, 3, 4}) {
        if (!three.try_send(i)) {
            println("full at {}", three.size());
        }
    }
}
```

Output:

```text
0 3
full at 3
```

## See also

- [size](size.md): the number of elements in the buffer
- [(constructor)](channel.md): where the capacity is given
- [sgcl::async::channel\<T\>](README.md)
