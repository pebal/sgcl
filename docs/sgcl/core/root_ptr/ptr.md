[sgcl](../../README.md) › [core](../README.md) › [root_ptr](../root_ptr.md)

# sgcl::root_ptr\<T\>::ptr, operator tracked_ptr\<T\>&

```cpp
/*(1)*/ tracked_ptr<T>& ptr() noexcept;
/*(2)*/ const tracked_ptr<T>& ptr() const noexcept;
/*(3)*/ operator tracked_ptr<T>&() noexcept;
/*(4)*/ operator const tracked_ptr<T>&() const noexcept;
```

The `tracked_ptr` the root holds its object by: the cell's word, inside a managed object, where a `tracked_ptr`
lives. A reference, not a copy: every read through it is the root's read and every store the root's store, with
its barrier.

- (1–2) By name.
- (3–4) The same reference as a conversion: a `root_ptr<T>` is passed as it is where a `tracked_ptr<T>&` or a
  `const tracked_ptr<T>&` is taken.

## Parameters

None.

## Return value

A reference to the cell's `tracked_ptr`.

## Complexity

Constant.

## Exceptions

None.

## Notes

The reference is for code that lives where a `tracked_ptr` may, and for an [atomic_ref](../atomic_ref.md) over
the root: `atomic_ref a(root)` is the atomic of the cell's word, for threads that store into and load from the same
`root_ptr`. A `tracked_ptr` of a base class is made from a `root_ptr` of a derived class by
its own constructor ([tracked_ptr](../tracked_ptr/tracked_ptr.md)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int value;
};

root_ptr<Node> current;  // a global, where a tracked_ptr may not live

int value_of(const tracked_ptr<Node>& node) {
    return node->value;
}

int main() {
    atomic_ref word(current);  // the atomic of the root: the cell's word
    word.store(make_tracked<Node>(3));
    tracked_ptr<Node> seen = word.load();

    current.ptr() = make_tracked<Node>(4);
    println("{} {}", seen->value, value_of(current));
}
```

Output:

```text
3 4
```

## See also

- [get](get.md): the raw pointer
- [atomic_ref](../atomic_ref.md): the atomic of the root
- [sgcl::root_ptr\<T\>](../root_ptr.md)
