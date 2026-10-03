[sgcl](../../README.md) › [core](../README.md) › [forward_list](../forward_list.md)

# sgcl::forward_list\<T\>::operator=

```cpp
forward_list& operator=(const forward_list& other);         // (1)
forward_list& operator=(forward_list&& other) noexcept;     // (2)
forward_list& operator=(std::initializer_list<T> ilist);    // (3)
```

Replaces the contents of the list.

1. Copies the elements of `other`, as [assign](assign.md)`(other.begin(), other.end())`: the elements this list has
   are assigned over in their nodes, the surplus erased, the missing ones appended. An assignment of a list to
   itself does nothing.
2. Destroys the elements of this list and takes the chain of nodes of `other` over; `other` is empty after. No
   element of `other` is touched.
3. Copies the elements of `ilist`, as `assign(ilist)`.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the list the elements are copied or taken from |
| `ilist` | the list of values to copy |

## Return value

`*this`.

## Complexity

- (1) Linear in the number of elements of this list and of `other`.
- (2) Linear in the number of elements of this list, destroyed.
- (3) Linear in the number of elements of this list and in `ilist.size()`.

## Exceptions

- (1), (3) What the copy assignment and the copy constructor of `T` throw.
- (2) None.

If an exception is thrown, the list stays valid: the elements assigned over before it keep their new values, and
none of the missing ones is appended.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    forward_list a = {1, 2, 3};
    forward_list<int> b;

    b = a;  // a copy, in nodes of its own
    println("{} {}", a, b);

    b = {4, 5};
    println("{}", b);

    b = std::move(a);
    println("{} {}", a, b);
}
```

Output:

```text
[1, 2, 3] [1, 2, 3]
[4, 5]
[] [1, 2, 3]
```

## See also

- [assign](assign.md): replaces the contents with copies of a value or a range
- [(constructor)](forward_list.md): constructs a list
- [sgcl::forward_list\<T\>](../forward_list.md)
