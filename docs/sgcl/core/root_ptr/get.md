[sgcl](../../README.md) › [core](../README.md) › [root_ptr](README.md)

# sgcl::root_ptr\<T\>::get

```cpp
element_type* get() const noexcept;
```

Returns the raw pointer: the `get()` of the cell's `tracked_ptr`, one indirection away.

## Parameters

None.

## Return value

The address the root holds, null for a null root.

## Complexity

Constant.

## Exceptions

None.

## Notes

A raw pointer keeps nothing alive by itself: it is valid while the root, or anything else, keeps its target
([The rules](../README.md#the-rules), 3).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Counter {
    int hits = 0;
};

void hit(Counter* counter) {
    ++counter->hits;
}

int main() {
    root_ptr<Counter> counter = make_tracked<Counter>();
    hit(counter.get());
    hit(counter.get());
    println("{} {}", counter->hits, root_ptr<Counter>().get() == nullptr);
}
```

Output:

```text
2 true
```

## See also

- [operator\*, operator->](operator_deref.md): the object
- [ptr, operator tracked_ptr\<T\>&](ptr.md): the cell's word
- [sgcl::root_ptr\<T\>](README.md)
