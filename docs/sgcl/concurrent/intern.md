# sgcl::concurrent::intern, sgcl::concurrent::intern_string

```cpp
#include "sgcl/concurrent/intern.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T, class Hash = std::hash<T>, class KeyEqual = std::equal_to<T>>
    class intern;
    string concurrent::intern_string(std::string_view s);
}
```

`concurrent::intern<T>` is a pool where equal values share one managed object: Go's `unique` package, Java's `String.intern`. `make(value)` is the canonical object of the value, the one the pool holds when it is alive, or a new one made from the value and entered; so a program holds one copy of each distinct value it interns and compares two by their identity, a pointer comparison, where a comparison of the contents would cost a walk. The values of a field that repeats across many records (a host, a tag, a symbol, a type name), the keys of a large map, the atoms of an interpreter.

The pool does not keep its objects alive. An entry is a [`weak_ptr`](../core/weak_ptr.md) to the canonical object, placed in the lock-free hash set of [concurrent::sorted_set](sorted_set.md) by the hash of the object's contents and compared through the live object: to find a value, the table hashes it, walks to the entries of that hash, locks each one's weak pointer and compares the contents. An object nobody holds any more is collected, and its entry is dead from then on: never found, since it equals nothing, and never in the way of the entry that replaces it, which the next `make` of that value adds beside it; the dead entries are swept out every so many insertions, by the inserting thread, the way the [weak containers](weak_map.md) do it (as many insertions as the pool has entries, 16 at least, one sweep at a time, none waiting), and on `sweep()`. An entry keeps the hash it was placed with, because the object it would be computed from may be gone by the time the entry is erased, and the hash has its top bit set, so that the word is never taken for a heap address by a conservative scan.

`find` is wait-free and never writes; `of` and `make` are lock-free: a `make` that finds no live object makes one, enters its entry with the table's compare-exchange, and, when another thread's entry for the same value got in first, hands back that thread's object and lets its own die. So two threads interning the same new value at once both get one object, and no lock is taken anywhere. With a transparent `Hash` and `KeyEqual` (`is_transparent`, as `std::hash` and `std::equal_to` of a [string](../core/string.md) are) the lookups take a value of another type and build none: a `string_view` or a literal finds, and interns, a string with no string made for the search.

For `sgcl::string` the interned string is the string itself. A string is one word pointing at an object never modified, compared by identity first, so the pool of strings keeps its entries on the strings' own objects rather than on objects holding strings: `concurrent::intern<string>::make(view)` hands back a `string`, not a pointer to one, the string made from the view when its value is new; a `string` passed in enters the pool as it is when its value is new (Java's `String.intern`: the string itself, not a copy), and the empty string, which is null, is one value with no object, never entered and never looked up. `concurrent::intern_string(view)` is `concurrent::intern<string>::make(view)` under a name that reads.

## Rules

