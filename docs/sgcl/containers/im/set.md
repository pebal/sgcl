# sgcl::im::set

```cpp
#include "sgcl/containers/im/set.h"   // or "sgcl/containers/im/im.h", "sgcl/sgcl.h"

namespace sgcl::im {
    template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class set;
}
```

`sgcl::im::set<Key, Hash, KeyEqual>` is the immutable hash set: the hash array mapped trie of [im::map](map.md), which has the account of the structure and the rules, with the key as the element. Every `insert` and `erase` returns a new set that shares all but the path it changed with the old one, which stays exactly as it was; a lookup walks log32(*n*) nodes; a set held by any number of threads is read by all of them without a lock, and a version is published, and replaced by the next, through a [copy_on_write](../../concurrent/copy_on_write.md) or an [atomic](../../concurrent/atomic.md) ([README: The structures](README.md#the-structures)). Two words, the size and the root, living where a `tracked_ptr` may. The elements are `const`; with a transparent hash and equality the lookups take a key of another type, a `string_view` for a `string`.

## Members

```cpp
using key_type = Key;
using value_type = Key;
using hasher = Hash;
using key_equal = KeyEqual;
using size_type = size_t;
using difference_type = ptrdiff_t;
using const_iterator = /* forward iterator over const Key */;
using iterator = const_iterator;

set();
explicit set(const Hash& hash, const KeyEqual& equal = KeyEqual());
template<std::input_iterator InputIt> set(InputIt first, InputIt last, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual());
set(std::initializer_list<Key> ilist, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual());
set(const set&) noexcept;
set& operator=(const set&) noexcept;

const_iterator begin() const noexcept;
const_iterator end() const noexcept;
size_type size() const noexcept;
bool empty() const noexcept;
hasher hash_function() const;
key_equal key_eq() const;
const Key* find(const Key& key) const;                    // the element, null when absent
bool contains(const Key& key) const;
size_type count(const Key& key) const;
template<class K> const Key* find(const K& key) const;    // when Hash and KeyEqual are transparent: and contains, count, erase
set insert(const Key& key) const;              // the set with the key: the path copied, the rest shared
set insert(Key&& key) const;
set erase(const Key& key) const;               // the set without the key; the same set when absent
template<class K> set erase(const K& key) const;
friend bool operator==(const set& a, const set& b);   // the same elements
friend bool operator!=(const set& a, const set& b);
```

```cpp
im::set<string> seen = {"alice", "bob"};
auto with_carol = seen.insert("carol");          // seen has two elements, with_carol three
bool was_there = seen.contains("carol");         // false: a literal, transparent
auto without_bob = with_carol.erase("bob");      // a literal again
```

### The mixins

`im::set` carries [mixin::immutable](../../core/mixin/immutable.md) and [mixin::enumerable](../../core/mixin/enumerable.md) (`contains` and `find` its own) ([the mixins](../../core/mixin/README.md)).

```cpp
im::set<int> s = im::set<int>().insert(2);
assert(s.contains(2) && s.count_of([](int x) { return x > 1; }) == 1);
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

// The ids seen so far, one version per step: what was seen at step k is
// still known at the end, and the versions share all but a path each
int main() {
    vector<im::set<int>> at_step;
    im::set<int> seen;
    for (int id : {7, 3, 7, 9, 3, 1}) {
        seen = seen.insert(id);
        at_step.push_back(seen);                      // two words: the version as it is now
    }
    for (int k : range(at_step.size())) {
        std::cout << "step " << k << ": " << at_step[k].size() << " ids, 9 " << (at_step[k].contains(9) ? "seen" : "not yet") << '\n';
    }
    return seen.size() == 4 ? 0 : 1;
}
```

The output:

```
step 0: 1 ids, 9 not yet
step 1: 2 ids, 9 not yet
step 2: 2 ids, 9 not yet
step 3: 3 ids, 9 seen
step 4: 3 ids, 9 seen
step 5: 4 ids, 9 seen
```

## See also

- [im::map](map.md), the structure, the rules and the numbers; [im::vector](vector.md)
- [copy_on_write](../../concurrent/copy_on_write.md), how a version is published to other threads
- [set](../set.md), the mutable one; [concurrent_set](../../concurrent/concurrent_set.md), the one many threads change in place
- [README: The structures](README.md#the-structures)
