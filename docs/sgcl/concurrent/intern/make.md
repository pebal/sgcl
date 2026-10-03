[sgcl](../../README.md) › [concurrent](../README.md) › [intern](../intern.md)

# sgcl::concurrent::intern\<T, Hash, KeyEqual\>::make

```cpp
/*(1)*/ static handle make(const T& value) noexcept(std::is_nothrow_constructible_v<T, const T&>);
/*(2)*/ template<class K> static handle make(const K& value)
            noexcept(std::is_nothrow_constructible_v<T, const K&>);
```

Returns the canonical object of `value` in the default pool of the type: `pool().of(value)`, Go's `unique.Make`.

1. Interns `value`.
2. Interns a value of another type, with no `T` built for the search: a `string_view` or a literal for strings.
   Takes part only when `Hash` and `KeyEqual` both have `is_transparent`.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value to intern |

## Return value

The canonical object: a `tracked_ptr<const T>`, or the string itself for strings.

## Complexity

As [of](of.md): constant on average.

## Exceptions

What the construction of `T` from `value` throws; none when it is noexcept.

## Notes

Lock-free, as `of` is. Every thread that makes a value gets the one object, alive while anyone holds it: the values
the whole program shares need no pool of their own.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Point {
    int x, y;
    bool operator==(const Point&) const = default;
};

struct PointHash {
    size_t operator()(const Point& p) const noexcept {
        return size_t(p.x) * 1000003 + size_t(p.y);
    }
};

using Points = concurrent::intern<Point, PointHash>;

int main() {
    tracked_ptr<const Point> a = Points::make({1, 2});
    tracked_ptr<const Point> b = Points::pool().of({1, 2});
    println("{} {}", a == b, a->y);

    string host = concurrent::intern<string>::make("alpha.example");
    println("{}", host);
}
```

Output:

```text
true 2
alpha.example
```

## See also

- [of](of.md): the canonical object in a pool of one's own
- [pool](pool.md): the default pool
- [intern_string](../intern_string.md): `make` for strings, under a name that reads
- [sgcl::concurrent::intern\<T, Hash, KeyEqual\>](../intern.md)
