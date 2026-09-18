# Sgcl::ConcurrentSortedSet

```cpp
#include "sgcl/Sgcl/Concurrent/ConcurrentHashSet.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T, class Compare = std::less<T>>
    class ConcurrentSortedSet;
}
```

The same class in the `sgcl` interface: [concurrent_set](../../concurrent/concurrent_set.md).

`ConcurrentSortedSet<T, Compare>` is a lock-free ordered set shared by any number of threads: the skip list of [ConcurrentSortedDictionary](ConcurrentSortedDictionary.md), which has the account of the algorithm and the rules, with the value as the element (Java's `ConcurrentSkipListSet`). The elements are const, as in `std::set`; everything else is the dictionary's: `Contains`, `FindEntry`, `LowerBound` and `UpperBound` wait-free; `Add`, `Emplace` and `Remove` lock-free and linearizable; `Count`, `IsEmpty`, `Clear`; weakly consistent iteration in order.

## Members

```cpp
using ValueType = T;
using InnerType = sgcl::concurrent_set<T, Compare>;
using SizeType = size_t;
using Iterator = InnerType::iterator;               // forward, over const T, holds its node
using ConstIterator = InnerType::const_iterator;

ConcurrentSortedSet();
explicit ConcurrentSortedSet(const Compare& cmp);
template<std::input_iterator It> ConcurrentSortedSet(It first, It last);
ConcurrentSortedSet(std::initializer_list<T> il);
ConcurrentSortedSet(const ConcurrentSortedSet&) = delete;

Iterator begin(ConcurrentSortedSet&) noexcept;      // free functions, and the const forms: in order
Iterator end(ConcurrentSortedSet&) noexcept;
Iterator End() noexcept;
bool IsEmpty() const noexcept;
SizeType Count() const noexcept;
bool Contains(const T& value) const noexcept;
Iterator FindEntry(const T& value) noexcept;
Iterator LowerBound(const T& value) noexcept;       // the first element not less than value
Iterator UpperBound(const T& value) noexcept;       // the first greater
template<class K = T> bool Contains(const K& value) const noexcept;   // and FindEntry, LowerBound, UpperBound, Remove: a K other than T when Compare is transparent (a string_view for a String)
bool Add(const T& value);                           // looks the value up first, builds nothing when it is there: whether it was new
bool Add(T&& value);
template<class... A> bool Emplace(A&&... a);
bool Remove(const T& value);
Iterator RemoveAt(ConstIterator pos);
void Clear();
Compare KeyCompare() const;
InnerType& Inner() noexcept;
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// Timestamps from many threads, read back in order while they arrive
int main() {
    ConcurrentSortedSet<long> stamps;
    List<Thread> threads;
    for (int t : Range(4)) {
        threads.Emplace([&, t] {
            for (long i : Range(1000L)) {
                stamps.Add(i * 4 + t);
            }
        });
    }
    long walks = 0, disorder = 0;
    for (long last = -1; last < 4 * 999; ++walks) {   // walks while the writers are at it, until one sees the last stamp
        last = -1;
        for (long s : stamps) {          // sorted at every walk, whatever the writers are doing
            disorder += s <= last;
            last = s;
        }
    }
    for (auto& th : threads) {
        th.Join();
    }
    std::cout << stamps.Count() << " stamps, " << walks << " walks, " << disorder << " out of order; "
              << "from 1000: " << *stamps.LowerBound(1000) << "\n";
    return stamps.Count() == 4000 && disorder == 0 ? 0 : 1;
}
```

The output of one run (the walks depend on how the threads interleave):

```
4000 stamps, 343 walks, 0 out of order; from 1000: 1000
```

## See also

- [ConcurrentSortedDictionary](ConcurrentSortedDictionary.md), the algorithm and the rules
- [ConcurrentHashSet](ConcurrentHashSet.md), the hash set; [SortedSet](../Containers/SortedSet.md), the sequential set
- [README: Lock-free containers](../../concurrent/README.md#lock-free-containers)
