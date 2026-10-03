[sgcl](../../README.md) › [core](../README.md) › [tracked_ptr](README.md)

# sgcl::tracked_ptr\<T\>::operator tracked_ptr\<void\>&

```cpp
operator tracked_ptr<void>&() noexcept;                // (1)
operator const tracked_ptr<void>&() const noexcept;    // (2)
```

Every `tracked_ptr<T>` is a `tracked_ptr<void>&`: the same word seen without its type, so a function taking a
`tracked_ptr<void>&` takes any of them. [type](type.md), [is](is.md) and [as](as.md) still work on it, since the
type is the object's, not the pointer's. A `tracked_ptr<void>` by value is made through the converting
constructor (`T*` converts to `void*`).

## Parameters

None.

## Return value

This pointer, as a reference to a `tracked_ptr<void>`.

## Complexity

Constant.

## Exceptions

None.

## Notes

The containers of pointers store this word: one type of node for every `T`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

bool holds_int(const tracked_ptr<void>& p) {
    return p.is<int>();
}

int main() {
    tracked_ptr number = make_tracked<int>(1);
    tracked_ptr<void>& ref = number;  // the same word
    tracked_ptr<void> copy = number;
    println("{} {} {}", ref.is<int>(), copy.is<int>(), holds_int(number));

    ref = nullptr;
    println("{}", number == nullptr);
}
```

Output:

```text
true true true
true
```

## See also

- [(constructor)](tracked_ptr.md): constructs the pointer, a `tracked_ptr<void>` from any of them
- [type](type.md): the type the object was created with
- [sgcl::tracked_ptr\<T\>](README.md)
