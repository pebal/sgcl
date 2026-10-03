[sgcl](../../README.md) › [core](../README.md) › [list](../list.md)

# sgcl::list\<T\>::operator=

```cpp
/*(1)*/ list& operator=(const list& other);
/*(2)*/ list& operator=(list&& other) noexcept;
/*(3)*/ list& operator=(std::initializer_list<T> ilist);
```

Replaces the contents of the list.

1. Copies the elements of `other`, as [assign](assign.md)`(other.begin(), other.end())`: the elements this list has
   are assigned over in their nodes, the surplus erased, the missing ones appended. An assignment of a list to
   itself does nothing.
2. Destroys the elements of this list and takes the sentinel of `other` over, with every node; `other` is empty
   after, without a sentinel. No element of `other` is touched.
3. Copies the elements of `ilist`, as `assign(ilist)`.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the list the elements are copied or taken from |
| `ilist` | the list of values to copy |

## Return value

`*this`.

## Complexity

- (1) Linear in `size()` and `other.size()`.
- (2) Linear in `size()`, the elements destroyed.
- (3) Linear in `size()` and `ilist.size()`.

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
    list a = {1, 2, 3};
    list<int> b;

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
- [(constructor)](list.md): constructs a list
- [sgcl::list\<T\>](../list.md)
