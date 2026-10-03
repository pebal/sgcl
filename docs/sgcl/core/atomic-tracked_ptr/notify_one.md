[sgcl](../../README.md) › [core](../README.md) › [atomic](../atomic.md) › [tracked_ptr](README.md)

# sgcl::atomic\<tracked_ptr\<T\>\>::notify_one

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

struct Job {
    int id;
};

int main() {
    atomic<tracked_ptr<Job>> next;
    thread worker([&next] {
        next.wait(nullptr);
        println("job {}", next.load()->id);
    });
    next = make_tracked<Job>(7);
    next.notify_one();
    worker.join();
}
```

Output:

```text
job 7
```

## See also

- [notify_all](notify_all.md): wakes every waiting thread
- [wait](wait.md): blocks while the pointer is the one given
- [sgcl::atomic\<tracked_ptr\<T\>\>](README.md)
