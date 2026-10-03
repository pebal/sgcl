[sgcl](../../README.md) › [async](../README.md) › [channel](../channel.md)

# sgcl::async::channel\<T\>::close

```cpp
void close() const;
```

Closes the channel: the end of the stream. Every receiver waiting is woken and gets nothing, every sender waiting is
woken and gets `false` with its element undelivered, and every later send gives `false`. What is in the buffer is
still received, then every receive gives nothing at once. A select case on the channel is served at once from then
on ([on_receive](on_receive.md), [on_send](on_send.md)). A second `close()` does nothing.

The same for `channel<void>`.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of waiters, each woken.

## Exceptions

`std::system_error` when the close wakes a waiting task and the wake starts the scheduler's workers, one of which
cannot be started.

## Notes

The closing side is the sending one, as in Go: a receiver learns of the end by the empty `optional`, a range-for
by its end. A send to a closed channel is not an error here (Go panics): it gives `false`.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> count(async::channel<int> numbers) {
    int n = 0;
    while (co_await numbers.receive()) {
        ++n;
    }
    co_return n;  // woken by the close
}

int main() {
    async::channel<int> numbers;
    async::task<int> counting = async::spawn(count(numbers));
    for (int i : range(3)) {
        numbers.send(i).wait();
    }
    numbers.close();
    numbers.close();  // nothing
    println("{} received", counting.wait());
    println("{} {}", numbers.closed(), numbers.send(3).wait());
}
```

Output:

```text
3 received
true false
```

## See also

- [closed](closed.md): checks whether the channel is closed
- [receive](receive.md), [send](send.md): what they give on a closed channel
- [sgcl::async::channel\<T\>](../channel.md)
