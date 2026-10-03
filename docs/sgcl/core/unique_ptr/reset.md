[sgcl](../../README.md) › [core](../README.md) › [unique_ptr](README.md)

# sgcl::unique_ptr\<T\>::reset

```cpp
void reset(pointer p = pointer()) noexcept;
```

The `reset` of `std::unique_ptr`. `reset()` destroys the object, at once and on the calling thread, and leaves the
owner empty; `reset(p)` with a raw pointer to a managed object that nothing owns, one [release](release.md) handed
out, takes it over, destroying the object owned before. The word is nulled before the object is destroyed, as the
standard orders it.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the object to own: a managed object nothing owns, or null |

## Return value

None.

## Complexity

Constant, plus the destructor of the object destroyed.

## Exceptions

None.

## Notes

The destructor of `unique_ptr` is `reset()`: a dead member leaves its slot's pointer word null for the next object
of the type, as a `tracked_ptr`'s destructor does.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Point {
    ~Point() { println("Point {} destroyed", x); }
    int x, y;
};

int main() {
    unique_ptr p = make_tracked<Point>(1, 2);
    unique_ptr q = make_tracked<Point>(3, 4);
    p.reset(q.release());
    println("p holds {}", p->x);
    p.reset();
    println("{}", bool(p));
}
```

Output:

```text
Point 1 destroyed
p holds 3
Point 3 destroyed
false
```

## See also

- [release](release.md): hands the raw pointer out and leaves the owner empty
- [operator=](operator_assign.md): destroys the object, if any, and takes another over
- [sgcl::unique_ptr\<T\>](README.md)
