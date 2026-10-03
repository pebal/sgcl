[sgcl](../../README.md) › [core](../README.md) › [thread](README.md)

# sgcl::thread::detach

```cpp
void detach();
```

Lets the thread run on independently of the object: after the call the object stands for no thread, and the
thread's resources are released when it finishes. The closure stays in its managed node, a root, until the
function returns, so a `tracked_ptr` the thread was given by value keeps its object for as long as the thread
needs it.

## Parameters

None.

## Return value

None.

## Complexity

Constant.

## Exceptions

`std::system_error`, as `std::thread::detach` throws it, when the object stands for no thread.

## Notes

A detached thread is still a thread to the collector: its stack is scanned until it ends. When `main` returns
while it runs, the process ends with it, as with `std::thread`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Mailbox {
    atomic<int> value = 0;
};

int main() {
    tracked_ptr box = make_tracked<Mailbox>();
    thread worker([box] {
        box->value = 7;
        box->value.notify_one();
    });
    worker.detach();  // the closure keeps the mailbox, not this frame
    println("{}", worker.joinable());

    box->value.wait(0);
    println("{}", box->value.load());
}
```

Output:

```text
false
7
```

## See also

- [join](join.md): waits for the thread to finish
- [sgcl::thread](README.md)
