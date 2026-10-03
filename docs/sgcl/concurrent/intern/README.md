[sgcl](../../README.md) › [concurrent](../README.md)

# sgcl::concurrent::intern\<T, Hash, KeyEqual\>

```cpp
#include "sgcl/concurrent/intern.h"   // or "sgcl/concurrent.h"

namespace sgcl::concurrent {
    template<class T, class Hash = std::hash<T>, class KeyEqual = std::equal_to<T>>
    class intern;
}
```

`sgcl::concurrent::intern<T, Hash, KeyEqual>` is a pool where equal values share one managed object, shared by any
number of threads: Go's `unique` package, Java's `String.intern`. [of](of.md) is the canonical object of a
value, the one the pool holds when it is alive, or a new one made from the value and entered; so a program holds
one copy of each distinct value it interns and compares two by their identity, a pointer comparison, where a
comparison of the contents would cost a walk. The values of a field that repeats across many records (a host, a
tag, a symbol, a type name), the keys of a large map, the atoms of an interpreter.

The pool does not keep its objects alive. An entry is a [weak_ptr](../../core/weak_ptr/README.md) to the canonical object,
placed in the lock-free hash set of [concurrent::set](../set/README.md) by the hash of the object's contents and compared
through the live object: to find a value, the table hashes it, walks to the entries of that hash, locks each one's
weak pointer and compares the contents. An object nobody holds any more is collected, and its entry is dead from
then on: never found, since it equals nothing, and never in the way of the entry that replaces it, which the next
`of` of that value adds beside it. The dead entries are swept out by an inserting thread every so many insertions,
as the [weak containers](../README.md#weak-containers) do it, and on [sweep](sweep.md). An entry keeps the hash
it was placed with, because the object it would be computed from may be gone by the time the entry is erased, and
the hash has its top bit set, so that the word is never taken for a heap address by a conservative scan.

A pool of `sgcl::string` (of any [basic_string](../../core/string/README.md)) keeps the string itself. A string is one word
pointing at an object never modified, compared by identity first, so the pool of strings keeps its entries on the
strings' own objects rather than on objects holding strings: `of` hands back a `string`, not a pointer to one, the
string made from a view when its value is new; a `string` passed in enters the pool as it is when its value is new
(Java's `String.intern`: the string itself, not a copy); the empty string, which is null, is one value with no
object, never entered and never looked up. [intern_string](../intern_string.md) is `make` on the default pool of
strings under a name that reads.

What differs from Go's `unique.Make`: the handle is the object itself, a `tracked_ptr` or the string, not a
`Handle` to ask for the value, and a program may have pools of its own besides the default one of each type,
`pool()`, which can be swept and cleared. What differs from Java's `String.intern`: any type interns, not only
strings.

## Rules

- A pool holds tracked pointers (the table's array, head and counters), so it lives on a thread's stack or inside a
  managed object ([The rules](../../core/README.md#the-rules), 1). The default pool of a type, [pool](pool.md),
  is a managed object under a [root_ptr](../../core/root_ptr/README.md), made on first use: what a global may hold.
- Every member function may be called from any thread at any time. `find` is wait-free and writes nothing once the
  value's bucket has its dummy node (the first lookup in a bucket makes it, as in [set](../set/README.md)); `of` and `make`
  are lock-free: an `of` that finds no live object makes one and enters its entry with the table's
  compare-exchange, and, when another thread's entry for the same value got in first, hands back that thread's
  object and lets its own die. So two threads interning the same new value at once both get one object, and no lock
  is taken anywhere. Nothing waits: a sweep runs one at a time, and a thread that finds one under way goes on.
- The handle is a `tracked_ptr<const T>` (the string itself for a pool of strings): the object is immutable, shared
  by everyone who interned the value, and alive for as long as anyone holds it. Its identity is the value's: two
  handles compare equal exactly when the values were equal at the time both were made and neither object died
  between (a value made again after its object died is a new object).
- An entry is dead once a cycle has found its object unreachable; between the object becoming unreachable and that
  cycle, `of` still hands the object out, alive again: the lag of any garbage collector, and no harm.
- `T` is copied into its object by `of` (`T(value)` for a `value` of type `T`, or of any type `T` is constructible
  from); `Hash` and `KeyEqual` are default-constructed, and see the contents through the handle: an equality
  called with a `const T&` and a `const K&` for a lookup by a `K`.
- A string longer than the largest size class of strings (49,960 bytes with its header, under a page of 64 KB)
  lives in a buffer of bytes rather than a slot, and a pointer to it cannot be made from its address in debug
  builds (the assertion of `tracked_ptr` from a raw pointer, which `atomic<string>` meets too): such strings intern
  in release builds only.
- Non-copyable, non-movable: a shared structure has one place.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the values: an object type `make_tracked` makes, constructible from the values interned. A `basic_string` keeps the string itself (see above). |
| `Hash` | The hash of the values, `std::hash<T>` by default. With `is_transparent` in both `Hash` and `KeyEqual` (as `std::hash` and `std::equal_to` of a `string` have it), `of`, `find` and `make` take a value of another type and build no `T` for the search. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. |
| `KeyEqual` | The equality of the values, `std::equal_to<T>` by default. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. |

## Member types

| Type | Definition |
|---|---|
| `value_type` | `T` |
| `handle` | `tracked_ptr<const T>`; `T` itself when `T` is a `basic_string` (`string`, `u16string` and the others) |
| `hasher` | `Hash` |
| `key_equal` | `KeyEqual` |
| `size_type` | `size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](intern.md) | constructs an empty pool |
| `(destructor)` | leaves the table to the collector; the objects live on where they are held |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the pool holds an entry |
| [size](size.md) | the number of entries, the dead ones not yet swept included |
| [reserve](reserve.md) | grows the table for a number of entries up front |

#### Modifiers

| Function | Description |
|---|---|
| [sweep](sweep.md) | drops the entries whose objects are gone |
| [clear](clear.md) | forgets every object |

#### Lookup

| Function | Description |
|---|---|
| [of](of.md) | the canonical object of a value, made and entered when there is none |
| [find](find.md) | the canonical object of a value when one is alive, or null |

#### The default pool

| Function | Description |
|---|---|
| [pool](pool.md) | the default pool of the type, one for the program |
| [make](make.md) | `of` on the default pool: Go's `unique.Make` |

## Complexity

- `of`, `make`: constant on average, a search of the hash set; a new value adds the object made and the insertion,
  and one insertion in as many as the pool has entries (16 at least) runs a sweep, linear in the entries
  ([Benchmarks](../benchmarks.md#the-bounded-queue-the-priority-queue-intern-and-the-weak-map): 69 ns per string
  interned on one thread, 5.2 ns across sixteen; Go's `unique.Make` is two to three times faster, Java's
  `String.intern` level).
- `find`: constant on average.
- `size`: constant, the sum of the table's stripes. `empty`: a walk from the head of the table's list to its first
  entry.
- `sweep`, `clear`: linear in the number of entries.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Record {
    string host;  // one of a few values, repeated in every record
    int bytes;
};

// Log records parsed by several threads: every record holds the one interned string of its
// host, and the records of a host share the object
int main() {
    concurrent::queue<tracked_ptr<Record>> records;
    vector<thread> parsers;
    for (int t : range(4)) {
        parsers.emplace_back([&records, t] {
            for (int i : range(1000)) {
                string line = (i + t) % 2 ? "alpha.example 512" : "beta.example 1024";
                string_slice host = line.as_slice(0, line.find(' '));
                // no string made once the host is known
                records.push(make_tracked<Record>(concurrent::intern_string(host), 512));
            }
        });
    }
    for (auto& p : parsers) {
        p.join();
    }

    string alpha = concurrent::intern_string("alpha.example");  // the object the parsers got
    int of_alpha = 0;
    while (auto r = records.try_pop()) {
        if ((*r)->host.object() == alpha.object()) {  // compared by identity: one word
            ++of_alpha;
        }
    }
    println("{} records of alpha.example", of_alpha);
    println("{} strings in the pool", concurrent::intern<string>::pool().size());
}
```

Output:

```text
2000 records of alpha.example
2 strings in the pool
```

## See also

- [intern_string](../intern_string.md): the interned string of some characters
- [concurrent::set](../set/README.md): the table underneath
- [concurrent::weak_map](../weak_map/README.md), [concurrent::weak_set](../weak_set/README.md): the containers keyed by the identity
  of objects held weakly, whose sweep the pool shares
- [string](../../core/string/README.md): one word, compared by identity first, hashed once; [weak_ptr](../../core/weak_ptr/README.md):
  the entry; [root_ptr](../../core/root_ptr/README.md): how the default pool is held
- [README: Weak containers](../README.md#weak-containers)
