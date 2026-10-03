[sgcl](../../README.md) › [concurrent](../README.md) › [set](../set.md)

# sgcl::concurrent::set\<Key, Hash, KeyEqual\>::size

```cpp
size_type size() const noexcept;
```

Returns the number of elements: the sum of the counts of the stripes. An insertion adds one, and an erasure
subtracts one, on the stripe of the thread that makes it, a cache line of its own (Java's `LongAdder`), so that the
threads do not fight over one word.

## Parameters

None.

## Return value

The sum of the stripes, 0 when it is negative.

## Complexity

Constant: the loads of the sixteen stripes.

## Exceptions

None.

## Notes

Wait-free. The stripes are read one after another while other threads change them, so under concurrent insertions
and erasures the sum is a snapshot of no particular moment, and a stripe may hold a negative count (a thread that
erased what another inserted); the sum is exact once the other threads are quiet. The same sum decides the growth
of the bucket array ([bucket_count](bucket_count.md)).

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::set<int> seen;
    vector<thread> threads;
    for (int t : range(4)) {
        threads.emplace_back([&seen] {
            for (int i : range(1000)) {
                seen.insert(i);  // the same thousand keys from every thread
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    println("{}", seen.size());

    for (int i : range(500)) {
        seen.erase(i);
    }
    println("{}", seen.size());
}
```

Output:

```text
1000
500
```

## See also

- [empty](empty.md): checks whether the set holds an element, without the count
- [bucket_count](bucket_count.md): the number of buckets, which follows the count
- [sgcl::concurrent::set\<Key, Hash, KeyEqual\>](../set.md)
