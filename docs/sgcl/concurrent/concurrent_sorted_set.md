# sgcl::concurrent_sorted_set

```cpp
#include "sgcl/concurrent/concurrent_sorted_set.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class Key, class Compare = std::less<Key>>
    class concurrent_sorted_set;
}
```

`sgcl::concurrent_sorted_set<Key, Compare>` is a lock-free sorted set shared by any number of threads: the skip list of [concurrent_sorted_map](concurrent_sorted_map.md), which has the account of the algorithm and the rules, with the key as the element (Java's `ConcurrentSkipListSet`). The elements are const, as in `std::set`; everything else is the map's: `find`, `contains`, `count`, `lower_bound` and `upper_bound` wait-free; `insert`, `emplace` and `erase` lock-free and linearizable; `size`, `empty`, `clear`; weakly consistent iteration in key order.

## Members

```cpp
using key_type = Key;
using value_type = Key;
using key_compare = Compare;
using size_type = size_t;
using iterator = /* forward iterator over const Key */;
using const_iterator = iterator;

concurrent_sorted_set();
explicit concurrent_sorted_set(const Compare& comp);
template<std::input_iterator InputIt> concurrent_sorted_set(InputIt first, InputIt last, const Compare& comp = Compare());
concurrent_sorted_set(std::initializer_list<Key> ilist, const Compare& comp = Compare());

iterator begin() noexcept;
iterator end() noexcept;
bool empty() const noexcept;
size_type size() const noexcept;
iterator find(const Key& key) noexcept;
bool contains(const Key& key) const noexcept;
size_type count(const Key& key) const noexcept;
iterator lower_bound(const Key& key) noexcept;
iterator upper_bound(const Key& key) noexcept;
template<class K> iterator find(const K& key) noexcept;   // when Compare::is_transparent: and contains, count, lower_bound, upper_bound, erase
pair<iterator, bool> insert(const Key& key);           // one search, the node built only when the key is absent
pair<iterator, bool> insert(Key&& key);
template<std::input_iterator InputIt> void insert(InputIt first, InputIt last);
void insert(std::initializer_list<Key> ilist);
template<class... A> pair<iterator, bool> emplace(A&&... a);
size_type erase(const Key& key);
iterator erase(const_iterator pos);
void clear();
key_compare key_comp() const;
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

// Timestamps from many threads, read back in order while they arrive
int main() {
    sgcl::concurrent_sorted_set<long> stamps;
    sgcl::vector<sgcl::thread> threads;
    for (int t : sgcl::range(4)) {
        threads.emplace_back([&, t] {
            for (long i : sgcl::range(1000L)) {
                stamps.insert(i * 4 + t);
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
        th.join();
    }
    std::cout << stamps.size() << " stamps, " << walks << " walks, " << disorder << " out of order; "
              << "from 1000: " << *stamps.lower_bound(1000) << "\n";
    return stamps.size() == 4000 && disorder == 0 ? 0 : 1;
}
```

The output of one run (the walks depend on how the threads interleave):

```
4000 stamps, 43 walks, 0 out of order; from 1000: 1000
```

## See also

- [concurrent_sorted_map](concurrent_sorted_map.md), the algorithm and the rules
- [concurrent_sorted_set](concurrent_sorted_set.md), the hash set; [sorted_set](../containers/sorted_set.md), the sequential set
- [README: Lock-free containers](README.md#lock-free-containers)