- A pool holds tracked pointers (the table's array, head and counters), so it lives on a thread's stack or inside a managed object ([The rules](../core/README.md#the-rules), 1). The default pool of a type, `pool()`, is a managed object under a `root_ptr`, made on first use: what a global may hold ([root_ptr](../core/root_ptr.md)); `make(value)` is `get` on it.
- The handle is a `tracked_ptr<const T>` (a `string` for a pool of strings): the object is immutable, shared by everyone who interned the value, and alive for as long as anyone holds it. Its identity is the value's: two handles compare equal exactly when the values were equal at the time both were made and neither object died between (a value made again after its object died is a new object).
- An entry is dead once a cycle has found its object unreachable; between the object becoming unreachable and that cycle, `make` still hands the object out, alive again: the lag of any garbage collector, and no harm.
- `T` is copied into its object by `make` (`T(value)` for a `value` of type `T`, or of any type `T` is constructible from); `Hash` and `KeyEqual` are default-constructed, and see the contents through the handle: `KeyEqual(const T&, const K&)` for a lookup by a `K`.
- A string longer than a page (64 KB) lives in a buffer of bytes rather than a slot, and a pointer to it cannot be made from its address in debug builds (the assertion of `tracked_ptr` from a raw pointer, which `atomic<string>` meets too): such strings intern in release builds only.
- Non-copyable, non-movable: a shared structure has one place.

## Members

### Types

```cpp
using value_type = T;
using handle = tracked_ptr<const T>;   // basic_string<CharT, Traits> for a pool of strings
using hasher = Hash;
using key_equal = KeyEqual;
using size_type = size_t;
```

### Constructors

```cpp
intern();
intern(const intern&) = delete;
```

A pool of its own, for the values of one subsystem, or when the default pool's lifetime, the program's, is too long:

```cpp
struct Symbols {
    concurrent::intern<string> names;   // inside a managed object, as any tracked pointer
};
concurrent::intern<Point, PointHash> points;   // on the stack
```

### get, find

```cpp
handle of(const T& value);
template<class K> handle of(const K& value);            // when Hash and KeyEqual are transparent
handle find(const T& value) const noexcept;
template<class K> handle find(const K& value) const noexcept;
```

`of` is the canonical object of the value (a name that enters, where `get` would only read): the pool's when one is alive, or a new one made from the value and entered. `find` is the same object when one is alive, or null (the empty string for a pool of strings), and never makes one.

```cpp
tracked_ptr<const Point> a = points.of({1, 2});
tracked_ptr<const Point> b = points.of({1, 2});
assert(a == b);                                  // one object
assert(points.find({2, 1}) == nullptr);
```

### make, pool

```cpp
static intern& pool();
static handle make(const T& value);
template<class K> static handle make(const K& value);   // when Hash and KeyEqual are transparent
```

The default pool of the type, one for the program, and `of` on it: Go's `unique.Make`.

```cpp
tracked_ptr<const Point> p = concurrent::intern<Point, PointHash>::make({1, 2});
string host = concurrent::intern<string>::make(view);   // the string itself, made if its value is new
```

### intern_string

```cpp
string concurrent::intern_string(std::string_view s);
```

`concurrent::intern<string>::make(s)`: the string of these characters, interned in the default pool of strings, the one every thread holds for them; a `string_view` or a literal, no string made when the value is known.

```cpp
string a = concurrent::intern_string("alpha");
string line = "alpha 512";
string b = concurrent::intern_string(line.as_slice(0, 5));   // a slice of another string: no string made when the value is known
assert(a.object() == b.object());                // the same object: compared in one word
```

### size, empty, sweep, clear, reserve

```cpp
size_type size() const noexcept;
bool empty() const noexcept;
size_type sweep();
void clear();
void reserve(size_type count);
```

The entries, the dead ones not yet swept included; `sweep()` drops the dead ones and returns how many (0 at once when another thread's sweep is under way); `clear()` forgets every object, the live ones included, which live on where they are held and are made again by the next `make`; `reserve` grows the table's array up front.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

struct Record {
    string host;    // one of a few values, repeated in every record
    int bytes;
};

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
    // Log records parsed by several threads: the host field takes one of a
    // few values, so every record holds the one interned string of its
    // host, and records of the same host share the object
    concurrent::queue<tracked_ptr<Record>> records;
    vector<thread> parsers;
    for (int t : range(4)) {
        parsers.emplace_back([&, t] {
            for (int i : range(1000)) {
                string line = (i + t) % 2 ? "alpha.example 512" : "beta.example 1024";   // a line read from a file
                string_slice host = line.as_slice(0, line.find(' '));
                records.push(make_tracked<Record>(concurrent::intern_string(host), 512));   // no string made once the host is known
            }
        });
    }
    for (auto& p : parsers) {
        p.join();
    }
    string alpha = concurrent::intern_string("alpha.example");   // the object the parsers got
    int of_alpha = 0;
    while (auto r = records.try_pop()) {
        if ((*r)->host.object() == alpha.object()) {   // compared by identity: one word
            ++of_alpha;
        }
    }
    std::cout << of_alpha << " records of alpha.example, " << concurrent::intern<string>::pool().size() << " strings in the pool\n";

    // A pool of values: one object per distinct value, compared by pointer
    concurrent::intern<Point, PointHash> points;
    tracked_ptr<const Point> a = points.of({1, 2});
    tracked_ptr<const Point> b = points.of({1, 2});
    tracked_ptr<const Point> c = points.of({2, 1});
    std::cout << (a == b) << ' ' << (a == c) << ' ' << points.size() << " points\n";
}
```

The output:

```
2000 records of alpha.example, 2 strings in the pool
1 0 2 points
```

## See also

- [Benchmarks](benchmarks.md#the-bounded-queue-the-priority-queue-intern-and-the-weak-map): measured against Go and Java

- [concurrent::weak_map](weak_map.md), [concurrent::weak_set](weak_set.md): the containers keyed by the identity of objects held weakly, whose sweep the pool shares
- [string](../core/string.md): one word, compared by identity first, hashed once; [weak_ptr](../core/weak_ptr.md): the entry
- [concurrent::sorted_set](sorted_set.md): the table underneath; [root_ptr](../core/root_ptr.md): how the default pool is held
- README: [Lock-free containers](README.md#lock-free-containers)
- `tests/concurrent/intern.cpp`: every behaviour above, checked, with the threads.
