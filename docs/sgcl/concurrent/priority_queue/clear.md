[sgcl](../../README.md) › [concurrent](../README.md) › [priority_queue](README.md)

# sgcl::concurrent::priority_queue\<T, Compare\>::clear

```cpp
void clear() noexcept;
```

Destroys every element, under the lock; the heap's buffer stays for the next pushes.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the size: one destructor per element.

## Exceptions

None.

## Notes

The elements are destroyed at once, on the calling thread, while the lock is held: a pop that waits for the lock
finds the queue empty, and the elements' objects held by `tracked_ptr` are the collector's from then on, unless
something else holds them. The numbering of the pushes goes on from where it was.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::priority_queue<string> words = {"pear", "apple", "fig"};
    words.clear();
    println("{} {}", words.empty(), words.size());
}
```

Output:

```text
true 0
```

## See also

- [try_pop](try_pop.md): takes one element
- [sgcl::concurrent::priority_queue\<T, Compare\>](README.md)
