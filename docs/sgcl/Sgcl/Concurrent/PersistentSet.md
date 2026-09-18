# Sgcl::PersistentSet

```cpp
#include "sgcl/Sgcl/Concurrent/PersistentDictionary.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T, class Hash = std::hash<T>, class Equal = std::equal_to<T>>
    class PersistentSet;
}
```

The same class in the `sgcl` interface: [persistent_set](../../concurrent/persistent_set.md).

`PersistentSet<T, Hash, Equal>` is the persistent hash set: the hash array mapped trie of [PersistentDictionary](PersistentDictionary.md), which has the account of the structure and the rules, with the value as the element. Every `Add` and `Remove` returns a new set that shares all but the path it changed with the old one, which stays exactly as it was; a lookup walks log32(*n*) nodes; a set held by any number of threads is read by all of them without a lock, and a version is published, and replaced by the next, through a [CopyOnWrite](CopyOnWrite.md) or an [Atomic](Atomic.md) ([README: Persistent structures](../../concurrent/README.md#persistent-structures)). Two words, the count and the root, living where a `Ptr` may. The elements are `const`; with a transparent hash and equality the lookups take a value of another type, a `string_view` for a `String`.

## Members

```cpp
using ValueType = T;
using InnerType = sgcl::persistent_set<T, Hash, Equal>;
using SizeType = size_t;
using Iterator = /* forward iterator over const T */;
using ConstIterator = Iterator;

PersistentSet();
template<std::input_iterator It> PersistentSet(It first, It last);
PersistentSet(std::initializer_list<T> il);
explicit PersistentSet(InnerType s) noexcept;
PersistentSet(const PersistentSet&) noexcept;
PersistentSet& operator=(const PersistentSet&) noexcept;

SizeType Count() const noexcept;
bool IsEmpty() const noexcept;
Hash HashFunction() const;
Equal KeyEqual() const;
template<class K = ValueType> bool Contains(const K& value) const;
template<class K = ValueType> const ValueType* Find(const K& value) const;   // the element, null when absent
PersistentSet Add(const ValueType& value) const;         // the set with the value: the path copied, the rest shared
PersistentSet Add(ValueType&& value) const;
template<class K = ValueType> PersistentSet Remove(const K& value) const;   // the set without the value; the same set when absent
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
friend bool operator==(const PersistentSet& a, const PersistentSet& b);   // the same elements
friend bool operator!=(const PersistentSet& a, const PersistentSet& b);
template<class T, class H, class E> auto begin(const PersistentSet<T, H, E>& s) noexcept;   // for a range-for
template<class T, class H, class E> auto end(const PersistentSet<T, H, E>& s) noexcept;
```

```cpp
PersistentSet<String> seen = {"alice", "bob"};
auto withCarol = seen.Add("carol");              // seen has two elements, withCarol three
bool wasThere = seen.Contains("carol");          // false: a literal, transparent
auto withoutBob = withCarol.Remove("bob");       // a literal again
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// The ids seen so far, one version per step: what was seen at step k is
// still known at the end, and the versions share all but a path each
int main() {
    List<PersistentSet<int>> atStep;
    PersistentSet<int> seen;
    for (int id : {7, 3, 7, 9, 3, 1}) {
        seen = seen.Add(id);
        atStep.Add(seen);                             // two words: the version as it is now
    }
    for (int k : Range(atStep.Count())) {
        std::cout << "step " << k << ": " << atStep[k].Count() << " ids, 9 " << (atStep[k].Contains(9) ? "seen" : "not yet") << '\n';
    }
    return seen.Count() == 4 ? 0 : 1;
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

- [PersistentDictionary](PersistentDictionary.md), the structure and the rules; [PersistentList](PersistentList.md)
- [CopyOnWrite](CopyOnWrite.md), how a version is published to other threads
- [HashSet](../Containers/HashSet.md), the mutable one; [ConcurrentHashSet](ConcurrentHashSet.md), the one many threads change in place
- [README: Persistent structures](../../concurrent/README.md#persistent-structures)
