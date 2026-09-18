# Sgcl::Intern, Sgcl::InternString

```cpp
#include "sgcl/Sgcl/Concurrent/Intern.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T, class Hash = std::hash<T>, class Equal = std::equal_to<T>>
    class Intern;
    String InternString(std::string_view s);
}
```

The same class in the `sgcl` interface: [intern, intern_string](../../concurrent/intern.md).

`Intern<T>` is a pool where equal values share one managed object: Go's `unique` package, Java's `String.intern`, .NET's `String.Intern`. `Make(value)` is the canonical object of the value, the one the pool holds when it is alive, or a new one made from the value and entered; so a program holds one copy of each distinct value it interns and compares two by their identity, a pointer comparison, where a comparison of the contents would cost a walk. The values of a field that repeats across many records (a host, a tag, a symbol, a type name), the keys of a large dictionary, the atoms of an interpreter.

The pool does not keep its objects alive. An entry is a [`WeakPtr`](../Core/WeakPtr.md) to the canonical object, placed in the lock-free hash table of [ConcurrentHashSet](ConcurrentHashSet.md) by the hash of the object's contents and compared through the live object; an object nobody holds any more is collected, its entry is dead from then on, never found and never in the way of the entry that replaces it, and the dead entries are swept out every so many insertions by the inserting thread, the way the [weak containers](ConcurrentWeakDictionary.md) do it, and on `Sweep()`. `Find` is wait-free and never writes; `Get` and `Make` are lock-free, and two threads interning the same new value at once both get one object, the one whose entry won the table's compare-exchange. With a transparent `Hash` and `Equal` (as `std::hash` and `std::equal_to` of a [String](../Core/String.md) are) the lookups take a value of another type and build none: a `string_view` or a literal finds, and interns, a String with no String made for the search.

