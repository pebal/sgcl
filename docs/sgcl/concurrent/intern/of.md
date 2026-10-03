[sgcl](../../README.md) › [concurrent](../README.md) › [intern](README.md)

# sgcl::concurrent::intern\<T, Hash, KeyEqual\>::of

```cpp
handle of(const T& value) noexcept(std::is_nothrow_constructible_v<T, const T&>);    // (1)
template<class K> handle of(const K& value)                                          // (2)
    noexcept(std::is_nothrow_constructible_v<T, const K&>);
```

Returns the canonical object of `value`: the pool's when one is alive, or a new one made from the value and
entered. The name says what a value maps to: `get` would only read, and `of` enters.

1. Interns `value`.
2. Interns a value of another type, with no `T` built for the search: a `string_view` or a literal for a pool of
   strings. Takes part only when `Hash` and `KeyEqual` both have `is_transparent`.

One search finds the entry of a live object of the value, which is the answer. When there is none, a new object is
made, once, and entered unless another thread's entry got in first, whose object is handed back then and this one
dropped; an entry found equal whose object has died since is passed over by the next try. For a pool of strings a
`string` passed in enters the pool as it is when its value is new, a view becomes a string first, and the empty
string is handed back as it is, never entered.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value to intern |

## Return value

The canonical object: a `tracked_ptr<const T>`, or the string itself for a pool of strings.

## Complexity

Constant on average: a search of the hash set, and for a new value the object made and the insertion. One
insertion in as many as the pool has entries (16 at least) also runs a sweep, linear in the number of entries.

## Exceptions

What the construction of `T` from `value` throws; none when it is noexcept.

If an exception is thrown, nothing is entered and the pool is as it was.

## Notes

Lock-free, and linearizable at the table's compare-exchange that links the entry: two threads interning the same new
value at once both get the object of the one whose entry won, the other object being garbage. The insertion that
brings the count of insertions since the last sweep to the threshold runs the sweep on its thread, unless another
thread's sweep is under way, in which case it goes on without waiting.

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

int main() {
    concurrent::intern<Point, PointHash> points;
    tracked_ptr<const Point> a = points.of({1, 2});
    tracked_ptr<const Point> b = points.of({1, 2});
    tracked_ptr<const Point> c = points.of({2, 1});
    println("{} {} {}", a == b, a == c, points.size());

    concurrent::intern<string> words;
    string mine = "gamma";
    string interned = words.of(mine);  // new: the string itself enters the pool
    println("{}", interned.object() == mine.object());
}
```

Output:

```text
true false 2
true
```

## See also

- [find](find.md): the canonical object when one is alive, never made
- [make](make.md): `of` on the default pool
- [sgcl::concurrent::intern\<T, Hash, KeyEqual\>](README.md)
