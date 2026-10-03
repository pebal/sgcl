[sgcl](../../README.md) › [core](../README.md) › [list](../list.md)

# sgcl::list\<T\>::pop_front

```cpp
void pop_front() noexcept;
```

Destroys the first element and unlinks its node, which is left to the collector. The list must not be empty: on an
empty list the call is undefined, and debug builds assert.

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
    list<string> jobs = {"first", "second", "third"};
    while (!jobs.empty()) {
        println("{}", jobs.front());
        jobs.pop_front();
    }
}
```

Output:

```text
first
second
third
```

## See also

- [push_front](push_front.md): inserts an element at the beginning
- [pop_back](pop_back.md): removes the last element
- [sgcl::list\<T\>](../list.md)
