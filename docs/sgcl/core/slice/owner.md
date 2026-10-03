[sgcl](../../README.md) › [core](../README.md) › [slice](README.md)

# sgcl::slice\<T\>::owner

```cpp
const tracked_ptr<const void>& owner() const noexcept;
```

The managed object the elements lie in, which the slice holds: the string, the buffer of a vector, the block of a
reader. Null for a slice of unmanaged memory. A slice made again of the same owner and a piece of its range,
`slice(s.owner(), s.data(), n)`, is a piece of the same object.

## Parameters

None.

## Return value

A reference to the owner's tracked pointer.

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
    string text = "abcdef";
    string_slice whole = text;
    string_slice piece(whole.owner(), whole.data() + 2, 3);  // the same owner, explicitly
    int local[2] = {};
    slice<int> unowned(local);
    println("{} {} {}", piece, piece.owner() == whole.owner(), unowned.owner() == nullptr);
}
```

Output:

```text
cde true true
```

## See also

- [owned](owned.md): checks whether there is an owner
- [(constructor)](slice.md): a slice of an owner
- [sgcl::slice\<T\>](README.md)
