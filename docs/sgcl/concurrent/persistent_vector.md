# sgcl::persistent_vector

```cpp
#include "sgcl/concurrent/persistent_vector.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T>
    class persistent_vector;
}
```

The same class in the `Sgcl` interface: [PersistentList](../Sgcl/Concurrent/PersistentList.md).

`sgcl::persistent_vector<T>` is the persistent vector of Clojure and Scala: a sequence every operation of which returns a new vector and leaves the old one exactly as it was, the two sharing everything but the path that changed. The elements live in a trie of 32-way branches indexed by five bits of the position per level, the last up-to-32 elements in a tail held apart from it: reading an element walks log32(*n*) branches (three for a million elements, none for the last 32), `set` copies the branches on the path to the element and the leaf, five objects at most, and shares the rest, `push_back` copies the tail (32 elements at most) and, once in 32 pushes, hangs the full tail on the trie along a copied path, `pop_back` does the reverse. Two versions of a vector of a hundred thousand elements that differ in one element cost the one vector plus a path: measured below, 439 KB plus 0.9 KB. Nothing is ever modified: a vector held by any number of threads is read by all of them without a lock, and a version is published, and replaced by the next, through a [copy_on_write](copy_on_write.md) or an [atomic](atomic.md) ([README: Persistent structures](README.md#persistent-structures)).

The vector is four words: the size, the height of the trie and a `tracked_ptr` to the root and to the tail. A copy of it is a copy of those words. The branches and the leaves are managed objects that no version owns: a leaf reached by ten versions is one leaf, and the collector frees it once the last version that reaches it is dropped. That is the question a persistent structure in a language without a collector answers with a reference count per node, and the reason these structures belong in a library that has one: the sharing costs nothing to account for.

## Rules

- `sgcl::persistent_vector` holds its root and its tail by `tracked_ptr`, so it lives where one may: on a thread's stack or inside a managed object ([The rules](../core/README.md#the-rules), 1). Its iterators hold nothing alive: an iterator is valid while the vector object it was taken from exists, as one of `std` is.
- Every member is `const`. `push_back`, `pop_back` and `set` return the new vector; the one they were called on is unchanged, and stays so for as long as it is held. The elements are reached as `const`.
- A change copies `T`s: `push_back` and `pop_back` copy the elements of the tail (up to 32), `set` the 32 elements of the leaf holding the position. `T`'s copy constructor is what a change costs, plus the branches on the path, 256 bytes each.
- A `T` holding tracked pointers is traced where it lives, in a leaf; a `T` with a destructor is destroyed when the collector frees its leaf, once no version reaches it. Nothing is destroyed by a `pop_back`: the old version still holds the element.
- Sharing between threads: any number of threads read any version. A `persistent_vector` variable that one thread replaces while others read it is the one thing that needs synchronization, and `copy_on_write<persistent_vector<T>>` is the shape for it: `load()` is one atomic load for a snapshot, `update(f)` copies four words, applies `f` (a `push_back`, a `set`) and swings the pointer, so an update costs O(log *n*) where a `copy_on_write<vector<T>>` costs a copy of everything. An `atomic<tracked_ptr<persistent_vector<T>>>` does the same with the version in a managed object of its own.
- `operator[]`, `front` and `back` are unchecked, `at` and `set` throw `std::out_of_range`; `pop_back` on an empty vector is undefined, as `std::vector`'s is.

## Members

### Types

```cpp
using value_type = T;
using reference = const T&;
using const_reference = const T&;
using size_type = size_t;
using difference_type = ptrdiff_t;
using const_iterator = /* random-access iterator over const T */;
using iterator = const_iterator;
using const_reverse_iterator = std::reverse_iterator<const_iterator>;
using reverse_iterator = const_reverse_iterator;
```

### Constructors

```cpp
persistent_vector() noexcept;
template<std::input_iterator InputIt> persistent_vector(InputIt first, InputIt last);
persistent_vector(std::initializer_list<T> ilist);
persistent_vector(const persistent_vector&) noexcept;
persistent_vector& operator=(const persistent_vector&) noexcept;
```

An empty vector holds no node at all. The range and the list constructors fill the leaves in place, 32 elements at a time, so building from a range costs what `std::vector` costs plus the branches.

```cpp
sgcl::persistent_vector<int> empty;
sgcl::persistent_vector<int> primes = {2, 3, 5, 7};
std::vector<int> v(1000, 1);
sgcl::persistent_vector from_range(v.begin(), v.end());   // deduced: persistent_vector<int>
```

### size, empty, depth

```cpp
size_type size() const noexcept;
bool empty() const noexcept;
unsigned depth() const noexcept;   // the levels of branches a random access walks: 0 with everything in the tail
```

### operator[], at, front, back

```cpp
const_reference operator[](size_type i) const noexcept;
const_reference at(size_type i) const;   // std::out_of_range when i >= size()
const_reference front() const noexcept;
const_reference back() const noexcept;
```

The element at a position: the tail directly for the last 32, `depth()` branches and a leaf for the rest; 1.6 ns for a random position of a hundred thousand.

### begin, end, cbegin, cend, rbegin, rend, crbegin, crend

```cpp
const_iterator begin() const noexcept;
const_iterator end() const noexcept;
const_reverse_iterator rbegin() const noexcept;
const_reverse_iterator rend() const noexcept;
```

A random-access iterator, so the algorithms of `<algorithm>` and `std::ranges` apply. It remembers the leaf it is in and walks the trie once in 32 elements: 0.55 ns per element over a hundred thousand.

```cpp
sgcl::persistent_vector<int> v = {5, 3, 9, 1};
auto smallest = std::ranges::min_element(v);   // an iterator: *smallest is 1
auto sum = std::accumulate(v.begin(), v.end(), 0);
```

### push_back, emplace_back

```cpp
persistent_vector push_back(const T& value) const;
persistent_vector push_back(T&& value) const;
template<class... A> persistent_vector emplace_back(A&&... a) const;
```

The vector with one more element at the end. The tail is copied with the element appended, and when it was full it goes into the trie as it is, along a copied path, with the element alone in a new tail: 21 ns per push of an `int`, a hundred thousand times over.

```cpp
sgcl::persistent_vector<int> v = {1, 2};
auto w = v.push_back(3);   // v is {1, 2}, w is {1, 2, 3}
```

### pop_back

```cpp
persistent_vector pop_back() const;
```

The vector without its last element: the tail copied one shorter, or, when it held one element, the trie's last leaf taken out along a copied path to serve as the tail. Not on an empty vector.

### set

```cpp
persistent_vector set(size_type i, const T& value) const;
persistent_vector set(size_type i, T&& value) const;
```

The vector with the element at `i` replaced: the branches on the path to it and the leaf copied, everything else shared. 185 ns for a random position of a hundred thousand `int`s.

```cpp
sgcl::persistent_vector<int> v = {1, 2, 3};
auto w = v.set(1, 20);   // v is {1, 2, 3}, w is {1, 20, 3}
```

### Comparison

```cpp
friend bool operator==(const persistent_vector& a, const persistent_vector& b);
friend bool operator!=(const persistent_vector& a, const persistent_vector& b);
```

The same elements in the same order, whatever the two share.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

// An undo history in one line per step: every version of the document
// is kept, and the versions share all but what each step changed
int main() {
    sgcl::vector<sgcl::persistent_vector<char>> history;
    sgcl::persistent_vector<char> text;
    sgcl::string word = "persistent";
    for (char c : word) {
        text = text.push_back(c);
        history.push_back(text);              // a version: four words, no copy of the text
    }
    text = text.set(0, 'P');
    history.push_back(text);
    for (auto& version : history) {
        std::cout << sgcl::string(version.begin(), version.end()) << '\n';
    }
    // the versions differing in one element share the rest: a hundred
    // thousand ints twice costs the one vector plus a path
    sgcl::persistent_vector<int> big;
    for (int i : sgcl::range(100000)) {
        big = big.push_back(i);
    }
    auto changed = big.set(50000, -1);
    std::cout << big[50000] << ' ' << changed[50000] << ' ' << big.depth() << " levels\n";
    return history.size() == 11 && changed.size() == big.size() ? 0 : 1;
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
50000 -1 3 levels
```

## Measured

On an Apple M-series core, `-O2`, `persistent_vector<int>` of a hundred thousand elements: `push_back` 21 ns (`std::vector`: 0.4), `pop_back` 23 ns, `set` at a random position 185 ns, `operator[]` at a random position 1.6 ns (`std::vector`: 0.5), iteration 0.55 ns per element. One version of a hundred thousand `int`s is 439 KB, 4.4 bytes per element; a second version differing in one element adds 0.9 KB, a third with one more element 0.9 KB. A change costs the copy of a path, a read costs a few dependent loads: the price of every version staying what it was.

## See also

- [persistent_map](persistent_map.md), [persistent_set](persistent_set.md), the persistent hash map and set
- [copy_on_write](copy_on_write.md), how a version is published to other threads; [atomic](atomic.md)
- [vector](../containers/vector.md), the mutable one
- [README: Persistent structures](README.md#persistent-structures), [README: The rules](../core/README.md#the-rules)
