# Sgcl::ConcurrentHashSet

```cpp
#include "sgcl/Sgcl/Concurrent/ConcurrentHashSet.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T, class Hash = std::hash<T>, class Equal = std::equal_to<T>>
    class ConcurrentHashSet;
}
```

The same class in the `sgcl` interface: [concurrent_unordered_set](../../concurrent/concurrent_unordered_set.md).

`ConcurrentHashSet<T, Hash, Equal>` is a lock-free hash set shared by any number of threads: the split-ordered list of [ConcurrentDictionary](ConcurrentDictionary.md), which has the account of the algorithm and the rules, with the value as the element. The elements are const, as in `std::unordered_set`; everything else is the dictionary's: `Contains` and `FindEntry` wait-free; `Add`, `Emplace` and `Remove` lock-free and linearizable; `Count`, `IsEmpty`, `BucketCount`, `Reserve`, `Clear`; weakly consistent iteration in the order of the list.

## Members

```cpp
using ValueType = T;
using InnerType = sgcl::concurrent_unordered_set<T, Hash, Equal>;
using SizeType = size_t;
using Iterator = InnerType::iterator;               // forward, over const T, holds its node
using ConstIterator = InnerType::const_iterator;

ConcurrentHashSet();
explicit ConcurrentHashSet(SizeType buckets);
template<std::input_iterator It> ConcurrentHashSet(It first, It last);
ConcurrentHashSet(std::initializer_list<T> il);
ConcurrentHashSet(const ConcurrentHashSet&) = delete;

Iterator begin(ConcurrentHashSet&) noexcept;        // free functions, and the const forms
Iterator end(ConcurrentHashSet&) noexcept;
Iterator End() noexcept;                            // what FindEntry returns when absent
bool IsEmpty() const noexcept;
SizeType Count() const noexcept;
SizeType BucketCount() const noexcept;
void Reserve(SizeType n);
bool Contains(const T& value) const noexcept;
Iterator FindEntry(const T& value) noexcept;        // the element as an iterator that holds its node
template<class K = T> bool Contains(const K& value) const noexcept;   // and FindEntry, Remove: a K other than T when Hash and Equal are transparent (a string_view for a String)
bool Add(const T& value);                           // looks the value up first, builds nothing when it is there: whether it was new
bool Add(T&& value);
template<class... A> bool Emplace(A&&... a);
bool Remove(const T& value);
Iterator RemoveAt(ConstIterator pos);
void Clear();
Hash HashFunction() const;
Equal KeyEqual() const;
InnerType& Inner() noexcept;
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// Threads claim ids: Add succeeds for exactly one of them per id
int main() {
    ConcurrentHashSet<int> claimed;
    Atomic wins = 0;
    List<Thread> threads;
    for (int t : Range(8)) {
        threads.Emplace([&] {
            for (int id : Range(10000)) {
                wins += claimed.Add(id);
            }
        });
    }
    for (auto& th : threads) {
        th.Join();
    }
    std::cout << wins << " claims, " << claimed.Count() << " ids\n";
    return wins == 10000 && claimed.Count() == 10000 ? 0 : 1;
}
```

The output:

```
10000 claims, 10000 ids
```

## See also

- [ConcurrentDictionary](ConcurrentDictionary.md), the algorithm and the rules
- [ConcurrentSortedSet](ConcurrentSortedSet.md), the ordered set (a skip list); [HashSet](../Containers/HashSet.md), the sequential set
- [README: Lock-free containers](../../concurrent/README.md#lock-free-containers)
