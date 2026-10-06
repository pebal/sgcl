[sgcl](../../README.md) › [concurrent](../README.md)

# sgcl::concurrent::hyperloglog

```cpp
#include "sgcl/concurrent/hyperloglog.h"   // or "sgcl/concurrent.h"

namespace sgcl::concurrent {
    class hyperloglog {
    public:
        friend bool operator==(const hyperloglog& a, const hyperloglog& b) noexcept;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::concurrent::hyperloglog` counts the distinct keys added, approximately, in a fixed few kilobytes whatever their
number: HyperLogLog (Flajolet, Fusy, Gandouet and Meunier, 2007). Each key's 64-bit XXH3 picks one of 2^*p* registers
by its first *p* bits and raises it to the leading zeros of the rest, plus one; the registers together estimate the
count with a relative standard error of 1.04/√2^*p*: 0.81% at the default precision of 14, in 16 KB. The estimate is
Ertl's improved estimator ("New cardinality estimation algorithms for HyperLogLog sketches", 2017), accurate from zero
to the far end of 64 bits by one formula, where HLL++ corrects the raw estimate with tables measured for each
precision; Redis estimates the same way.

Many threads add at once: a register is raised by an atomic maximum, a load and a compare-exchange only when the new
rank is higher, so a sketch past its first keys is mostly read. Sketches merge by the maximum of each register, so a
count is split over threads, machines or days and summed exactly as one sketch of all the keys would count them. The
registers are a byte each and dense from the start: there is no sparse form, whose conversion to the dense one under
concurrent adds would need a lock.

## Rules

- A hyperloglog is a handle: one word, a tracked word to the registers, which copies share;
  [operator==](operator_cmp.md) says whether two are the same sketch, and [clone](clone.md) copies the registers.
- A key is text, bytes or a number, as a [bloom_filter](../bloom_filter/README.md)'s; a key added again changes
  nothing.
- The hash is XXH3 with the seed 0 in every process: the bytes of [to_bytes](to_bytes.md) merge with a sketch of
  another program.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](hyperloglog.md) | constructs a sketch of a precision, or a handle of the same sketch |
| `(destructor)` | lets go of the handle; the registers are the collector's once no handle holds them |

#### Keys

| Function | Description |
|---|---|
| [add](add.md) | counts a key |
| [estimate](estimate.md) | the distinct keys added, estimated |
| [precision](precision.md) | the precision, *p* |

#### Whole sketches

| Function | Description |
|---|---|
| [merge](merge.md) | adds another sketch's keys: the union |
| [clear](clear.md) | every register zero |
| [clone](clone.md) | a sketch of its own with the same registers |
| [to_bytes](to_bytes.md) | the sketch as bytes |
| [from_bytes](from_bytes.md) | a sketch from its bytes |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | checks whether two handles are the same sketch |

## Complexity

An add is one hash and one register, constant. An estimate and a merge read every register once.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::hyperloglog visitors;
    for (uint64_t i = 0; i < 1'000'000; ++i) {
        visitors.add(i % 250'000);  // each visitor four times
    }
    double e = visitors.estimate();
    println("within 3%: {}", e > 250'000 * 0.97 && e < 250'000 * 1.03);
}
```

Output:

```text
within 3%: true
```

## See also

- [bloom_filter](../bloom_filter/README.md): whether a key was added
- [count_min_sketch](../count_min_sketch/README.md): how often a key
