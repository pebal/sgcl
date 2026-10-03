[sgcl](../../README.md) › [core](../README.md) › [tracked_ptr](../tracked_ptr.md)

# sgcl::tracked_ptr\<T\>::get

```cpp
element_type* get() const noexcept;
```

Returns the raw pointer.

## Parameters

None.

## Return value

The address the pointer holds, null for a null pointer.

## Complexity

Constant.

## Exceptions

None.

## Notes

A raw pointer keeps nothing alive by itself: it is valid while a `tracked_ptr`, a `unique_ptr` or a container keeps
its target, as with `std` ([The rules](../README.md#the-rules), 3). A raw pointer that stays in a stack frame does
keep its target until the word is overwritten, since the stack is scanned conservatively; that is a delay, not a
guarantee ([Stack roots](../../../garbage_collector/overview.md#stack-roots)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Item {
    int value = 1;
};

void bump(Item* item) {
    ++item->value;
}

int main() {
    tracked_ptr item = make_tracked<Item>();
    bump(item.get());  // valid while item keeps the object
    println("{} {}", item->value, tracked_ptr<Item>().get() == nullptr);
}
```

Output:

```text
2 true
```

## See also

- [operator\*, operator->](operator_deref.md): the object
- [sgcl::tracked_ptr\<T\>](../tracked_ptr.md)
