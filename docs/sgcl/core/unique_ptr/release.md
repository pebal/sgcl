[sgcl](../../README.md) › [core](../README.md) › [unique_ptr](README.md)

# sgcl::unique_ptr\<T\>::release

```cpp
pointer release() noexcept;
```

Hands the raw pointer out and leaves the `unique_ptr` empty: the `release` of `std::unique_ptr`. The object is then
owned by nobody and is not tracked either, so `release()` is for handing the object to something that will own it:
another `unique_ptr`, through [reset](reset.md). A `tracked_ptr` does it by itself, in its constructor from a
`unique_ptr&&` ([(constructor)](../tracked_ptr/tracked_ptr.md)).

## Parameters

None.

## Return value

The address of the object, null when there was none.

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
    unique_ptr first = make_tracked<int>(5);
    unique_ptr<int> second;
    second.reset(first.release());  // the object changes owners
    println("{} {}", bool(first), *second);
}
```

Output:

```text
false 5
```

## See also

- [reset](reset.md): destroys the object, or replaces it
- [tracked_ptr](../tracked_ptr/README.md): takes the object from a `unique_ptr&&` for the collector
- [sgcl::unique_ptr\<T\>](README.md)
