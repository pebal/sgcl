# sgcl::list

```cpp
#include "sgcl/core/list.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T>
    class list;
}
```

`sgcl::list<T>` is `std::list` over managed nodes: a circular doubly linked list around a managed sentinel. The interface is the one of `std::list` (constructors, `assign`, `front`/`back`, bidirectional iterators, modifiers at both ends and in the middle, `merge`, `splice`, `remove`, `remove_if`, `reverse`, `unique`, `sort`, three-way comparison, `std::erase`/`std::erase_if`), and so is the behaviour: an element is constructed at insertion and destroyed at removal, references and iterators to the other elements stay valid through every insertion, erasure and relink.

What differs is who frees the nodes. The links are `tracked_ptr`s, so a rooted sentinel keeps every node alive and the list walks them through raw pointers; an `erase` unlinks a node and destroys its element, and the collector reclaims the node later, once nothing refers to it. Nothing is ever freed by hand, so a cycle through a list is collected like any other cycle. The list object is two words (the sentinel and the count); the sentinel is created on first use, so a default-constructed list allocates nothing. A node is as big as its `std` counterpart and pays no malloc rounding ([Benchmarks: Containers](benchmarks.md#containers): 17.8 ns per `push_back` against 26.4 ns for `std::list`, 2.0 ns per step of iteration against 2.2 ns).

## Rules

- A list holds a `tracked_ptr` (the sentinel), so it lives where one may: on a stack or inside a managed object, never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame ([The rules](README.md#the-rules), 1).
- The list destroys an element the moment it is erased, popped, cleared, assigned over or the list is destroyed, exactly like `std::list`. The one exception is a list dying in a sweep, inside a managed object nobody refers to any more: its nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it ([Containers](README.md#containers)).
- An iterator to an erased element is invalid as in `std`: it keeps the node's memory mapped but not the element. A `tracked_ptr` may not address an element of a list ([The rules](README.md#the-rules), 4): elements are reached through the list, its iterators, references and raw pointers.
- Iterators are raw node pointers, trivially copyable and at home in any container, a `std::vector` too: the list roots every linked node. Stepping and dereferencing are plain loads, with no write barrier.
- Relinking operations (`splice`, `merge`, `sort`, `reverse`) move nodes, never elements: every reference stays valid, and a throwing comparator leaves valid lists of the same elements.
- Thread safety is that of `std::list`: concurrent readers, or one writer, with the program's own synchronization. The collector never waits for a mutator and never touches a node the list still links.

## Members

### Types

```cpp
using value_type = T;
using reference = T&;
using const_reference = const T&;
using pointer = T*;
using const_pointer = const T*;
using size_type = size_t;
using difference_type = ptrdiff_t;
using iterator = /* bidirectional iterator over T */;
using const_iterator = /* bidirectional iterator over const T */;
using reverse_iterator = std::reverse_iterator<iterator>;
using const_reverse_iterator = std::reverse_iterator<const_iterator>;
```

`iterator` converts to `const_iterator`; both model `std::bidirectional_iterator`.

### Constructors

```cpp
list() noexcept;
explicit list(size_type count);
list(size_type count, const T& value);
template<std::input_iterator InputIt> list(InputIt first, InputIt last);
template<std::ranges::input_range R> explicit list(R&& r);   // from a range of what T is made of: the pieces of a string, a view, another container (not a list: that is the copy)
list(std::initializer_list<T> ilist);
list(const list& other);
list(list&& other) noexcept;
```

The default constructor allocates nothing, not even the sentinel. `list(count)` holds `count` value-initialized elements, `list(count, value)` `count` copies. A copy has nodes of its own; a move takes the sentinel over and leaves `other` empty. An element constructor that throws leaves the list as it was before that insertion.

```cpp
list<int> zeros(4);                          // 0 0 0 0
list<string> words(2, "x");             // "x" "x"
list digits = {1, 2, 3};
list<int> copy(digits.begin(), digits.end());
list<int> taken = std::move(digits);         // digits is empty now
```

### Destructor

```cpp
~list();
```

Destroys the elements, unless the list dies in a sweep: then its nodes are garbage of the same sweep and destroy their elements themselves. The nodes and the sentinel are left to the collector.

### operator=

```cpp
list& operator=(const list& other);
list& operator=(list&& other) noexcept;
list& operator=(std::initializer_list<T> ilist);
```

Copy assignment is `assign(other.begin(), other.end())`. Move assignment clears this list and takes the other sentinel over, leaving `other` empty.

```cpp
list a = {1, 2, 3};
list<int> b;
b = a;              // a copy, in nodes of its own
b = {4, 5};         // two elements
b = std::move(a);   // a is empty
```

### assign

```cpp
void assign(size_type count, const T& value);
template<std::input_iterator InputIt> void assign(InputIt first, InputIt last);
void assign(std::initializer_list<T> ilist);
```

Replaces the contents: the existing elements are assigned over in their nodes, the surplus erased, the missing ones appended.

```cpp
list l = {1, 2, 3};
l.assign(2, 9);          // 9 9, in the first two nodes
l.assign({7, 8, 9, 10});
```

### front, back

```cpp
reference front();
const_reference front() const;
reference back();
const_reference back() const;
```

The first and the last element; the list must not be empty (debug builds assert).

### Iterators

```cpp
iterator begin() noexcept;                      const_iterator begin() const noexcept;
iterator end() noexcept;                        const_iterator end() const noexcept;
const_iterator cbegin() const noexcept;
const_iterator cend() const noexcept;
reverse_iterator rbegin() noexcept;             const_reverse_iterator rbegin() const noexcept;
reverse_iterator rend() noexcept;               const_reverse_iterator rend() const noexcept;
const_reverse_iterator crbegin() const noexcept;
const_reverse_iterator crend() const noexcept;
```

Bidirectional iterators, raw node pointers: `std::ranges` algorithms that need no random access work on the list. `end()` is the sentinel (null for a list that has none yet, which means the same). An iterator keeps nothing alive by itself and is invalidated only by the erasure of its own element, with one exception: the `end()` of a list that never held an element is a null iterator, and the first insertion, which makes the sentinel, invalidates it, so take a fresh `end()` after it. Used all the same, it does no harm: it no longer compares equal to `end()`, but it still means the end as a position (`insert`, `emplace`, `splice`) and as the end of a range, and a range that starts at it is empty (`erase(stale, end())` erases nothing).

```cpp
list l = {3, 1, 2};
auto it = std::ranges::find(l, 1);
l.erase(it);                                   // 3 2; it is invalid now
for (auto r = l.rbegin(); r != l.rend(); ++r) {
    *r *= 10;                                  // 30 20
}
```

### empty, size, max_size

```cpp
bool empty() const noexcept;
size_type size() const noexcept;
size_type max_size() const noexcept;           // PTRDIFF_MAX
```

`size()` is a word of the list object, kept through every splice.

### clear

```cpp
void clear() noexcept;
```

Destroys every element and unlinks every node; the sentinel stays.

### insert, emplace

```cpp
template<class... A> iterator emplace(const_iterator pos, A&&... a);
iterator insert(const_iterator pos, const T& value);
iterator insert(const_iterator pos, T&& value);
iterator insert(const_iterator pos, size_type count, const T& value);
template<std::input_iterator InputIt> iterator insert(const_iterator pos, InputIt first, InputIt last);
iterator insert(const_iterator pos, std::initializer_list<T> ilist);
```

Inserts before `pos` and returns an iterator to the first inserted element (`pos` itself when nothing is inserted). A node is made and its element constructed in one step, so no node ever holds an unconstructed element; a range is built as a chain first and linked in at once, so a throwing constructor leaves the list as it was.

```cpp
list l = {1, 4};
auto it = l.insert(std::next(l.begin()), 2);   // 1 2 4
l.insert(std::next(it), 2, 3);                 // 1 2 3 3 4
l.emplace(l.end(), 5);                         // 1 2 3 3 4 5
l.insert(l.begin(), {-1, 0});                  // -1 0 1 2 3 3 4 5
```

### erase

```cpp
iterator erase(const_iterator pos);
iterator erase(const_iterator first, const_iterator last);
```

Destroys the elements and unlinks their nodes; returns the iterator to the element after the erased range. `erase(end())` is a no-op, and so is a range that starts at the null `end()` of a list that had no sentinel yet ([Iterators](#iterators)). The nodes are the collector's once unlinked.

```cpp
list l = {1, 2, 3, 4, 5};
auto it = l.erase(l.begin());                  // 2 3 4 5, it -> 2
l.erase(std::next(it), l.end());               // 2
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

Appends an element at the back or the front, in a node of its own, and (`emplace_*`) returns a reference to it.

```cpp
list<tracked_ptr<int>> ptrs;
for (int i : range(1000)) {
    ptrs.push_back(make_tracked<int>(i));
}
int& first = *ptrs.emplace_front(make_tracked<int>(-1));
```

### pop_back, pop_front

```cpp
void pop_back();
void pop_front();
```

Destroys the last or the first element and unlinks its node. The list must not be empty (debug builds assert).

### resize

```cpp
void resize(size_type count);
void resize(size_type count, const T& value);
```

Erases the elements past `count`, or appends value-initialized elements (copies of `value`) up to it.

```cpp
list l = {1, 2, 3};
l.resize(5);          // 1 2 3 0 0
l.resize(2);          // 1 2
l.resize(4, 7);       // 1 2 7 7
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
bool is_sorted() const;  bool binary_search(const auto& value) const;  size_t sorted_index_of(const auto& value) const;   // on a sorted list; and with a comparator
auto lower_bound(const auto& value);  auto upper_bound(const auto& value);   // and const, and with a comparator
void sort();  template<class Compare> void sort(Compare cmp);  template<class Proj> void sort_by(Proj proj);  void stable_sort();   // the list's own, on the nodes
// mixin::sequence
void fill(const auto& value);  void reverse() noexcept;
// mixin::equatable, mixin::comparable: == and <=>, below
```

The members of the mixins every sequence of the library carries ([the mixins](mixin/README.md)): the questions of [mixin::enumerable](mixin/enumerable.md), the order of [mixin::ordered](mixin/ordered.md), the writes of [mixin::sequence](mixin/sequence.md), so that `x.sort()` reads as `x.push_back(x)` does. A question that compares elements exists only for elements that compare; `index_of`, `last_index_of` and `find_index` give the position, or `npos` when nothing matches, `find_if` the element the predicate accepts first, or null. `reverse` and `sort` are the list's own, on the nodes (below).

```cpp
list l = {5, 3, 9, 3};
assert(l.contains(9) && l.index_of(3) == 1 && l.last_index_of(3) == 3 && l.index_of(7) == npos);
assert(l.find_index([](int x) { return x > 4; }) == 0 && l.exists([](int x) { return x == 9; }) && !l.all([](int x) { return x > 3; }));
if (int* big = l.find_if([](int x) { return x > 8; })) {
    *big = 8;
}
assert(l.count_of([](int x) { return x == 3; }) == 2 && l.min() == 3 && l.max() == 8);
l.sort();                                        // 3 3 5 8
assert(l.is_sorted());
l.sort([](int a, int b) { return a > b; });     // 8 5 3 3
l.reverse();                                     // 3 3 5 8
int sum = 0;
l.for_each([&](int x) { sum += x; });         // 19
l.fill(0);
```

### swap

```cpp
void swap(list& other) noexcept;
template<class T> void swap(list<T>& lhs, list<T>& rhs) noexcept;
```

Exchanges the sentinels and the counts; no element is touched.

### merge

```cpp
void merge(list& other);
void merge(list&& other);
template<class Compare> void merge(list& other, Compare comp);
template<class Compare> void merge(list&& other, Compare comp);
```

Merges two sorted lists into this one, stable: of equal elements, those of this list come first. Runs of the other list move over in one relink; the sizes follow each move, so a throwing comparator leaves two valid lists. Merging a list with itself does nothing.

```cpp
list<int> a = {1, 3, 5}, b = {2, 4, 6};
a.merge(b);                                    // a is 1 2 3 4 5 6, b is empty
```

### splice

```cpp
void splice(const_iterator pos, list& other);
void splice(const_iterator pos, list&& other);
void splice(const_iterator pos, list& other, const_iterator it);
void splice(const_iterator pos, list&& other, const_iterator it);
void splice(const_iterator pos, list& other, const_iterator first, const_iterator last);
void splice(const_iterator pos, list&& other, const_iterator first, const_iterator last);
```

Moves the nodes of `other`, the node `it` names, or the nodes in `[first, last)`, before `pos`; no element is copied or destroyed and every iterator stays valid, now naming an element of this list. `other` may be this list. Splicing a range from another list counts its nodes.

```cpp
list<int> a = {1, 2}, b = {3, 4, 5};
a.splice(a.end(), b, b.begin());               // a is 1 2 3, b is 4 5
a.splice(a.begin(), b);                        // a is 4 5 1 2 3, b is empty
```

### remove, remove_if

```cpp
size_type remove(const T& value);
template<class UnaryPredicate> size_type remove_if(UnaryPredicate pred);
```

Erase every element equal to `value`, or satisfying `pred`, and return how many were erased. A `value` that is an element of this list is removed last, after the comparisons that read it.

```cpp
list l = {1, 2, 2, 3, 4};
size_t twos = l.remove(2);                                       // 2; l is 1 3 4
size_t big = l.remove_if([](int x) { return x > 2; });           // 2; l is 1
```

### reverse

```cpp
void reverse() noexcept;
```

Reverses the order of the nodes, in place.

### unique

```cpp
size_type unique();
template<class BinaryPredicate> size_type unique(BinaryPredicate pred);
```

Erases every element equal to the one before it (`pred(previous, current)`), keeping the first of each run; returns how many were erased.

```cpp
list l = {1, 1, 2, 2, 2, 3};
size_t dropped = l.unique();                   // 3; l is 1 2 3
```

### sort

```cpp
void sort();
template<class Compare> void sort(Compare comp);
```

A stable merge sort in place: the nodes are relinked within the list, never detached, so a throwing comparator leaves a valid list of the same elements.

```cpp
list l = {3, 1, 2};
l.sort();                                      // 1 2 3
l.sort(std::greater<>());                      // 3 2 1
```

### Comparisons

```cpp
friend bool operator==(const list& l, const list& r);
friend auto operator<=>(const list& l, const list& r);
```

From [mixin::equatable](mixin/equatable.md) and [mixin::comparable](mixin/comparable.md), for elements that compare. Element-wise, as for `std::list`: `==` compares sizes first, `<=>` is lexicographical with the synthesized three-way comparison (`<=>` of `T` when it has one, else a `std::weak_ordering` built from `<`), so `<`, `<=`, `>`, `>=` and `!=` follow.

```cpp
list<int> a = {1, 2}, b = {1, 3};
bool less = a < b;                  // true
bool same = a == b;                 // false
```

### std::erase, std::erase_if

```cpp
template<class T, class U> typename list<T>::size_type erase(list<T>& c, const U& value);
template<class T, class Pred> typename list<T>::size_type erase_if(list<T>& c, Pred pred);
```

Declared in `sgcl` and brought into `std`, as the other containers' are, so an unqualified call finds them too. `remove_if` under the standard names: erase every element equal to `value`, or satisfying `pred`, and return how many were erased.

```cpp
list l = {1, 2, 2, 3, 4};
size_t twos = std::erase(l, 2);                                  // 2; l is 1 3 4
size_t big = std::erase_if(l, [](int x) { return x > 2; });      // 2; l is 1
```

### list<unique_ptr<T>>

```cpp
template<typename T>
class list<unique_ptr<T>> : public std::list<unique_ptr<T>>;
```

A `unique_ptr` owns its object and needs no tracing, so a list of them is a plain `std::list` with the constructors of the base: it may live anywhere a `std::list` may, and the objects die when their `unique_ptr` does.

```cpp
list<unique_ptr<int>> owned;
owned.push_back(make_tracked<int>(1));
owned.pop_front();                                 // the int is destroyed here, deterministically
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

struct Item {
    int key;
    tracked_ptr<Item> twin;                     // items may point at each other
};

struct Registry {
    list<tracked_ptr<Item>> items;          // inside a managed object: traced with it
};

int main() {
    // A list of values on the stack: the nodes are on the managed heap
    list numbers = {5, 3, 9, 1};
    numbers.push_front(7);
    numbers.sort();                                 // 1 3 5 7 9, the nodes relinked in place
    numbers.remove_if([](int x) { return x % 2 == 0; });

    // A registry in a managed object; the items live in its list
    tracked_ptr r = make_tracked<Registry>();
    for (int i : range(1000)) {
        r->items.push_back(make_tracked<Item>(i));
    }
    r->items.front()->twin = r->items.back();       // a cycle through the list: collected like any other
    r->items.back()->twin = r->items.front();

    // An iterator survives every other erasure: erase the odd keys around it
    auto kept = std::next(r->items.begin(), 500);
    for (auto it = r->items.begin(); it != r->items.end();) {
        it = (*it)->key % 2 ? r->items.erase(it) : std::next(it);   // the unlinked nodes are the collector's
    }
    list<tracked_ptr<Item>> moved;
    moved.splice(moved.end(), r->items, kept);      // the node moves, the iterator still names it

    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    collector::force_collect(true);
    std::cout << numbers.size() << " odd numbers, " << r->items.size() << " items left in the registry, "
              << "item " << (*kept)->key << " moved out; "
              << collector::get_live_object_count() << " live objects\n";
    return numbers.size() == 5 && r->items.size() == 499 && moved.front()->key == 500 ? 0 : 1;
}
```

The output:

```
5 odd numbers, 499 items left in the registry, item 500 moved out; 1010 live objects
```

The lists `numbers` and `moved` die at the end of `main` and destroy their elements then; their nodes are freed by the collector once nothing refers to them.

## See also

- [tracked_ptr](tracked_ptr.md), [unique_ptr](unique_ptr.md), [make_tracked](make_tracked.md)
- [forward_list](forward_list.md) for a singly linked list, [deque](deque.md) and [vector](vector.md) for random access
- [README: Containers](README.md#containers), [README: The rules](README.md#the-rules), [README: Stack roots](../../garbage_collector/overview.md#stack-roots)
