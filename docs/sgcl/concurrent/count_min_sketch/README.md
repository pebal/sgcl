[sgcl](../../README.md) › [concurrent](../README.md)

# sgcl::concurrent::count_min_sketch

```cpp
#include "sgcl/concurrent/count_min_sketch.h"   // or "sgcl/concurrent.h"

namespace sgcl::concurrent {
    class count_min_sketch {
    public:
        friend bool operator==(const count_min_sketch& a, const count_min_sketch& b) noexcept;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::concurrent::count_min_sketch` counts how often each key was added, approximately, in a fixed table whatever the
keys: the count-min sketch of Cormode and Muthukrishnan (2005). A key adds to one counter in each of *d* rows of *w*;
its count is the least of its counters, never below the truth, and above it by more than ε·*N* (*N* every count
added) with a probability under δ, for *w* = ⌈e/ε⌉ and *d* = ⌈ln(1/δ)⌉: the heavy hitters of a stream, the hot keys
of a cache, the requests of each client, without a map of every key. The positions come from one XXH3-128 of the key
by double hashing, one a row, each reduced by a multiplication.

Many threads add and ask at once: a counter is added to atomically, 64 bits wide so that it never wraps. There is no
conservative update (only the counters below the new least raised, Estan and Varghese): two updates of one key at
once that read the same least would raise to the same value and lose a count, an estimate under the truth, and a lock
across the rows would cost every add its freedom from locks.

## Rules

- A count_min_sketch is a handle: one word, a tracked word to the counters, which copies share;
  [operator==](operator_cmp.md) says whether two are the same sketch, and [clone](clone.md) copies the counters.
- A key is text, bytes or a number, as a [bloom_filter](../bloom_filter/README.md)'s.
- The hash is XXH3 with the seed 0 in every process: the bytes of [to_bytes](to_bytes.md) merge with a sketch of
  another program.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](count_min_sketch.md) | constructs a sketch for an error and a probability, or a handle of the same sketch |
| [with_size](with_size.md) | constructs a sketch of a width and a depth |
| `(destructor)` | lets go of the handle; the counters are the collector's once no handle holds them |

#### Keys

| Function | Description |
|---|---|
| [add](add.md) | adds a count of a key |
| [estimate](estimate.md) | how often a key was added, never less than the truth |
| [total](total.md) | every count added |

#### Observers

| Function | Description |
|---|---|
| [width](width.md) | the counters a row |
| [depth](depth.md) | the rows |

#### Whole sketches

| Function | Description |
|---|---|
| [merge](merge.md) | adds another sketch's counts |
| [clear](clear.md) | every counter zero |
| [clone](clone.md) | a sketch of its own with the same counters |
| [to_bytes](to_bytes.md) | the sketch as bytes |
| [from_bytes](from_bytes.md) | a sketch from its bytes |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | checks whether two handles are the same sketch |

## Complexity

An add and an estimate are one hash and *d* counters, constant. A merge, a clear and a clone touch every counter.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::count_min_sketch hits;
    for (const char* page : {"/", "/about", "/", "/", "/news"}) {
        hits.add(page);
    }
    println("{} {} {}", hits.estimate("/"), hits.estimate("/news"), hits.total());
}
```

Output:

```text
3 1 5
```

## See also

- [bloom_filter](../bloom_filter/README.md): whether a key was added
- [hyperloglog](../hyperloglog/README.md): how many distinct keys
- [cache](../cache/README.md): a bounded map
