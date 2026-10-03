[sgcl](../../README.md) › [core](../README.md) › [queue](README.md)

# sgcl::queue\<T, Container\>::front

```cpp
reference front() noexcept(noexcept(c.front()));                // (1)
const_reference front() const noexcept(noexcept(c.front()));    // (2)
```

Returns a reference to the first element, the oldest one, the next [pop](pop.md) removes: `c.front()`. The queue
must not be empty.

## Parameters

None.

## Return value

A reference to the first element.

## Complexity

Constant.

## Exceptions

What the container's `front` throws; none for `deque` and `list`.

## Notes

`front` on an empty queue is undefined behaviour, as with `std::queue`. The reference is valid as long as the
container's would be: for `deque` and `list`, until the element is popped or the queue is destroyed. It does not
keep the element alive; a `tracked_ptr` copied out of the queue does keep the object it points to.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    queue<tracked_ptr<string>> jobs;
    jobs.push(make_tracked<string>("build"));
    jobs.push(make_tracked<string>("test"));

    tracked_ptr next = jobs.front();
    jobs.pop();  // the string lives on in `next`
    println("{}, then {}", *next, *jobs.front());
}
```

Output:

```text
build, then test
```

## See also

- [back](back.md): access the last element
- [pop](pop.md): removes the first element
- [sgcl::queue\<T, Container\>](README.md)
