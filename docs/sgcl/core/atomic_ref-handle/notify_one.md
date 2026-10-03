[sgcl](../../README.md) › [core](../README.md) › [atomic_ref](../atomic_ref.md) › [handle](README.md)

# sgcl::atomic_ref\<H\>::notify_one

```cpp
void notify_one() noexcept;
```

Wakes one of the threads blocked in [wait](wait.md) on the handle's word, if there is one, as
`std::atomic_ref::notify_one` does. Any view of the same handle wakes it. It is called after the store the waiting
thread waits for.

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

struct Call {
    string reply;
};

int main() {
    tracked_ptr call = make_tracked<Call>();
    string none = atomic_ref(call->reply).load();
    thread client([call, none] {
        atomic_ref(call->reply).wait(none);
        println("reply: {}", atomic_ref(call->reply).load());
    });
    atomic_ref reply(call->reply);  // another view of the same handle
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
- [sgcl::atomic_ref\<H\>](README.md)
