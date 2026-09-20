# sgcl::concurrent_set

```cpp
#include "sgcl/concurrent/concurrent_sorted_set.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class concurrent_set;
}
```

`sgcl::concurrent_set<Key, Hash, KeyEqual>` is a lock-free hash set shared by any number of threads: the split-ordered list of [concurrent_sorted_map](concurrent_sorted_map.md), which has the account of the algorithm and the rules, with the key as the element. The elements are const, as in `std::unordered_set`; everything else is the map's: `find`, `contains` and `count` wait-free; `insert`, `emplace` and `erase` lock-free and linearizable; `size`, `empty`, `bucket_count`, `reserve`, `clear`; weakly consistent iteration in the order of the list.

## Members

```cpp
using key_type = Key;
using value_type = Key;
using hasher = Hash;
using key_equal = KeyEqual;
using size_type = size_t;
using iterator = /* forward iterator over const Key */;
using const_iterator = iterator;

concurrent_set();
explicit concurrent_set(size_type buckets, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual());
template<std::input_iterator InputIt> concurrent_set(InputIt first, InputIt last, size_type buckets = 16, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual());
concurrent_set(std::initializer_list<Key> ilist, size_type buckets = 16, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual());

iterator begin() noexcept;
iterator end() noexcept;
bool empty() const noexcept;
size_type size() const noexcept;
size_type bucket_count() const noexcept;
void reserve(size_type count);
iterator find(const Key& key) noexcept;
bool contains(const Key& key) const noexcept;
size_type count(const Key& key) const noexcept;
template<class K> iterator find(const K& key) noexcept;   // when Hash and KeyEqual are transparent: and contains, count, erase
pair<iterator, bool> insert(const Key& key);           // one search, the node built only when the key is absent
pair<iterator, bool> insert(Key&& key);
template<std::input_iterator InputIt> void insert(InputIt first, InputIt last);
void insert(std::initializer_list<Key> ilist);
template<class... A> pair<iterator, bool> emplace(A&&... a);
size_type erase(const Key& key);
iterator erase(const_iterator pos);
void clear();
hasher hash_function() const;
key_equal key_eq() const;
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

// Threads claim ids: insert succeeds for exactly one of them per id
int main() {
    sgcl::concurrent_set<int> claimed;
    sgcl::atomic wins = 0;
    sgcl::vector<sgcl::thread> threads;
    for (int t : sgcl::range(8)) {
        threads.emplace_back([&] {
            for (int id : sgcl::range(10000)) {
                wins += claimed.insert(id).second;
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    std::cout << wins << " claims, " << claimed.size() << " ids\n";
    return wins == 10000 && claimed.size() == 10000 ? 0 : 1;
}
```

The output:

```
10000 claims, 10000 ids
```

## See also

- [concurrent_sorted_map](concurrent_sorted_map.md), the algorithm and the rules
- [concurrent_sorted_set](concurrent_sorted_set.md), the sorted set (a skip list); [set](../containers/set.md), the sequential set
- [README: Lock-free containers](README.md#lock-free-containers)
