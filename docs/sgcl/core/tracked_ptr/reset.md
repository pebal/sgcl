[sgcl](../../README.md) › [core](../README.md) › [tracked_ptr](../tracked_ptr.md)

# sgcl::tracked_ptr\<T\>::reset

```cpp
void reset() noexcept;                   // (1)
void reset(element_type* p) noexcept;    // (2)
```

Replaces the pointer.

1. With null: `*this = nullptr`.
2. With a raw pointer, under the rules of the raw-pointer constructor: a managed object or a part of it, never an
   element of a container's buffer, never an object a `unique_ptr` owns. One store with its barrier, no temporary.
   Debug builds assert the rules.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the address of a managed object or of a part of it, or null |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Notes

(2) is what the containers use to relink the nodes they root.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Item {
    int value = 3;
};

int main() {
    tracked_ptr item = make_tracked<Item>();
    tracked_ptr<int> alias;
    alias.reset(&item->value);  // an alias into the Item
    println("{}", *alias);

    alias.reset();
    println("{}", alias == nullptr);
}
```

Output:

```text
3
true
```

## See also

- [operator=](operator_assign.md): assigns the pointer
- [(constructor)](tracked_ptr.md): the rules of the raw-pointer constructor
- [sgcl::tracked_ptr\<T\>](../tracked_ptr.md)
