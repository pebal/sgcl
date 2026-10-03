[sgcl](../../README.md) › [core](../README.md) › [atomic_ref](../atomic_ref.md) › [tracked_ptr](README.md)

# sgcl::atomic_ref\<tracked_ptr\<T\>\>::notify_one

```cpp
void notify_one() noexcept;
```

Wakes one of the threads blocked in [wait](wait.md) on the word viewed, if there is one, as
`std::atomic_ref::notify_one` does. Any view of the same word wakes it. It is called after the store the waiting
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

struct Job {
    int id;
};

struct Queue {
    tracked_ptr<Job> next;
};

int main() {
    tracked_ptr queue = make_tracked<Queue>();
    thread worker([queue] {
        atomic_ref(queue->next).wait(nullptr);
        println("job {}", atomic_ref(queue->next).load()->id);
    });
    atomic_ref next(queue->next);  // another view of the same word
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
- [sgcl::atomic_ref\<tracked_ptr\<T\>\>](README.md)
