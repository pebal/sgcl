[sgcl](../../README.md) › [concurrent](../README.md)

# sgcl::concurrent::bloom_filter

```cpp
#include "sgcl/concurrent/bloom_filter.h"   // or "sgcl/concurrent.h"

namespace sgcl::concurrent {
    class bloom_filter {
    public:
        friend bool operator==(const bloom_filter& a, const bloom_filter& b) noexcept;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::concurrent::bloom_filter` is a Bloom filter: a set that answers whether a key was added with no false
negatives and a chosen rate of false positives, in a few bits a key whatever the keys are. A key sets *k* bits of *m*;
a key whose *k* bits are all set may have been added, one with a bit unset certainly was not. Made for *n* keys and a
rate *p*, it takes the textbook optimum, *m* = −*n* ln *p* / ln²2 bits and *k* = *m*/*n* ln 2 hashes: 9.6 bits and 7
hashes a key at 1%, 14.4 bits and 10 at 0.1%. The *k* positions come from one XXH3-128 of the key by double hashing
(Kirsch and Mitzenmacher), each reduced over the whole *m* by a multiplication, no division.

Many threads add and ask at once: a bit is set by an atomic OR of its word, after a look that finds it unset, so a
filter that is mostly set is mostly read; there is no lock and nothing to wait for. Go's standard library has no
Bloom filter.

## Rules

- A bloom_filter is a handle: one word, a tracked word to the bits, which copies share;
  [operator==](operator_cmp.md) says whether two are the same filter, and [clone](clone.md) copies the bits.
- A key is text (a literal, a `std::string`, a [string](../../core/string/README.md)), bytes (a slice of bytes, a
  `vector<byte>`), or a number, hashed by its eight bytes, little-endian: the text `"ann"` and the bytes `a n n` are
  one key.
- The hash is XXH3 with the seed 0 in every process, so the bytes of [to_bytes](to_bytes.md) are read by another
  program alike.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](bloom_filter.md) | constructs a filter of the optimal shape for a count and a rate, or a handle of the same filter |
| [with_size](with_size.md) | constructs a filter of a number of bits and hashes |
| `(destructor)` | lets go of the handle; the bits are the collector's once no handle holds them |

#### Keys

| Function | Description |
|---|---|
| [add](add.md) | sets the bits of a key, and says whether it is certainly new |
| [contains](contains.md) | checks whether a key may have been added |

#### Observers

| Function | Description |
|---|---|
| [bit_count](bit_count.md) | the bits, *m* |
| [hash_count](hash_count.md) | the hashes a key, *k* |
| [approximate_count](approximate_count.md) | the keys added, estimated from the bits set |
| [false_positive_rate](false_positive_rate.md) | the rate of false positives now |

#### Whole filters

| Function | Description |
|---|---|
| [merge](merge.md) | adds another filter's keys: the union |
| [clear](clear.md) | unsets every bit |
| [clone](clone.md) | a filter of its own with the same bits |
| [to_bytes](to_bytes.md) | the filter as bytes |
| [from_bytes](from_bytes.md) | a filter from its bytes |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | checks whether two handles are the same filter |

## Complexity

An add and a question are one hash and *k* words read, constant; an add writes only the words whose bit it sets. The
estimates read every word once.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::bloom_filter seen(1000, 0.01);
    for (const char* url : {"/a", "/b", "/a"}) {
        println("{} {}", url, seen.add(url) ? "new" : "seen before");
    }
    println("{} {}", seen.contains("/b"), seen.contains("/c"));
}
```

Output:

```text
/a new
/b new
/a seen before
true false
```

## See also

- [hyperloglog](../hyperloglog/README.md): how many distinct keys
- [count_min_sketch](../count_min_sketch/README.md): how often a key
- [set](../set/README.md): the exact set, a node a key
