# sgcl::multimap

```cpp
#include "sgcl/multimap.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class Key, class T, class Compare = std::less<Key>, template<class> class Ptr = tracked_ptr>
    class multimap;
}
```

`sgcl::multimap<Key, T, Compare>` is `std::multimap` on a red-black tree whose nodes are managed objects: the same tree as [map](map.md), with equivalent keys allowed. The interface is the one of `std::multimap` (constructors, `insert`, `emplace`, `erase`, `extract`, `merge`, node handles, the lookups with transparent comparators, bidirectional iterators, `key_comp`/`value_comp`, `swap`, `==` and `<=>`, `std::erase_if`), and so is the behaviour: elements with equivalent keys are adjacent, a new one goes after the ones already there (insertion order within a key is kept), `erase(key)` removes all of them, an element is destroyed the moment it is erased.

`Ptr`, the last parameter, is the kind of the word by which the container holds its memory: `tracked_ptr` by default, so that the container lives where a `tracked_ptr` may (on a stack or inside a managed object), or [`gc::tracked_ptr`](gc/tracked_ptr.md), so that it lives anywhere, at the cost of a `gc::tracked_ptr` on each access to that word; `gc::multimap` ([gc/gc.h](README.md#the-gc-namespace)) names the latter. The nodes and buffers are the same managed objects either way, and the elements are a choice apart; an element type that names a `tracked_type` (`gc::tracked_ptr<T>` names `sgcl::tracked_ptr<T>`) is stored as that type, one word in the same mode, and handed out as the type it was given, so a container of `gc::tracked_ptr`s costs what one of `sgcl::tracked_ptr`s does ([the gc namespace](README.md#the-gc-namespace)).

What differs from `std` is where the memory lives. The multimap object holds one `tracked_ptr` (to a header node), a count and the comparator, so it lives where a `tracked_ptr` may live; the nodes are managed objects linked by tracked pointers, traced from the header, so elements holding `tracked_ptr`s are traced and a cycle through a multimap is collected like any other. Nothing is freed by hand: an `erase` destroys the element and unlinks the node, the collector reclaims the node later. Iterators are one raw node pointer each, trivially copyable, storable anywhere, valid while their element is in the container. Lookups and iteration read raw pointers and pay no write barrier; insertions, erasures and rebalancing store tracked pointers and pay the barrier on each link they relink ([README: Containers](../README.md#containers)). The header is allocated on the first insertion: an empty multimap costs nothing.

## Rules

- A multimap holds a `tracked_ptr`, so it lives on a stack or inside a managed object: never in `new`/`malloc` memory, a `std` container, a global, a `thread_local` or a plain coroutine frame ([The rules](../README.md#the-rules), 1). The same holds for a node handle.
- The elements may hold tracked pointers: the nodes are managed objects, so those pointers are traced.
- An element is destroyed the moment it is erased, cleared, assigned over, or the multimap is destroyed, exactly as in `std`. The one exception is a multimap dying in a sweep, inside a managed object nobody refers to any more: its nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it, on a collector thread.
- An iterator, a reference or a pointer to an element is valid while the element is in the container, across insertions, erasures of other elements, `swap`, `merge` and a move of the container. An iterator to an erased element is invalid as in `std`.
- A `tracked_ptr` may point at an element or a member of one (a node is a managed object); it keeps the node alive, not the element.
- Thread safety is that of `std::multimap`: concurrent readers, or one writer, with the program's own synchronization ([The rules](../README.md#the-rules), 6).

## Members

### Types

```cpp
using key_type = Key;
using mapped_type = T;
using value_type = std::pair<const Key, T>;
using key_compare = Compare;
using value_compare = /* compares two value_type by key with Compare */;
using size_type = size_t;
using difference_type = ptrdiff_t;
using reference = value_type&;
using const_reference = const value_type&;
using pointer = value_type*;
using const_pointer = const value_type*;
using iterator = /* bidirectional, one raw node pointer */;
using const_iterator = /* bidirectional, one raw node pointer */;
using reverse_iterator = std::reverse_iterator<iterator>;
using const_reverse_iterator = std::reverse_iterator<const_iterator>;
using node_type = /* the node handle, below */;
static constexpr bool Multi = true;
```

`iterator` converts to `const_iterator`, not back. There is no `insert_return_type`: every `insert` returns an iterator.

### Constructors

```cpp
multimap() noexcept(std::is_nothrow_default_constructible_v<Compare>);
explicit multimap(const Compare& comp);
template<std::input_iterator InputIt> multimap(InputIt first, InputIt last, const Compare& comp = Compare());
multimap(std::initializer_list<value_type> ilist, const Compare& comp = Compare());
multimap(const multimap& other);
multimap(multimap&& other) noexcept(std::is_nothrow_move_constructible_v<Compare>);
```

The default constructor allocates nothing. The range and list constructors insert in order with `end()` as the hint (sorted input costs one comparison per element), keeping every element. A copy has nodes of its own in the same order; a move takes the tree over and leaves `other` empty. A constructor or comparator that throws destroys the elements built so far.

```cpp
gc::multimap<std::string, int> scores = {{"ann", 3}, {"bob", 5}, {"ann", 7}};     // ann 3, ann 7, bob 5
gc::multimap<int, int, std::greater<int>> desc(std::greater<int>{});
gc::multimap<std::string, int> taken = std::move(scores);                         // scores is empty now
```

### Destructor

```cpp
~multimap();
```

Destroys the elements when the multimap dies on a stack or inside a managed object destroyed by hand; in a sweep it leaves the nodes to the same sweep, which destroys the elements. The node memory is reclaimed by the collector in both cases.

### operator=

```cpp
multimap& operator=(const multimap& other);
multimap& operator=(multimap&& other) noexcept(std::is_nothrow_move_assignable_v<Compare>);
multimap& operator=(std::initializer_list<value_type> ilist);
```

Copy assignment clears this container (destroying its elements at once), takes `other`'s comparator and inserts copies of its elements; move assignment clears and takes the tree over; the list form clears and inserts.

```cpp
gc::multimap<int, int> a = {{1, 1}, {1, 2}}, b;
b = a;
b = {{5, 5}};                // the old elements die here
a = std::move(b);            // a holds {5, 5}, b is empty
```

### key_comp, value_comp

```cpp
key_compare key_comp() const;
value_compare value_comp() const;
```

Copies of the comparator, for keys and for `value_type`s (compared by key).

```cpp
gc::multimap<int, int> m = {{1, 1}, {2, 2}};
bool by_key = m.key_comp()(1, 2);                          // true
bool by_value = m.value_comp()(*m.begin(), *m.rbegin());   // true
```

### Iterators

```cpp
iterator begin() noexcept;                       const_iterator begin() const noexcept;
iterator end() noexcept;                         const_iterator end() const noexcept;
const_iterator cbegin() const noexcept;          const_iterator cend() const noexcept;
reverse_iterator rbegin() noexcept;              const_reverse_iterator rbegin() const noexcept;
reverse_iterator rend() noexcept;                const_reverse_iterator rend() const noexcept;
const_reverse_iterator crbegin() const noexcept; const_reverse_iterator crend() const noexcept;
```

`begin()` is the smallest key in O(1), `--end()` the largest; equivalent keys come in insertion order. Before the first insertion `begin()` and `end()` are both null iterators, equal to each other, neither of which may be dereferenced or moved; an `end()` taken then does not compare equal to `end()` after the first insertion. Iterators are raw node pointers: copying and advancing costs a load, and they may be kept in unmanaged memory while their element is in the container.

```cpp
gc::multimap<std::string, int> m = {{"b", 2}, {"a", 1}, {"a", 3}};
for (auto& [key, value] : m) {          // a 1, a 3, b 2
    value *= 10;
}
auto largest = std::prev(m.end());      // "b"
```

### empty, size, max_size

```cpp
[[nodiscard]] bool empty() const noexcept;
size_type size() const noexcept;
size_type max_size() const noexcept;
```

`size()` is a stored count, O(1).

### clear

```cpp
void clear() noexcept;
```

Destroys every element at once and unlinks every node; the header stays. The nodes are reclaimed by the collector.

```cpp
gc::multimap<int, std::string> m = {{1, "a"}, {1, "b"}};
m.clear();                     // both strings are destroyed here
bool gone = m.empty();         // true
```

### insert

```cpp
iterator insert(const value_type& value);
iterator insert(value_type&& value);
template<class P> requires std::is_constructible_v<value_type, P&&> iterator insert(P&& value);
iterator insert(const_iterator hint, const value_type& value);
iterator insert(const_iterator hint, value_type&& value);
template<class P> requires std::is_constructible_v<value_type, P&&> iterator insert(const_iterator hint, P&& value);
template<std::input_iterator InputIt> void insert(InputIt first, InputIt last);
void insert(std::initializer_list<value_type> ilist);
iterator insert(node_type&& nh);
iterator insert(const_iterator hint, node_type&& nh);
```

Always inserts; a key already present gets the new element after its equivalents. The `P&&` forms build the element through `emplace`. The hinted forms are O(1) amortized when the element belongs right before `hint`; an append in sorted order at `end()` costs one comparison. The range and list forms insert one by one with `end()` as the hint. The node-handle forms link the node of `nh` without copying the element and leave `nh` empty; an empty handle inserts nothing and returns `end()`.

```cpp
gc::multimap<std::string, int> m;
auto it = m.insert({"a", 1});
m.insert({"a", 2});                                   // after the first "a"
m.insert(m.end(), std::pair<const char*, int>("z", 26));   // an append: one comparison
m.insert({{"b", 2}, {"b", 3}});
gc::multimap<std::string, int> other = {{"q", 17}};
m.insert(other.extract("q"));                         // relinked, no copy
bool order = std::next(it)->second == 2;              // true
```

### emplace, emplace_hint

```cpp
template<class... A> iterator emplace(A&&... a);
template<class... A> iterator emplace_hint(const_iterator hint, A&&... a);
```

Builds the `value_type` from `a...` in a new node and links it after its equivalents (or where `hint` says, when that is right). A comparator that throws destroys the new element and leaves the container as it was.

```cpp
gc::multimap<std::string, std::string> m;
m.emplace("k", "v");
m.emplace(std::piecewise_construct, std::forward_as_tuple("k"), std::forward_as_tuple(3, 'x'));   // a second "k"
auto it = m.emplace_hint(m.end(), "z", "last");
```

### erase

```cpp
iterator erase(iterator pos);
iterator erase(const_iterator pos);
iterator erase(const_iterator first, const_iterator last);
size_type erase(const key_type& key);
```

Destroys the element at once, unlinks the node (the collector reclaims it later) and returns the iterator after it. The key form erases every element with an equivalent key and returns how many. Erasing `[begin(), end())` is a `clear()`. There is no transparent `erase`.

```cpp
gc::multimap<int, std::string> m = {{1, "a"}, {1, "b"}, {2, "c"}};
auto erased = m.erase(1);                          // 2: "a" and "b" are destroyed here
for (auto it = m.begin(); it != m.end();) {
    it = it->first == 2 ? m.erase(it) : std::next(it);
}
```

### swap

```cpp
void swap(multimap& other) noexcept(std::is_nothrow_swappable_v<Compare>);
friend void swap(multimap& lhs, multimap& rhs) noexcept(noexcept(lhs.swap(rhs)));   // free function in namespace sgcl
```

Exchanges the trees, counts and comparators; no element is touched, and every iterator keeps pointing at its element, now in the other container.

```cpp
gc::multimap<int, int> a = {{1, 1}}, b = {{2, 2}};
auto it = a.begin();
swap(a, b);                       // it still points at {1, 1}, which is in b now
bool moved = it == b.begin();     // true
```

### extract

```cpp
node_type extract(iterator pos);
node_type extract(const_iterator pos);
node_type extract(const key_type& key);
```

Unlinks the node and hands it over in a node handle, the element untouched; the handle destroys the element if it dies unused. The key form extracts the first element with an equivalent key, or returns an empty handle. See [node_type](#node_type-the-node-handle).

```cpp
gc::multimap<int, std::string> m = {{1, "a"}, {1, "b"}};
auto nh = m.extract(1);           // {1, "a"}; m holds {1, "b"}
nh.key() = 2;
m.insert(std::move(nh));          // {1, "b"}, {2, "a"}
```

### merge

```cpp
template<class Traits2> void merge(detail::RbTree<Traits2>& source);    // any gc::map or multimap<Key, T, C2>
template<class Traits2> void merge(detail::RbTree<Traits2>&& source);
```

Relinks every node of `source` into this multimap (a multi tree takes them all), each after its equivalents, and leaves `source` empty. No element is copied or destroyed; iterators follow their nodes. `source` may be a `sgcl::map` or `sgcl::multimap` with the same `Key` and `T` and any comparator.

```cpp
gc::multimap<int, int> a = {{1, 1}, {3, 3}};
gc::map<int, int> b = {{2, 2}, {3, 30}};
a.merge(b);                       // a: 1, 2, 3, 3(30);  b is empty
```

### count, find, contains

```cpp
size_type count(const key_type& key) const;
iterator find(const key_type& key);
const_iterator find(const key_type& key) const;
bool contains(const key_type& key) const;
template<class K> size_type count(const K& key) const;                 // when Compare::is_transparent
template<class K> iterator find(const K& key);                          //   "
template<class K> const_iterator find(const K& key) const;              //   "
template<class K> bool contains(const K& key) const;                    //   "
```

`find` returns the first element with an equivalent key, `count` how many there are (O(log n + count)). The `K` overloads exist for a transparent comparator.

```cpp
gc::multimap<std::string, int, std::less<>> m = {{"a", 1}, {"a", 2}};
std::string_view key = "a";
auto n = m.count(key);             // 2, no std::string built
auto first = m.find(key);          // {"a", 1}
```

### equal_range, lower_bound, upper_bound

```cpp
std::pair<iterator, iterator> equal_range(const key_type& key);
std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const;
iterator lower_bound(const key_type& key);
const_iterator lower_bound(const key_type& key) const;
iterator upper_bound(const key_type& key);
const_iterator upper_bound(const key_type& key) const;
template<class K> ... equal_range(const K& key);      // when Compare::is_transparent, same set of overloads
template<class K> ... lower_bound(const K& key);      //   "
template<class K> ... upper_bound(const K& key);      //   "
```

`equal_range` is the run of elements with an equivalent key, in insertion order; `lower_bound` its start, `upper_bound` its end.

```cpp
gc::multimap<int, char> m = {{1, 'a'}, {2, 'b'}, {2, 'c'}, {3, 'd'}};
auto [from, to] = m.equal_range(2);
std::string run;
for (auto it = from; it != to; ++it) {
    run += it->second;             // "bc"
}
```

### Comparisons

```cpp
friend bool operator==(const multimap& lhs, const multimap& rhs);
friend auto operator<=>(const multimap& lhs, const multimap& rhs);
```

Element-wise in iteration order, as for `std::multimap`: `==` compares sizes first; `<=>` is lexicographical with the synthesized three-way comparison, so `!=`, `<`, `<=`, `>` and `>=` follow.

```cpp
gc::multimap<int, int> a = {{1, 1}, {1, 2}}, b = {{1, 1}, {1, 3}};
bool less = a < b;                                // true
bool same = a == b;                               // false
```

### _check

```cpp
bool _check() const;
```

Verifies the red-black invariants, the header links, the order and the count; for the tests. O(n).

### node_type (the node handle)

```cpp
class node_type {
public:
    using key_type = Key;
    using mapped_type = T;
    node_type() noexcept;
    node_type(node_type&&) noexcept;
    node_type& operator=(node_type&&) noexcept;
    ~node_type();
    [[nodiscard]] bool empty() const noexcept;
    explicit operator bool() const noexcept;
    key_type& key() const noexcept;        // writable: the node is out of any container
    mapped_type& mapped() const noexcept;
};
```

Owns one unlinked node: movable, not copyable; the element is destroyed when the handle dies without having been inserted. The handle holds the node through a `tracked_ptr`, so it lives on a stack or inside a managed object. It is the same handle type as `sgcl::map<Key, T>::node_type`. There is no `swap` member.

### std::erase_if

```cpp
namespace sgcl {
    template<class Key, class T, class Compare, class Pred>
    typename multimap<Key, T, Compare>::size_type erase_if(multimap<Key, T, Compare>& c, Pred pred);
}
namespace std { using sgcl::erase_if; }
```

Erases every element for which `pred(*it)` is true and returns how many.

```cpp
gc::multimap<int, int> m = {{1, 1}, {1, 2}, {2, 3}};
auto n = std::erase_if(m, [](const auto& p) { return p.second % 2 == 0; });   // 1; m is {1,1} {2,3}
```

### Deduction guides

```cpp
template<std::input_iterator InputIt, class Compare = std::less<Key>>   // Key and T from the iterator's pair
multimap(InputIt, InputIt, Compare = Compare()) -> multimap<Key, T, Compare>;
template<class Key, class T, class Compare = std::less<Key>>
multimap(std::initializer_list<std::pair<Key, T>>, Compare = Compare()) -> multimap<Key, T, Compare>;
```

```cpp
gc::multimap m = {std::pair{1, 2.0}, std::pair{1, 3.0}};     // multimap<int, double>
gc::multimap from_range(m.begin(), m.end());                 // multimap<int, double>
```

From an iterator pair or an initializer list, as for `std::multimap`; an initializer list of a map spells its pairs out (`std::pair{1, 2.0}`), since a braced pair alone names no type.

## Example

```cpp
#include "gc/gc.h"
#include <iostream>
#include <string>

struct Event {
    std::string what;
    gc::tracked_ptr<Event> cause;     // traced through the node that holds the Event
};

int main() {
    // A multimap on the stack: several events per day, in the order they came
    gc::multimap<int, gc::tracked_ptr<Event>> log;
    gc::tracked_ptr first = gc::make_tracked<Event>("boot");
    log.emplace(1, first);
    log.emplace(1, gc::make_tracked<Event>("login", first));
    log.emplace(2, gc::make_tracked<Event>("logout"));
    log.emplace(2, gc::make_tracked<Event>("shutdown"));
    log.emplace(1, gc::make_tracked<Event>("late entry"));     // goes after the other day-1 events

    // The run of one key, in insertion order
    auto [from, to] = log.equal_range(1);
    std::cout << "day 1:";
    for (auto it = from; it != to; ++it) {
        std::cout << ' ' << it->second->what;                  // boot login late entry
    }
    std::cout << '\n';

    // Erasing a whole key destroys its elements (the tracked_ptrs) at once;
    // the Events they pointed at are collected, except "boot", still held by `first`
    auto erased = log.erase(1);
    first = nullptr;                                           // now "boot" is unreachable too
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    gc::collector::force_collect(true);
    std::cout << erased << " erased, " << log.size() << " left, "
              << gc::collector::get_live_object_count() << " live objects\n";
    return erased == 3 && log.count(2) == 2 ? 0 : 1;
}
```

## See also

- [map](map.md) for unique keys, [multiset](multiset.md) for keys alone, [unordered_multimap](unordered_multimap.md) for a hash table
- [tracked_ptr](tracked_ptr.md), [make_tracked](make_tracked.md)
- [README: Containers](../README.md#containers), [README: The rules](../README.md#the-rules)
