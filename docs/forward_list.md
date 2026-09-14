# sgcl::forward_list

```cpp
#include "sgcl/forward_list.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T, template<class> class Ptr = tracked_ptr>
    class forward_list;
}
```

`sgcl::forward_list<T>` is `std::forward_list` over managed nodes: singly linked nodes behind a sentinel, the one `before_begin()` addresses. The interface is the one of `std::forward_list` (constructors, `assign`, `front`, forward iterators, `insert_after`, `emplace_after`, `erase_after`, `push_front`, `merge`, `splice_after`, `remove`, `remove_if`, `reverse`, `unique`, `sort`, three-way comparison, `std::erase`/`std::erase_if`; no `size()`), and so is the behaviour: an element is constructed at insertion and destroyed at removal, references and iterators to the other elements stay valid through every insertion, erasure and relink.

`Ptr`, the last parameter, is the kind of the word by which the container holds its memory: `tracked_ptr` by default, so that the container lives where a `tracked_ptr` may (on a stack or inside a managed object), or [`gc::tracked_ptr`](gc/tracked_ptr.md), so that it lives anywhere, at the cost of a `gc::tracked_ptr` on each access to that word; `gc::forward_list` ([gc/gc.h](README.md#the-gc-namespace)) names the latter. The nodes and buffers are the same managed objects either way, and the elements are a choice apart; an element type that names a `tracked_type` (`gc::tracked_ptr<T>` names `sgcl::tracked_ptr<T>`) is stored as that type, one word in the same mode, and handed out as the type it was given, so a container of `gc::tracked_ptr`s costs what one of `sgcl::tracked_ptr`s does ([the gc namespace](README.md#the-gc-namespace)).

What differs is who frees the nodes. The links are `tracked_ptr`s, so the rooted sentinel keeps every node alive and the list walks them through raw pointers; an `erase_after` unlinks a node and destroys its element, and the collector reclaims the node later, once nothing refers to it. Nothing is ever freed by hand, so a cycle through a list is collected like any other cycle. The list object is one word, the sentinel, which every list owns from its construction on (a default-constructed list allocates it), so `insert_after` and `erase_after` work the same at any position; the sentinel is a bare link with no element. A node is as big as its `std` counterpart and pays no malloc rounding ([Benchmarks: Containers](../README.md#containers-1): 14.5 ns per `push_front` against 22.1 ns for `std::forward_list`).

## Rules

- A forward_list holds a `tracked_ptr` (the sentinel), so it lives where one may: on a stack or inside a managed object, never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame ([The rules](../README.md#the-rules), 1).
- The list destroys an element the moment it is erased, popped, cleared, assigned over or the list is destroyed, exactly like `std::forward_list`. The one exception is a list dying in a sweep, inside a managed object nobody refers to any more: its nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it ([Containers](../README.md#containers)).
- An iterator to an erased element is invalid as in `std`: it keeps the node's memory mapped but not the element. A `tracked_ptr` may not address an element of a list ([The rules](../README.md#the-rules), 4): elements are reached through the list, its iterators, references and raw pointers.
- Iterators are raw node pointers, trivially copyable and at home in any container, a `std::vector` too: the list roots every linked node. Stepping and dereferencing are plain loads, with no write barrier.
- Relinking operations (`splice_after`, `merge`, `sort`, `reverse`) move nodes, never elements, and every node stays reachable through a `tracked_ptr` while it is being moved: every reference stays valid.
- Thread safety is that of `std::forward_list`: concurrent readers, or one writer, with the program's own synchronization. The collector never waits for a mutator and never touches a node the list still links.

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
using iterator = /* forward iterator over T */;
using const_iterator = /* forward iterator over const T */;
```

`iterator` converts to `const_iterator`; both model `std::forward_iterator`.

### Constructors

```cpp
forward_list();
explicit forward_list(size_type count) requires std::default_initializable<T>;
forward_list(size_type count, const T& value);
template<std::input_iterator InputIt> forward_list(InputIt first, InputIt last);
forward_list(std::initializer_list<T> ilist);
forward_list(const forward_list& other);
forward_list(forward_list&& other);
```

Every constructor allocates the sentinel. `forward_list(count)` holds `count` value-initialized elements, `forward_list(count, value)` `count` copies. A copy has nodes of its own; a move takes the chain of `other`, which keeps its sentinel and is empty. An element constructor that throws leaves the list empty and the exception propagates.

```cpp
gc::forward_list<int> zeros(4);                          // 0 0 0 0
gc::forward_list<std::string> words(2, "x");             // "x" "x"
gc::forward_list<int> digits = {1, 2, 3};
gc::forward_list<int> copy(digits.begin(), digits.end());
gc::forward_list<int> taken = std::move(digits);         // digits is empty now
```

### Destructor

```cpp
~forward_list();
```

Destroys the elements, unless the list dies in a sweep: then its nodes are garbage of the same sweep and destroy their elements themselves. The nodes and the sentinel are left to the collector.

### operator=

```cpp
forward_list& operator=(const forward_list& other);
forward_list& operator=(forward_list&& other) noexcept;
forward_list& operator=(std::initializer_list<T> ilist);
```

Copy assignment is `assign(other.begin(), other.end())`. Move assignment clears this list and takes the chain of `other`, which is empty afterwards.

```cpp
gc::forward_list<int> a = {1, 2, 3};
gc::forward_list<int> b;
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
gc::forward_list<int> l = {1, 2, 3};
l.assign(2, 9);          // 9 9, in the first two nodes
l.assign({7, 8, 9, 10});
```

### front

```cpp
reference front();
const_reference front() const;
```

The first element; the list must not be empty.

### Iterators

```cpp
iterator before_begin() noexcept;               const_iterator before_begin() const noexcept;
const_iterator cbefore_begin() const noexcept;
iterator begin() noexcept;                      const_iterator begin() const noexcept;
const_iterator cbegin() const noexcept;
iterator end() noexcept;                        const_iterator end() const noexcept;
const_iterator cend() const noexcept;
```

Forward iterators, raw node pointers. `before_begin()` is the sentinel, the position to insert or erase after for the front; it must not be dereferenced. `end()` is a null iterator. An iterator keeps nothing alive by itself and is invalidated only by the erasure of its own element.

```cpp
gc::forward_list<int> l = {3, 1, 2};
auto it = std::ranges::find(l, 1);
*it = 10;                                      // 3 10 2
auto before = l.before_begin();
l.insert_after(before, 0);                     // 0 3 10 2
```

### empty, max_size

```cpp
bool empty() const noexcept;
size_type max_size() const noexcept;           // PTRDIFF_MAX
```

There is no `size()`, as in `std::forward_list`: `std::ranges::distance(l)` counts the nodes.

### clear

```cpp
void clear() noexcept;
```

Destroys every element and unlinks every node; the sentinel stays.

### insert_after, emplace_after

```cpp
iterator insert_after(const_iterator pos, const T& value);
iterator insert_after(const_iterator pos, T&& value);
iterator insert_after(const_iterator pos, size_type count, const T& value);
template<std::input_iterator InputIt> iterator insert_after(const_iterator pos, InputIt first, InputIt last);
iterator insert_after(const_iterator pos, std::initializer_list<T> ilist);
template<class... A> iterator emplace_after(const_iterator pos, A&&... a);
```

Inserts after `pos` (`before_begin()` for the front) and returns an iterator to the last inserted element (`pos` when nothing is inserted). A node is made and its element constructed in one step, so no node ever holds an unconstructed element; a range is built as a chain and linked in, so a throwing constructor leaves the list as it was.

```cpp
gc::forward_list<int> l = {1, 4};
auto it = l.insert_after(l.begin(), 2);        // 1 2 4, it -> 2
it = l.insert_after(it, 2, 3);                 // 1 2 3 3 4, it -> the second 3
l.emplace_after(it, 9);                        // 1 2 3 3 9 4
l.insert_after(l.before_begin(), {-1, 0});     // -1 0 1 2 3 3 9 4
```

### erase_after

```cpp
iterator erase_after(const_iterator pos);
iterator erase_after(const_iterator first, const_iterator last);
```

Destroys the element after `pos`, or the elements in `(first, last)`, and unlinks their nodes; returns the iterator to the element after the erased ones (`end()` when there is none after `pos`). The nodes are the collector's once unlinked.

```cpp
gc::forward_list<int> l = {1, 2, 3, 4, 5};
auto it = l.erase_after(l.before_begin());     // 2 3 4 5, it -> 2
l.erase_after(it, l.end());                    // 2
```

### push_front, emplace_front, pop_front

```cpp
void push_front(const T& value);
void push_front(T&& value);
template<class... A> reference emplace_front(A&&... a);
void pop_front();
```

Prepends an element in a node of its own, (`emplace_front`) returning a reference to it; `pop_front` destroys the first element and unlinks its node. The list must not be empty for `pop_front`.

```cpp
gc::forward_list<gc::tracked_ptr<int>> ptrs;
for (int i = 0; i < 1000; ++i) {
    ptrs.push_front(gc::make_tracked<int>(i));
}
int& first = *ptrs.emplace_front(gc::make_tracked<int>(-1));
ptrs.pop_front();                                  // the int behind `first` is unreachable now
```

### resize

```cpp
void resize(size_type count) requires std::default_initializable<T>;
void resize(size_type count, const value_type& value);
```

Erases the elements past `count`, or appends value-initialized elements (copies of `value`) up to it; one walk of the list.

```cpp
gc::forward_list<int> l = {1, 2, 3};
l.resize(5);          // 1 2 3 0 0
l.resize(2);          // 1 2
l.resize(4, 7);       // 1 2 7 7
```

### swap

```cpp
void swap(forward_list& other) noexcept;
template<class T> void swap(forward_list<T>& lhs, forward_list<T>& rhs) noexcept;
```

Exchanges the sentinels; no element is touched.

### merge

```cpp
void merge(forward_list& other);
void merge(forward_list&& other);
template<class Compare> void merge(forward_list& other, Compare comp);
template<class Compare> void merge(forward_list&& other, Compare comp);
```

Merges two sorted lists into this one, stable: of equal elements, those of this list come first; `other` is empty afterwards. The nodes are relinked, never copied.

```cpp
gc::forward_list<int> a = {1, 3, 5}, b = {2, 4, 6};
a.merge(b);                                    // a is 1 2 3 4 5 6, b is empty
```

### splice_after

```cpp
void splice_after(const_iterator pos, forward_list& other);
void splice_after(const_iterator pos, forward_list&& other);
void splice_after(const_iterator pos, forward_list& other, const_iterator it);
void splice_after(const_iterator pos, forward_list&& other, const_iterator it);
void splice_after(const_iterator pos, forward_list& other, const_iterator first, const_iterator last);
void splice_after(const_iterator pos, forward_list&& other, const_iterator first, const_iterator last);
```

Moves the nodes of `other`, the node after `it`, or the nodes in `(first, last)`, after `pos`; no element is copied or destroyed and every iterator stays valid, now naming an element of this list. `other` may be this list. Moving all of `other` walks it to its last node.

```cpp
gc::forward_list<int> a = {1, 2}, b = {3, 4, 5};
a.splice_after(a.begin(), b, b.before_begin());   // a is 1 3 2, b is 4 5
a.splice_after(a.before_begin(), b);              // a is 4 5 1 3 2, b is empty
```

### remove, remove_if

```cpp
size_type remove(const T& value);
template<class UnaryPredicate> size_type remove_if(UnaryPredicate pred);
```

Erase every element equal to `value`, or satisfying `pred`, and return how many were erased.

```cpp
gc::forward_list<int> l = {1, 2, 2, 3, 4};
size_t twos = l.remove(2);                                       // 2; l is 1 3 4
size_t big = l.remove_if([](int x) { return x > 2; });           // 2; l is 1
```

### unique

```cpp
size_type unique();
template<class BinaryPredicate> size_type unique(BinaryPredicate pred);
```

Erases every element equal to the one before it (`pred(previous, current)`), keeping the first of each run; returns how many were erased.

```cpp
gc::forward_list<int> l = {1, 1, 2, 2, 2, 3};
size_t dropped = l.unique();                   // 3; l is 1 2 3
```

### reverse

```cpp
void reverse() noexcept;
```

Reverses the order of the nodes, in place.

### sort

```cpp
void sort();
template<class Compare> void sort(Compare comp);
```

A stable merge sort in place: the nodes are relinked, never detached.

```cpp
gc::forward_list<int> l = {3, 1, 2};
l.sort();                                      // 1 2 3
l.sort(std::greater<>());                      // 3 2 1
```

### Comparisons

```cpp
friend bool operator==(const forward_list& lhs, const forward_list& rhs);
friend auto operator<=>(const forward_list& lhs, const forward_list& rhs);
```

Element-wise, as for `std::forward_list`, in one walk of both lists: `<=>` is lexicographical with the synthesized three-way comparison (`<=>` of `T` when it has one, else a `std::weak_ordering` built from `<`), so `<`, `<=`, `>`, `>=` and `!=` follow.

```cpp
gc::forward_list<int> a = {1, 2}, b = {1, 3};
bool less = a < b;                  // true
bool same = a == b;                 // false
```

### std::erase, std::erase_if

```cpp
template<class T, class U> typename forward_list<T>::size_type erase(forward_list<T>& c, const U& value);
template<class T, class Pred> typename forward_list<T>::size_type erase_if(forward_list<T>& c, Pred pred);
```

Declared in `sgcl` and brought into `std`: `remove_if` under the standard names, returning how many elements were erased.

```cpp
gc::forward_list<int> l = {1, 2, 2, 3, 4};
size_t twos = std::erase(l, 2);                                  // 2; l is 1 3 4
size_t big = std::erase_if(l, [](int x) { return x > 2; });      // 2; l is 1
```

### forward_list<unique_ptr<T>>

```cpp
template<class T>
class forward_list<unique_ptr<T>> : public std::forward_list<unique_ptr<T>>;
```

A `unique_ptr` owns its object and needs no tracing, so a forward_list of them is a plain `std::forward_list` with the constructors of the base: it may live anywhere a `std::forward_list` may, and the objects die when their `unique_ptr` does.

```cpp
gc::forward_list<gc::unique_ptr<int>> owned;
owned.push_front(gc::make_tracked<int>(1));
owned.pop_front();                                 // the int is destroyed here, deterministically
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>
#include <ranges>

struct Vertex {
    int id;
    gc::forward_list<gc::tracked_ptr<Vertex>> edges;       // an adjacency list, inside the vertex
};

int main() {
    // A list of values on the stack: the nodes are on the managed heap
    gc::forward_list<int> numbers = {5, 3, 9, 1};
    numbers.push_front(7);
    numbers.sort();                                 // 1 3 5 7 9, the nodes relinked in place
    numbers.remove_if([](int x) { return x > 5; });

    // A graph: every vertex points at every other, so every vertex is in a cycle
    gc::forward_list<gc::tracked_ptr<Vertex>> vertices;
    for (int i = 0; i < 100; ++i) {
        vertices.push_front(gc::make_tracked<Vertex>(i));
    }
    for (const auto& v : vertices) {
        for (const auto& w : vertices) {
            if (v != w) {
                v->edges.push_front(w);
            }
        }
    }

    // Keep one vertex, drop the list: the whole graph stays reachable through it
    gc::tracked_ptr keep = vertices.front();
    vertices.clear();
    keep->edges.remove_if([](const gc::tracked_ptr<Vertex>& w) { return w->id % 2; });     // the odd ids, kept by the cycle still

    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    gc::collector::force_collect(true);
    int edges = int(std::ranges::distance(keep->edges));
    std::cout << std::ranges::distance(numbers) << " numbers, vertex " << keep->id << " with "
              << edges << " edges, " << gc::collector::get_live_object_count() << " live objects\n";

    keep = nullptr;                                 // the cycle is unreachable now: collected as a whole
    gc::collector::force_collect(true);             // optional, as above
    std::cout << gc::collector::get_live_object_count() << " live objects after the graph is gone\n";
    return std::ranges::distance(numbers) == 3 && edges == 50 ? 0 : 1;
}
```

The list `numbers` dies at the end of `main` and destroys its elements then; its nodes are freed by the collector once nothing refers to them.

## See also

- [tracked_ptr](tracked_ptr.md), [unique_ptr](unique_ptr.md), [make_tracked](make_tracked.md)
- [list](list.md) for a doubly linked list with `size()`, `push_back` and reverse iteration
- [README: Containers](../README.md#containers), [README: The rules](../README.md#the-rules), [README: Stack roots](../README.md#stack-roots)
