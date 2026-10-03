[sgcl](../../README.md) › [concurrent](../README.md) › [map](README.md)

# sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>::size

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
    concurrent::map<int, int> owners;
    vector<thread> threads;
    for (int t : range(4)) {
        threads.emplace_back([&owners, t] {
            for (int i : range(1000)) {
                owners.try_emplace(t * 1000 + i, t);
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    println("{}", owners.size());

    for (int i : range(500)) {
        owners.erase(i);
    }
    println("{}", owners.size());
}
```

Output:

```text
4000
3500
```

## See also

- [empty](empty.md): checks whether the map holds an element, without the count
- [bucket_count](bucket_count.md): the number of buckets, which follows the count
- [sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>](README.md)
