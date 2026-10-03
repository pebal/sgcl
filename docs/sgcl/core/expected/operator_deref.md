[sgcl](../../README.md) › [core](../README.md) › [expected](../expected.md)

# sgcl::expected\<T, E\>::operator-\>, operator*

```cpp
const T* operator->() const;      // (1)
T* operator->();                  // (2)
const T& operator*() const&;      // (3)
T& operator*() &;                 // (4)
const T&& operator*() const&&;    // (5)
T&& operator*() &&;               // (6)
```

The value, checked: without a value, `bad_expected_access<E>` carrying a copy of the error, as
[value](value.md) throws it.

- (1–2) A pointer to the value.
- (3–6) A reference to the value, an rvalue reference from an rvalue `expected`.

`expected<void, E>` has `void operator*() const noexcept`, which returns nothing and checks nothing.

## Parameters

None.

## Return value

- (1–2) A pointer to the value.
- (3–6) A reference to the value.

## Complexity

Constant.

## Exceptions

`bad_expected_access<E>` when there is no value, and what the copy of the error into it throws.

## Notes

`std::expected`'s `*` and `->` without a value are undefined behaviour; here they are `value()` under another name
(DESIGN 220). Where the value is passed on as a `T`, neither is written: the `expected` converts to its value by
itself ([value, operator U](value.md)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Point {
    int x, y;
};

int main() {
    expected<Point, string> p = Point{1, 2};
    println("{} {}", p->x, (*p).y);

    expected<Point, string> missing = unexpected("no point");
    try {
        println("{}", missing->x);
    } catch (const bad_expected_access<string>& e) {
        println("{}", e.error());
    }
}
```

Output:

```text
1 2
no point
```

## See also

- [value, operator U](value.md): the value, by name and by conversion
- [operator bool, has_value](operator_bool.md): checks whether there is a value
- [sgcl::expected\<T, E\>](../expected.md)
