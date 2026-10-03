[sgcl](../../README.md) › [core](../README.md) › [rooted](README.md)

# sgcl::rooted\<T\>::ptr

```cpp
tracked_ptr<T> ptr() const noexcept;
```

Returns the value's managed object as a `tracked_ptr`, a copy, for code that lives where one may: a stack, a
managed object, a task.

## Parameters

None.

## Return value

A `tracked_ptr` to the value; null only for a `rooted` moved from.

## Complexity

Constant.

## Exceptions

None.

## Notes

The `tracked_ptr` keeps the value alive by itself, as any other: the value outlives the last `rooted` while it is
held.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <vector>

using namespace sgcl;

struct Job {
    int id;
};

int main() {
    std::vector<rooted<Job>> jobs;
    jobs.emplace_back(Job{1});
    tracked_ptr<Job> job = jobs.front().ptr();  // a stack word of its own
    jobs.clear();
    println("job {}", job->id);
}
```

Output:

```text
job 1
```

## See also

- [get](get.md): the address of the value
- [root_ptr](../root_ptr/README.md): the root under it
- [sgcl::rooted\<T\>](README.md)
