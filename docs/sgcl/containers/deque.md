# sgcl::deque

```cpp
#include "sgcl/containers/deque.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T>
    class deque;
}
```

`sgcl::deque<T>` is `std::deque` over managed memory. The interface is the one of `std::deque` (constructors, `assign`, element access, random-access iterators, `shrink_to_fit`, modifiers at both ends and in the middle, three-way comparison, `std::erase`/`std::erase_if`), and so is the behaviour: elements are constructed at insertion and destroyed at removal, references stay valid across a push or pop at either end, iterators do not.

What differs is where the memory lives. The elements live in blocks on the managed heap (a block holds as many elements as fit in 4 KB, rounded down to a power of two, at least one), addressed through a managed array of block pointers, the map; the deque object is four words (the map, its size, the index of the first element, the count). A map that is outgrown is replaced by a fresh one, never shifted in place. A block emptied by pops stays in the map as the spare block of its end, so a window of elements travelling through the deque allocates no blocks; every block goes when the deque becomes empty. Nothing is ever freed by hand: the blocks and the maps the deque lets go of are reclaimed by the collector once nothing refers to them. A `sgcl::deque<tracked_ptr<T>>` is the managed form of a deque of pointers: its elements are traced, and it may hold cycles like any other managed object.

## Rules

- A deque holds a `tracked_ptr` (the map), so it lives where one may: on a stack or inside a managed object, never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame ([The rules](../core/README.md#the-rules), 1).
- The deque destroys its elements itself, exactly when `std::deque` does: on removal (`erase`, `pop_back`, `pop_front`, `clear`, `resize`, `assign`) and in the destructor. The one exception is a deque dying in a sweep, inside a managed object nobody refers to any more: its blocks are garbage of the same sweep, and each destroys the elements it still holds when the sweep reaches it ([Containers](README.md#containers)).
- A `tracked_ptr` may not address an element of a deque ([The rules](../core/README.md#the-rules), 4): elements are reached through the deque, its iterators, references and raw pointers, valid exactly as long as with `std::deque` (a reference survives an insertion or erasure at either end; any other insertion or erasure invalidates everything). An invalid iterator must not be used, as in `std`.
- Iterators are raw (a pointer to the map, an index, a pointer to the element), cheap to copy, and may live anywhere, in a `std::vector` too. An iterator dying in a frame nulls its words, so a temporary left behind does not root the blocks under the conservative stack scan.
- Thread safety is that of `std::deque`: concurrent readers, or one writer, with the program's own synchronization. The collector never waits for a mutator and never touches a block the deque still holds.
- The cost of a push at either end: a load of the map, a load of the block, the construction, two stores (the block's range and the count); the map at its end or a missing block goes the slow way ([Benchmarks: Containers](benchmarks.md#containers): 2.0 ns per push against 1.5 ns for `std::deque`).

## Members

### Types

```cpp
using value_type = T;
using size_type = size_t;
using difference_type = ptrdiff_t;
using reference = T&;
using const_reference = const T&;
using pointer = T*;
using const_pointer = const T*;
using iterator = /* random-access iterator over T */;
using const_iterator = /* random-access iterator over const T */;
using reverse_iterator = std::reverse_iterator<iterator>;
using const_reverse_iterator = std::reverse_iterator<const_iterator>;
```

`iterator` converts to `const_iterator`; both model `std::random_access_iterator`.

### Constructors

```cpp
deque();
explicit deque(size_type count) requires std::default_initializable<T>;
deque(size_type count, const T& value);
template<std::input_iterator InputIt> deque(InputIt first, InputIt last);
template<std::ranges::input_range R> explicit deque(R&& r);   // from a range of what T is made of: the pieces of a string, a view, another container (not a deque: that is the copy)
deque(std::initializer_list<T> ilist);
deque(const deque& other);
deque(deque&& other) noexcept;
```

The default constructor allocates nothing. `deque(count)` holds `count` value-initialized elements, `deque(count, value)` `count` copies. The range constructor appends element by element, single-pass ranges included. A copy has blocks of its own; a move takes the map over and leaves `other` empty. An element constructor that throws leaves the deque empty and the exception propagates.

```cpp
sgcl::deque<int> zeros(4);                          // 0 0 0 0
sgcl::deque<sgcl::string> words(2, "x");             // "x" "x"
sgcl::deque digits = {1, 2, 3};
sgcl::deque<int> copy(digits.begin(), digits.end());
sgcl::deque<int> taken = std::move(digits);         // digits is empty now
```

### Destructor

```cpp
~deque();
```

Destroys the elements, unless the deque dies in a sweep: then its blocks are garbage of the same sweep and destroy the elements they still hold themselves. The blocks and the map are left to the collector.

### operator=

```cpp
deque& operator=(const deque& other);
deque& operator=(deque&& other) noexcept;
deque& operator=(std::initializer_list<T> ilist);
```

Copy assignment is `assign(other.begin(), other.end())`. Move assignment clears this deque and takes the other map over, leaving `other` empty.

```cpp
sgcl::deque a = {1, 2, 3};
sgcl::deque<int> b;
b = a;              // a copy, in blocks of its own
b = {4, 5};         // two elements
b = std::move(a);   // a is empty
```

### assign

```cpp
void assign(size_type count, const T& value);
template<std::input_iterator InputIt> void assign(InputIt first, InputIt last);
void assign(std::initializer_list<T> ilist);
```

Replaces the contents: the existing elements are assigned over, the surplus popped from the back, the missing ones pushed to the back.

```cpp
sgcl::deque d = {1, 2, 3};
d.assign(2, 9);          // 9 9
d.assign({7, 8, 9, 10});
```

### at, operator[]

```cpp
reference at(size_type pos);
const_reference at(size_type pos) const;
reference operator[](size_type pos);
const_reference operator[](size_type pos) const;
```

`at` throws `std::out_of_range` for `pos >= size()`; `operator[]` does not check. Both are a division by the block size and two loads.

```cpp
sgcl::deque d = {10, 20, 30};
d[1] = 25;
try { d.at(3); } catch (const std::out_of_range&) { /* 3 >= size() */ }
```

### front, back

```cpp
reference front();
const_reference front() const;
reference back();
const_reference back() const;
```

The first and the last element; the deque must not be empty.

### Iterators

```cpp
iterator begin() noexcept;                      const_iterator begin() const noexcept;
const_iterator cbegin() const noexcept;
iterator end() noexcept;                        const_iterator end() const noexcept;
const_iterator cend() const noexcept;
reverse_iterator rbegin() noexcept;             const_reverse_iterator rbegin() const noexcept;
const_reverse_iterator crbegin() const noexcept;
reverse_iterator rend() noexcept;               const_reverse_iterator rend() const noexcept;
const_reverse_iterator crend() const noexcept;
```

Random-access iterators: a step within a block and the access are plain loads, a step across a block boundary one load of the map, so `std::ranges` algorithms and `std::sort` work on the deque. An iterator keeps nothing alive by itself and is invalidated exactly when a `std::deque` iterator is.

```cpp
sgcl::deque d = {3, 1, 2};
std::ranges::sort(d);                          // 1 2 3
for (auto it = d.rbegin(); it != d.rend(); ++it) {
    *it *= 10;                                 // 10 20 30
}
```

### empty, size, max_size

```cpp
bool empty() const noexcept;
size_type size() const noexcept;
size_type max_size() const noexcept;           // PTRDIFF_MAX
```

`size()` is a word of the deque object.

### shrink_to_fit

```cpp
void shrink_to_fit();
```

Replaces the map by one holding exactly the blocks in use: the spare blocks go. An empty deque drops everything, as `clear()` does.

### clear

```cpp
void clear() noexcept;
```

Destroys every element and drops the blocks and the map: unlike `sgcl::vector`, nothing is kept for the next push.

### insert, emplace

```cpp
iterator insert(const_iterator pos, const T& value);
iterator insert(const_iterator pos, T&& value);
iterator insert(const_iterator pos, size_type count, const T& value);
template<std::input_iterator InputIt> iterator insert(const_iterator pos, InputIt first, InputIt last);
iterator insert(const_iterator pos, std::initializer_list<T> ilist);
template<class... A> iterator emplace(const_iterator pos, A&&... a);
```

Inserts before `pos` and returns an iterator to the first inserted element (`pos` itself when nothing is inserted). An insertion at either end is a push there; in the middle the shorter side of the deque shifts by the count. The value is built before anything moves, so an argument that refers to an element of this deque stays valid. A single-pass range is collected first, so that a failure leaves the deque as it was; on an exception the deque is as it was.

```cpp
sgcl::deque d = {1, 4};
d.insert(d.begin() + 1, 2);               // 1 2 4
d.insert(d.begin() + 2, 2, 3);            // 1 2 3 3 4
d.emplace(d.end(), 5);                    // 1 2 3 3 4 5
d.insert(d.begin(), {-1, 0});             // -1 0 1 2 3 3 4 5
d.insert(d.end(), d[0]);                  // an element of d: fine, copied before anything moves
```

### erase

```cpp
iterator erase(const_iterator pos);
iterator erase(const_iterator first, const_iterator last);
```

Removes the elements, shifting the shorter side of the deque over them and popping at that end, so the elements are destroyed at the end nearer to the range, as `std::deque` may do; returns the iterator to the element after the erased range. `erase(end())` and an empty range are no-ops.

```cpp
sgcl::deque d = {1, 2, 3, 4, 5};
auto it = d.erase(d.begin());             // 2 3 4 5, it -> 2
d.erase(it + 1, d.end());                 // 2 3
```

### push_back, emplace_back, push_front, emplace_front

```cpp
void push_back(const T& value);
void push_back(T&& value);
template<class... A> reference emplace_back(A&&... a);
void push_front(const T& value);
void push_front(T&& value);
template<class... A> reference emplace_front(A&&... a);
```

Appends an element at the back or the front and (`emplace_*`) returns a reference to it. The common case, a block with room at that end (the last one in use, or the spare), is two loads, the construction and two stores; the map at its end or a missing block allocates a block, and a fresh map when the map is full. References to the other elements stay valid, iterators do not.

```cpp
sgcl::deque<sgcl::tracked_ptr<int>> ptrs;
for (int i : sgcl::range(1000)) {
    ptrs.push_back(sgcl::make_tracked<int>(i));
    ptrs.push_front(sgcl::make_tracked<int>(-i));     // the outgrown maps are collected
}
int& last = *ptrs.emplace_back(sgcl::make_tracked<int>(1000));
```

### pop_back, pop_front

```cpp
void pop_back();
void pop_front();
```

Destroys the last or the first element. A block left empty stays as the spare of its end (an older spare at the same end goes) while the deque holds elements, and every block goes when it holds none. The deque must not be empty.

### resize

```cpp
void resize(size_type count) requires std::default_initializable<T>;
void resize(size_type count, const value_type& value);
```

Pops from the back down to `count`, or pushes value-initialized elements (copies of `value`) at the back up to it.

```cpp
sgcl::deque d = {1, 2, 3};
d.resize(5);          // 1 2 3 0 0
d.resize(2);          // 1 2
d.resize(4, 7);       // 1 2 7 7
```

### The mixins

```cpp
// mixin::enumerable
template<class Pred> size_t find_index(Pred pred) const;   // npos when none
template<class Pred> T* find_if(Pred pred) noexcept;       // null when none; and const
template<class Pred> bool exists(Pred pred) const;  template<class Pred> bool all(Pred pred) const;
template<class Pred> size_t count_of(Pred pred) const;
template<class F> void for_each(F f);                      // and const
bool contains(const auto& value) const;                    // elements with ==: anything an element compares with
size_t index_of(const auto& value) const;  size_t last_index_of(const auto& value) const;   // npos when none
decltype(auto) min() const;  decltype(auto) max() const;   // elements with <; and with a comparator
// mixin::ordered
bool is_sorted() const;  bool binary_search(const auto& value) const;  size_t sorted_index_of(const auto& value) const;   // on a sorted deque; and with a comparator
auto lower_bound(const auto& value);  auto upper_bound(const auto& value);   // and const, and with a comparator
void sort();  template<class Compare> void sort(Compare cmp);  template<class Proj> void sort_by(Proj proj);  void stable_sort();
// mixin::sequence
void fill(const auto& value);  void reverse() noexcept;
// mixin::equatable, mixin::comparable: == and <=>, below
```

The members of the mixins every sequence of the library carries ([the mixins](../core/mixin/README.md)): the questions of [mixin::enumerable](../core/mixin/enumerable.md), the order of [mixin::ordered](../core/mixin/ordered.md), the writes of [mixin::sequence](../core/mixin/sequence.md), so that `x.sort()` reads as `x.push_back(x)` does. A question that compares elements exists only for elements that compare; `index_of`, `last_index_of` and `find_index` give the position, or `npos` when nothing matches, `find_if` the element the predicate accepts first, or null.

```cpp
sgcl::deque d = {5, 3, 9, 3};
assert(d.contains(9) && d.index_of(3) == 1 && d.last_index_of(3) == 3 && d.index_of(7) == sgcl::npos);
assert(d.find_index([](int x) { return x > 4; }) == 0 && d.exists([](int x) { return x == 9; }) && !d.all([](int x) { return x > 3; }));
if (int* big = d.find_if([](int x) { return x > 8; })) {
    *big = 8;
}
assert(d.count_of([](int x) { return x == 3; }) == 2 && d.min() == 3 && d.max() == 8);
d.sort();                                        // 3 3 5 8
assert(d.is_sorted());
d.sort([](int a, int b) { return a > b; });     // 8 5 3 3
d.reverse();                                     // 3 3 5 8
int sum = 0;
d.for_each([&](int x) { sum += x; });         // 19
d.fill(0);
```

### swap

```cpp
void swap(deque& other) noexcept;
template<class T> void swap(deque<T>& lhs, deque<T>& rhs) noexcept;
```

Exchanges the maps and the counts; no element is touched.

### Comparisons

```cpp
friend bool operator==(const deque& lhs, const deque& rhs);
friend auto operator<=>(const deque& lhs, const deque& rhs);
```

From [mixin::equatable](../core/mixin/equatable.md) and [mixin::comparable](../core/mixin/comparable.md), for elements that compare. Element-wise, as for `std::deque`: `==` compares sizes first, `<=>` is lexicographical with the synthesized three-way comparison (`<=>` of `T` when it has one, else a `std::weak_ordering` built from `<`), so `<`, `<=`, `>`, `>=` and `!=` follow.

```cpp
sgcl::deque<int> a = {1, 2}, b = {1, 3};
bool less = a < b;                  // true
bool same = a == b;                 // false
```

### std::erase, std::erase_if

```cpp
template<class T, class U> typename deque<T>::size_type erase(deque<T>& c, const U& value);
template<class T, class Pred> typename deque<T>::size_type erase_if(deque<T>& c, Pred pred);
```

Declared in `sgcl` and brought into `std`: remove every element equal to `value`, or satisfying `pred`, and return how many were removed.

```cpp
sgcl::deque d = {1, 2, 2, 3, 4};
size_t twos = std::erase(d, 2);                                  // 2; d is 1 3 4
size_t big = std::erase_if(d, [](int x) { return x > 2; });      // 2; d is 1
```

### deque<unique_ptr<T>>

```cpp
template<class T>
class deque<unique_ptr<T>> : public std::deque<unique_ptr<T>>;
```

A `unique_ptr` owns its object and needs no tracing, so a deque of them is a plain `std::deque` with the constructors of the base: it may live anywhere a `std::deque` may, and the objects die when their `unique_ptr` does.

```cpp
sgcl::deque<sgcl::unique_ptr<int>> owned;
owned.push_back(sgcl::make_tracked<int>(1));
owned.pop_front();                                 // the int is destroyed here, deterministically
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

struct Job {
    int id;
    sgcl::vector<sgcl::tracked_ptr<Job>> depends_on;       // a job may wait for others
};

struct Scheduler {
    sgcl::deque<sgcl::tracked_ptr<Job>> pending;        // inside a managed object: traced with it
};

int main() {
    // A work queue on the stack: pushes at the back, pops at the front,
    // and a window of elements travelling through it allocates no blocks
    sgcl::deque<int> window;
    for (int i : sgcl::range(100000)) {
        window.push_back(i);
        if (window.size() > 16) {
            window.pop_front();                     // the emptied block stays as the spare
        }
    }

    // A scheduler in a managed object; urgent jobs go to the front
    sgcl::tracked_ptr s = sgcl::make_tracked<Scheduler>();
    for (int i : sgcl::range(1000)) {
        sgcl::tracked_ptr job = sgcl::make_tracked<Job>(i);
        if (i % 100 == 0) {
            s->pending.push_front(job);
        } else {
            s->pending.push_back(job);
            s->pending.front()->depends_on.push_back(job);   // the urgent job waits for it
        }
    }

    // Drain the front half: a popped job nothing else refers to is garbage,
    // a popped urgent job lives on while a pending job it waits for is held
    int drained = 0;
    while (s->pending.size() > 500) {
        s->pending.pop_front();
        ++drained;
    }
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    sgcl::collector::force_collect(true);
    std::cout << drained << " jobs drained, " << s->pending.size() << " pending, first is job "
              << s->pending.front()->id << "; " << sgcl::collector::get_live_object_count() << " live objects\n";
    return window.size() == 16 && window.front() == 99984 && s->pending.size() == 500 ? 0 : 1;
}
```

The output:

```
500 jobs drained, 500 pending, first is job 495; 508 live objects
```

The deque `window` dies at the end of `main` and destroys its sixteen elements then; its blocks and its map are freed by the collector once nothing refers to them.

## See also

- [tracked_ptr](../core/tracked_ptr.md), [unique_ptr](../core/unique_ptr.md), [make_tracked](../core/make_tracked.md)
- [vector](vector.md) for a contiguous buffer, [list](list.md) for stable references, [stack](stack.md) and [queue](queue.md), the adapters over a deque
- [README: Containers](README.md#containers), [README: The rules](../core/README.md#the-rules), [README: Stack roots](../../garbage_collector/overview.md#stack-roots)
