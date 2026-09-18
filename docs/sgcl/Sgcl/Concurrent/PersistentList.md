# Sgcl::PersistentList

```cpp
#include "sgcl/Sgcl/Concurrent/PersistentList.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T>
    class PersistentList;
}
```

The same class in the `sgcl` interface: [persistent_vector](../../concurrent/persistent_vector.md).

`PersistentList<T>` is the persistent vector of Clojure and Scala: a list every change of which returns a new list and leaves the old one exactly as it was, the two sharing everything but the path that changed. The elements live in a trie of 32-way branches indexed by five bits of the position per level, the last up-to-32 elements in a tail held apart from it: reading an element walks log32(*n*) branches (three for a million elements, none for the last 32), `Set` copies the branches on the path to the element and the leaf, five objects at most, and shares the rest, `Add` copies the tail (32 elements at most) and, once in 32 adds, hangs the full tail on the trie along a copied path, `RemoveLast` does the reverse. Two versions of a list of a hundred thousand elements that differ in one element cost the one list plus a path: 439 KB plus 0.9 KB ([persistent_vector: Measured](../../concurrent/persistent_vector.md#measured)). Nothing is ever modified: a list held by any number of threads is read by all of them without a lock, and a version is published, and replaced by the next, through a [CopyOnWrite](CopyOnWrite.md) or an [Atomic](Atomic.md) ([README: Persistent structures](../../concurrent/README.md#persistent-structures)).

The list is four words: the count, the height of the trie and a `Ptr` to the root and to the tail; a copy of it is a copy of those words. The branches and the leaves are managed objects that no version owns: a leaf reached by ten versions is one leaf, and the collector frees it once the last version that reaches it is dropped, the accounting a persistent structure without a collector does with a reference count per node.

## Rules

- `PersistentList` holds its root and its tail by `Ptr`, so it lives where one may: on a thread's stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1). Its iterators hold nothing alive: valid while the list object they were taken from exists.
- Every member is `const`. `Add`, `Set` and `RemoveLast` return the new list; the one they were called on is unchanged, and stays so for as long as it is held. The elements are reached as `const`.
- A change copies `T`s: `Add` and `RemoveLast` the elements of the tail (up to 32), `Set` the 32 elements of the leaf holding the position. `T`'s copy constructor is what a change costs, plus the branches on the path.
- A `T` holding a `Ptr` is traced where it lives, in a leaf; a `T` with a destructor is destroyed when the collector frees its leaf, once no version reaches it.
- Sharing between threads: any number of threads read any version. A `PersistentList` variable that one thread replaces while others read it needs synchronization, and `CopyOnWrite<PersistentList<T>>` is the shape for it: `Load()` is one atomic load for a snapshot, `Update(f)` copies four words, applies `f` (an `Add`, a `Set`) and swings the pointer, so an update costs O(log *n*) where a `CopyOnWrite<List<T>>` costs a copy of everything.
- `operator[]`, `First` and `Last` are unchecked, `At` and `Set` throw `std::out_of_range`; `RemoveLast` on an empty list is undefined.

## Members

### Types

```cpp
using ValueType = T;
using InnerType = sgcl::persistent_vector<T>;
using SizeType = size_t;
using Iterator = /* random-access iterator over const T */;
using ConstIterator = Iterator;
```

### Constructors

```cpp
PersistentList() noexcept;
template<std::input_iterator It> PersistentList(It first, It last);
PersistentList(std::initializer_list<T> il);
explicit PersistentList(InnerType v) noexcept;
PersistentList(const PersistentList&) noexcept;
PersistentList& operator=(const PersistentList&) noexcept;
```

```cpp
PersistentList<int> empty;
PersistentList<int> primes = {2, 3, 5, 7};
List<int> v(1000, 1);
PersistentList fromRange(begin(v), end(v));   // deduced: PersistentList<int>
```

### Count, IsEmpty

```cpp
SizeType Count() const noexcept;
bool IsEmpty() const noexcept;
```

### operator[], At, First, Last

```cpp
const T& operator[](SizeType i) const noexcept;
const T& At(SizeType i) const;   // std::out_of_range when i >= Count()
const T& First() const noexcept;
const T& Last() const noexcept;
```

The element at a position: the tail directly for the last 32, log32(*n*) branches and a leaf for the rest; 1.6 ns for a random position of a hundred thousand.

### begin, end

```cpp
template<class T> auto begin(const PersistentList<T>& l) noexcept;
template<class T> auto end(const PersistentList<T>& l) noexcept;
```

Free functions, for a range-for and `std::ranges`: a random-access iterator that remembers the leaf it is in, 0.55 ns per element over a hundred thousand.

```cpp
PersistentList<int> l = {5, 3, 9, 1};
auto smallest = std::ranges::min_element(l);   // an iterator: *smallest is 1
for (int x : l) {
    std::cout << x << ' ';
}
```

### Add, Emplace

```cpp
PersistentList Add(const T& value) const;
PersistentList Add(T&& value) const;
template<class... A> PersistentList Emplace(A&&... a) const;
```

The list with one more element at the end: the tail copied with the element appended, and when it was full it goes into the trie as it is, along a copied path, with the element alone in a new tail; 21 ns per `Add` of an `int`, a hundred thousand times over.

```cpp
PersistentList<int> l = {1, 2};
auto m = l.Add(3);   // l is {1, 2}, m is {1, 2, 3}
```

### Set

```cpp
PersistentList Set(SizeType i, const T& value) const;
PersistentList Set(SizeType i, T&& value) const;
```

The list with the element at `i` replaced: the branches on the path to it and the leaf copied, everything else shared; 185 ns for a random position of a hundred thousand `int`s.

```cpp
PersistentList<int> l = {1, 2, 3};
auto m = l.Set(1, 20);   // l is {1, 2, 3}, m is {1, 20, 3}
```

### RemoveLast

```cpp
PersistentList RemoveLast() const;
```

The list without its last element: the tail copied one shorter, or, when it held one element, the trie's last leaf taken out along a copied path to serve as the tail. Not on an empty list.

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The `sgcl::persistent_vector` inside.

### Comparison

```cpp
friend bool operator==(const PersistentList& a, const PersistentList& b);
friend bool operator!=(const PersistentList& a, const PersistentList& b);
```

The same elements in the same order, whatever the two share.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// An undo history in one line per step: every version of the document
// is kept, and the versions share all but what each step changed
int main() {
    List<PersistentList<char>> history;
    PersistentList<char> text;
    String word = "persistent";
    for (char c : word) {
        text = text.Add(c);
        history.Add(text);                    // a version: four words, no copy of the text
    }
    text = text.Set(0, 'P');
    history.Add(text);
    for (auto& version : history) {
        std::cout << String(begin(version), end(version)) << '\n';
    }
    // the versions differing in one element share the rest: a hundred
    // thousand ints twice costs the one list plus a path
    PersistentList<int> big;
    for (int i : Range(100000)) {
        big = big.Add(i);
    }
    auto changed = big.Set(50000, -1);
    std::cout << big[50000] << ' ' << changed[50000] << '\n';
    return history.Count() == 11 && changed.Count() == big.Count() ? 0 : 1;
}
```

The output:

```
p
pe
per
pers
persi
persis
persist
persiste
persisten
persistent
Persistent
50000 -1
```

## See also

- [PersistentDictionary](PersistentDictionary.md), [PersistentSet](PersistentSet.md), the persistent hash dictionary and set
- [CopyOnWrite](CopyOnWrite.md), how a version is published to other threads; [Atomic](Atomic.md)
- [List](../Containers/List.md), the mutable one
- [README: Persistent structures](../../concurrent/README.md#persistent-structures), [README: The rules](../../core/README.md#the-rules)