For `String` the interned string is the string itself: `Intern<String>::Make(view)` hands back a `String`, not a pointer to one, a `String` passed in enters the pool as it is when its value is new (Java's `String.intern`), and the empty String, which is null, is never entered and never looked up. `InternString(view)` is `Intern<String>::Make(view)` under a name that reads.

## Rules

- A pool holds tracked pointers, so it lives on a thread's stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1). The default pool of a type in this interface, `Pool()`, is a managed object under a `RootPtr`, made on first use ([RootPtr](../Core/RootPtr.md)); `Make(value)` is `Get` on it. It is a pool of its own, apart from `sgcl::intern<T>::pool()`.
- The handle is a `Ptr<const T>` (a `String` for a pool of Strings): the object is immutable, shared by everyone who interned the value, and alive for as long as anyone holds it. Two handles compare equal exactly when the values were equal at the time both were made and neither object died between.
- An entry is dead once a cycle has found its object unreachable; between the object becoming unreachable and that cycle, `Make` still hands the object out, alive again: the lag of any garbage collector, and no harm.
- `T` is copied into its object by `Make`; `Hash` and `Equal` are default-constructed and see the contents through the handle. A String longer than a page (64 KB) interns in release builds only, as [intern](../../concurrent/intern.md) explains.
- Non-copyable, non-movable: a shared structure has one place.

## Members

### Types

```cpp
using ValueType = T;
using HandleType = Ptr<const T>;               // String for Intern<String>
using InnerType = sgcl::intern<T, Hash, Equal>;   // sgcl::intern<sgcl::string, ...> for Intern<String>
using SizeType = size_t;
```

### Constructors

```cpp
Intern();
Intern(const Intern&) = delete;
```

A pool of its own, for the values of one subsystem, or when the default pool's lifetime, the program's, is too long:

```cpp
struct Symbols {
    Intern<String> names;                     // inside a managed object, as any tracked pointer
};
Intern<Point, PointHash> points;              // on the stack
```

### Get, Find

```cpp
HandleType Get(const T& value);
template<class K> HandleType Get(const K& value);           // when Hash and Equal are transparent
HandleType Find(const T& value) const noexcept;
template<class K> HandleType Find(const K& value) const noexcept;
```

`Get` is the canonical object of the value: the pool's when one is alive, or a new one made from the value and entered. `Find` is the same object when one is alive, or null (the empty String for a pool of Strings), and never makes one.

```cpp
Ptr<const Point> a = points.Get({1, 2});
Ptr<const Point> b = points.Get({1, 2});
assert(a == b);                               // one object
assert(points.Find({2, 1}) == nullptr);
```

### Make, Pool

```cpp
static Intern& Pool();
static HandleType Make(const T& value);
template<class K> static HandleType Make(const K& value);   // when Hash and Equal are transparent
```

The default pool of the type, one for the program, and `Get` on it: Go's `unique.Make`.

```cpp
Ptr<const Point> p = Intern<Point, PointHash>::Make({1, 2});
String host = Intern<String>::Make(view);     // the String itself, made if its value is new
```

### InternString

```cpp
String InternString(std::string_view s);
```

`Intern<String>::Make(s)`: the String of these characters, interned in the default pool of Strings, the one every thread holds for them; a `string_view` or a literal, no String made when the value is known.

```cpp
String a = InternString("alpha");
String line = "alpha 512";
String b = InternString(line.View(0, 5));   // a view of another string: no String made when the value is known
assert(a.Object() == b.Object());             // the same object: compared in one word
```

### Count, IsEmpty, Sweep, Clear, Reserve, Inner

```cpp
SizeType Count() const noexcept;
bool IsEmpty() const noexcept;
SizeType Sweep();
void Clear();
void Reserve(SizeType n);
InnerType& Inner() noexcept;
```

The entries, the dead ones not yet swept included; `Sweep()` drops the dead ones and returns how many (0 at once when another thread's sweep is under way); `Clear()` forgets every object, the live ones included, which live on where they are held and are made again by the next `Make`; `Reserve` grows the table's array up front; `Inner()` is the `sgcl::intern` underneath.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

struct Record {
    String host;    // one of a few values, repeated in every record
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
    ConcurrentQueue<Ptr<Record>> records;
    List<Thread> parsers;
    for (int t : Range(4)) {
        parsers.Emplace([&, t] {
            for (int i : Range(1000)) {
                String line = (i + t) % 2 ? "alpha.example 512" : "beta.example 1024";   // a line read from a file
                StringView host = line.View(0, line.IndexOf(' '));
                records.Enqueue(Make<Record>(InternString(host), 512));   // no string made once the host is known
            }
        });
    }
    for (auto& p : parsers) {
        p.Join();
    }
    String alpha = InternString("alpha.example");   // the object the parsers got
    int of_alpha = 0;
    while (auto r = records.TryDequeue()) {
        if ((*r)->host.Object() == alpha.Object()) {   // compared by identity: one word
            ++of_alpha;
        }
    }
    std::cout << of_alpha << " records of alpha.example, " << Intern<String>::Pool().Count() << " strings in the pool\n";

    // A pool of values: one object per distinct value, compared by pointer
    Intern<Point, PointHash> points;
    Ptr<const Point> a = points.Get({1, 2});
    Ptr<const Point> b = points.Get({1, 2});
    Ptr<const Point> c = points.Get({2, 1});
    std::cout << (a == b) << ' ' << (a == c) << ' ' << points.Count() << " points\n";
}
```

The output:

```
2000 records of alpha.example, 2 strings in the pool
1 0 2 points
```

## See also

- [ConcurrentWeakDictionary](ConcurrentWeakDictionary.md), [ConcurrentWeakHashSet](ConcurrentWeakHashSet.md): the containers keyed by the identity of objects held weakly, whose sweep the pool shares
- [String](../Core/String.md): one word, compared by identity first, hashed once; [WeakPtr](../Core/WeakPtr.md): the entry
- [ConcurrentHashSet](ConcurrentHashSet.md): the table underneath; [RootPtr](../Core/RootPtr.md): how the default pool is held
- README: [Lock-free containers](../../concurrent/README.md#lock-free-containers)
- `tests/Sgcl/weak_and_intern.cpp`: the wrapper checked; `tests/concurrent/intern.cpp`: every behaviour above, with the threads.
