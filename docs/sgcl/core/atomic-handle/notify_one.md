[sgcl](../../README.md) › [core](../README.md) › [atomic](../atomic.md) › [handle](../atomic-handle.md)

# sgcl::atomic\<H\>::notify_one

```cpp
void notify_one() noexcept;
```

Wakes one of the threads blocked in [wait](wait.md) on this atomic, if there is one, as `std::atomic::notify_one`
does. It is called after the store the waiting thread waits for.

## Parameters

None.

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    atomic<string> reply;
    string none = reply.load();
    thread client([&reply, &none] {
        reply.wait(none);
        println("reply: {}", reply.load());
    });
    reply = string("pong");
    reply.notify_one();
    client.join();
}
```

Output:

```text
reply: pong
```

## See also

- [notify_all](notify_all.md): wakes every waiting thread
- [wait](wait.md): blocks while the handle holds the object given
- [sgcl::atomic\<H\>](../atomic-handle.md)
