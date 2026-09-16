# sgcl::multiset

```cpp
#include "sgcl/multiset.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class Key, class Compare = std::less<Key>>
    class multiset;
}
```

`sgcl::multiset<Key, Compare>` is `std::multiset` on a red-black tree whose nodes are managed objects: the same tree as [set](set.md), with equivalent keys allowed. The interface is the one of `std::multiset` (constructors, `insert`, `emplace`, `erase`, `extract`, `merge`, node handles, the lookups with transparent comparators, bidirectional iterators, `key_comp`/`value_comp`, `swap`, `==` and `<=>`, `std::erase_if`), and so is the behaviour: equivalent keys are adjacent and a new one goes after those already there, `erase(key)` removes all of them, `iterator` is `const_iterator`, an element is destroyed the moment it is erased.

What differs from `std` is where the memory lives. The multiset object holds one `tracked_ptr` (to a header node), a count and the comparator, so it lives where a `tracked_ptr` may live; the nodes are managed objects linked by tracked pointers and traced from the header, so elements that are or hold `tracked_ptr`s are traced and a cycle through a multiset is collected like any other. Nothing is freed by hand: an `erase` destroys the element and unlinks the node, the collector reclaims the node later. Iterators are one raw node pointer each, trivially copyable, storable anywhere, valid while their element is in the container. Lookups and iteration read raw pointers and pay no write barrier; insertions, erasures and rebalancing store tracked pointers and pay the barrier on each link they relink ([README: Containers](../README.md#containers)). The header is allocated on the first insertion: an empty multiset costs nothing.

## Rules

- A multiset holds a `tracked_ptr`, so it lives on a stack or inside a managed object: never in `new`/`malloc` memory, a `std` container, a global, a `thread_local` or a plain coroutine frame ([The rules](../README.md#the-rules), 1). The same holds for a node handle.
- The elements may be, or hold, tracked pointers: the nodes are managed objects, so those pointers are traced.
- An element is destroyed the moment it is erased, cleared, assigned over, or the multiset is destroyed, exactly as in `std`. The one exception is a multiset dying in a sweep, inside a managed object nobody refers to any more: its nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it, on a collector thread.
- An iterator, a reference or a pointer to an element is valid while the element is in the container, across insertions, erasures of other elements, `swap`, `merge` and a move of the container. An iterator to an erased element is invalid as in `std`.
- A `tracked_ptr` may point at an element (a node is a managed object); it keeps the node alive, not the element.
- Thread safety is that of `std::multiset`: concurrent readers, or one writer, with the program's own synchronization ([The rules](../README.md#the-rules), 6).

## Members

### Types

```cpp
using key_type = Key;
using value_type = Key;
using key_compare = Compare;
using value_compare = Compare;
using size_type = size_t;
using difference_type = ptrdiff_t;
using reference = value_type&;
using const_reference = const value_type&;
using pointer = value_type*;
using const_pointer = const value_type*;
using iterator = /* bidirectional, one raw node pointer, yields const Key& */;
using const_iterator = iterator;
using reverse_iterator = std::reverse_iterator<iterator>;
using const_reverse_iterator = std::reverse_iterator<const_iterator>;
using node_type = /* the node handle, below */;
static constexpr bool Multi = true;
```

There is no `insert_return_type`: every `insert` returns an iterator.

### Constructors

```cpp
multiset() noexcept(std::is_nothrow_default_constructible_v<Compare>);
explicit multiset(const Compare& comp);
template<std::input_iterator InputIt> multiset(InputIt first, InputIt last, const Compare& comp = Compare());
multiset(std::initializer_list<value_type> ilist, const Compare& comp = Compare());
multiset(const multiset& other);
multiset(multiset&& other) noexcept(std::is_nothrow_move_constructible_v<Compare>);
```

The default constructor allocates nothing. The range and list constructors insert in order with `end()` as the hint (sorted input costs one comparison per element), keeping every element. A copy has nodes of its own in the same order; a move takes the tree over and leaves `other` empty. A constructor or comparator that throws destroys the elements built so far.

```cpp
sgcl::multiset<int> rolls = {4, 2, 4, 6, 2};                          // 2 2 4 4 6
sgcl::multiset<int, std::greater<int>> desc(std::greater<int>{});
std::vector<int> src = {3, 1, 3};
sgcl::multiset<int> from_range(src.begin(), src.end());              // 1 3 3
sgcl::multiset<int> taken = std::move(rolls);                         // rolls is empty now
```

### Destructor

```cpp
~multiset();
```

Destroys the elements when the multiset dies on a stack or inside a managed object destroyed by hand; in a sweep it leaves the nodes to the same sweep, which destroys the elements. The node memory is reclaimed by the collector in both cases.

### operator=

```cpp
multiset& operator=(const multiset& other);
multiset& operator=(multiset&& other) noexcept(std::is_nothrow_move_assignable_v<Compare>);
multiset& operator=(std::initializer_list<value_type> ilist);
```

Copy assignment clears this container (destroying its elements at once), takes `other`'s comparator and inserts copies; move assignment clears and takes the tree over; the list form clears and inserts.

```cpp
sgcl::multiset<int> a = {1, 1}, b;
b = a;
b = {5};                     // the old elements die here
a = std::move(b);            // a is 5, b is empty
```

### key_comp, value_comp

```cpp
key_compare key_comp() const;
value_compare value_comp() const;
```

Copies of the comparator (the same type for both, as in `std::multiset`).

```cpp
sgcl::multiset<int> s = {1, 2};
bool less = s.key_comp()(*s.begin(), *s.rbegin());   // true
```

### Iterators

```cpp
iterator begin() const noexcept;                 const_iterator cbegin() const noexcept;
iterator end() const noexcept;                   const_iterator cend() const noexcept;
reverse_iterator rbegin() const noexcept;        const_reverse_iterator crbegin() const noexcept;
reverse_iterator rend() const noexcept;          const_reverse_iterator crend() const noexcept;
```

`iterator` and `const_iterator` are one type, yielding `const Key&`. `begin()` is the smallest key in O(1), `--end()` the largest; equivalent keys come in insertion order. Before the first insertion `begin()` and `end()` are both null iterators, equal to each other, neither of which may be dereferenced or moved; an `end()` taken then does not compare equal to `end()` after the first insertion. Iterators are raw node pointers and may be kept in unmanaged memory while their element is in the container.

```cpp
sgcl::multiset<std::string> s = {"b", "a", "a"};
std::string joined;
for (const auto& key : s) {             // a a b
    joined += key;
}
auto largest = std::prev(s.end());      // "b"
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
sgcl::multiset<std::string> s = {"a", "a"};
s.clear();                     // both strings are destroyed here
bool gone = s.empty();         // true
```

### insert

```cpp
iterator insert(const value_type& value);
iterator insert(value_type&& value);
iterator insert(const_iterator hint, const value_type& value);
iterator insert(const_iterator hint, value_type&& value);
template<std::input_iterator InputIt> void insert(InputIt first, InputIt last);
void insert(std::initializer_list<value_type> ilist);
iterator insert(node_type&& nh);
iterator insert(const_iterator hint, node_type&& nh);
```

Always inserts; a key already present gets the new element after its equivalents. The hinted forms are O(1) amortized when the element belongs right before `hint`; an append in sorted order at `end()` costs one comparison. The range and list forms insert one by one with `end()` as the hint. The node-handle forms link the node of `nh` without copying the element and leave `nh` empty; an empty handle inserts nothing and returns `end()`.

```cpp
sgcl::multiset<std::string> s;
auto it = s.insert("a");
s.insert("a");                                        // after the first "a"
s.insert(s.end(), "z");                               // an append: one comparison
s.insert({"b", "b"});
sgcl::multiset<std::string> other = {"q"};
s.insert(other.extract("q"));                         // relinked, no copy
auto count = s.count("a");                            // 2
```

### emplace, emplace_hint

```cpp
template<class... A> iterator emplace(A&&... a);
template<class... A> iterator emplace_hint(const_iterator hint, A&&... a);
```

Builds the key from `a...` in a new node and links it after its equivalents (or where `hint` says, when that is right). A comparator that throws destroys the new element and leaves the container as it was.

```cpp
sgcl::multiset<std::string> s;
s.emplace(3, 'x');                          // "xxx"
s.emplace(3, 'x');                          // a second "xxx"
s.emplace_hint(s.end(), "zzz");
```

### erase

```cpp
iterator erase(const_iterator pos);
iterator erase(const_iterator first, const_iterator last);
size_type erase(const key_type& key);
```

Destroys the element at once, unlinks the node (the collector reclaims it later) and returns the iterator after it. The key form erases every element with an equivalent key and returns how many. Erasing `[begin(), end())` is a `clear()`. There is no transparent `erase`.

```cpp
sgcl::multiset<int> s = {1, 1, 2, 3};
auto erased = s.erase(1);                          // 2
s.erase(s.find(2));                                // one element: 3 is left
```

### swap

```cpp
void swap(multiset& other) noexcept(std::is_nothrow_swappable_v<Compare>);
friend void swap(multiset& lhs, multiset& rhs) noexcept(noexcept(lhs.swap(rhs)));   // free function in namespace sgcl
```

Exchanges the trees, counts and comparators; no element is touched, and every iterator keeps pointing at its element, now in the other container.

```cpp
sgcl::multiset<int> a = {1}, b = {2};
auto it = a.begin();
swap(a, b);                       // it still points at 1, which is in b now
bool moved = it == b.begin();     // true
```

### extract

```cpp
node_type extract(const_iterator pos);
node_type extract(const key_type& key);
```

Unlinks the node and hands it over in a node handle, the element untouched; the handle destroys the element if it dies unused. The key form extracts the first element with an equivalent key, or returns an empty handle. See [node_type](#node_type-the-node-handle).

```cpp
sgcl::multiset<std::string> s = {"a", "a"};
auto nh = s.extract("a");         // s holds one "a"
nh.value() = "b";
s.insert(std::move(nh));          // "a" "b"
```

### merge

```cpp
template<class Traits2> void merge(detail::RbTree<Traits2>& source);    // any sgcl::set or multiset<Key, C2>
template<class Traits2> void merge(detail::RbTree<Traits2>&& source);
```

Relinks every node of `source` into this multiset (a multi tree takes them all), each after its equivalents, and leaves `source` empty. No element is copied or destroyed; iterators follow their nodes. `source` may be a `sgcl::set` or `sgcl::multiset` with the same `Key` and any comparator.

```cpp
sgcl::multiset<int> a = {1, 3};
sgcl::set<int> b = {2, 3};
a.merge(b);                       // a: 1 2 3 3;  b is empty
```

### count, find, contains

```cpp
size_type count(const key_type& key) const;
iterator find(const key_type& key) const;
bool contains(const key_type& key) const;
template<class K> size_type count(const K& key) const;        // when Compare::is_transparent
template<class K> iterator find(const K& key) const;          //   "
template<class K> bool contains(const K& key) const;          //   "
```

`find` returns the first element with an equivalent key, `count` how many there are (O(log n + count)). The `K` overloads exist for a transparent comparator.

```cpp
sgcl::multiset<std::string, std::less<>> s = {"a", "a"};
std::string_view key = "a";
auto n = s.count(key);             // 2, no std::string built
```

### equal_range, lower_bound, upper_bound

```cpp
std::pair<iterator, iterator> equal_range(const key_type& key) const;
iterator lower_bound(const key_type& key) const;
iterator upper_bound(const key_type& key) const;
template<class K> std::pair<iterator, iterator> equal_range(const K& key) const;   // when Compare::is_transparent
template<class K> iterator lower_bound(const K& key) const;                        //   "
template<class K> iterator upper_bound(const K& key) const;                        //   "
```

`equal_range` is the run of elements with an equivalent key; `lower_bound` its start, `upper_bound` its end.

```cpp
sgcl::multiset<int> s = {1, 2, 2, 3};
auto [from, to] = s.equal_range(2);
auto twos = std::distance(from, to);      // 2
```

### Comparisons

```cpp
friend bool operator==(const multiset& lhs, const multiset& rhs);
friend auto operator<=>(const multiset& lhs, const multiset& rhs);
```

Element-wise in order, as for `std::multiset`: `==` compares sizes first; `<=>` is lexicographical with the synthesized three-way comparison, so `!=`, `<`, `<=`, `>` and `>=` follow.

```cpp
sgcl::multiset<int> a = {1, 1}, b = {1, 2};
bool less = a < b;                                // true
auto ord = a <=> b;                               // std::strong_ordering::less
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
    using value_type = Key;
    node_type() noexcept;
    node_type(node_type&&) noexcept;
    node_type& operator=(node_type&&) noexcept;
    ~node_type();
    [[nodiscard]] bool empty() const noexcept;
    explicit operator bool() const noexcept;
    value_type& value() const noexcept;    // writable: the node is out of any container
};
```

Owns one unlinked node: movable, not copyable; the element is destroyed when the handle dies without having been inserted. The handle holds the node through a `tracked_ptr`, so it lives on a stack or inside a managed object. It is the same handle type as `sgcl::set<Key>::node_type`. There is no `swap` member.

### std::erase_if

```cpp
namespace sgcl {
    template<class Key, class Compare, class Pred>
    typename multiset<Key, Compare>::size_type erase_if(multiset<Key, Compare>& c, Pred pred);
}
namespace std { using sgcl::erase_if; }
```

Erases every element for which `pred(*it)` is true and returns how many.

```cpp
sgcl::multiset<int> s = {1, 2, 2, 3};
auto n = std::erase_if(s, [](int x) { return x == 2; });   // 2; s is 1 3
```

### Deduction guides

```cpp
template<std::input_iterator InputIt, class Compare = std::less<Key>>   // Key from the iterator
multiset(InputIt, InputIt, Compare = Compare()) -> multiset<Key, Compare>;
template<class Key, class Compare = std::less<Key>>
multiset(std::initializer_list<Key>, Compare = Compare()) -> multiset<Key, Compare>;
```

```cpp
sgcl::multiset s = {3, 1, 1};                           // multiset<int>
std::vector<int> src = {5, 4, 4};
sgcl::multiset from_range(src.begin(), src.end());      // multiset<int>
```

From an iterator pair or an initializer list, as for `std::multiset`; an initializer list of a map spells its pairs out (`std::pair{1, 2.0}`), since a braced pair alone names no type.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>
#include <string>

struct Task {
    std::string name;
    int priority;
    sgcl::tracked_ptr<Task> blocked_by;
};

// Tasks ordered by priority, several per level: a multiset of pointers
// with a comparator that looks through them
struct ByPriority {
    bool operator()(const sgcl::tracked_ptr<Task>& l, const sgcl::tracked_ptr<Task>& r) const {
        return l->priority < r->priority;
    }
};

int main() {
    sgcl::multiset<sgcl::tracked_ptr<Task>, ByPriority> queue;
    sgcl::tracked_ptr build = sgcl::make_tracked<Task>("build", 1);
    queue.insert(build);
    queue.insert(sgcl::make_tracked<Task>("test", 2, build));
    queue.insert(sgcl::make_tracked<Task>("lint", 2));               // after "test": equal keys keep their order
    queue.insert(sgcl::make_tracked<Task>("deploy", 3));

    std::cout << "order:";
    for (const auto& task : queue) {
        std::cout << ' ' << task->name;                             // build test lint deploy
    }
    std::cout << '\n';

    // Everything at priority 2 goes: the two tracked_ptrs are destroyed now,
    // "test" and "lint" are collected, "build" stays through `build`
    sgcl::tracked_ptr probe = sgcl::make_tracked<Task>("", 2);          // a key to look up with
    auto erased = queue.erase(probe);
    build = nullptr;
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    sgcl::collector::force_collect(true);
    std::cout << erased << " erased, " << queue.size() << " left, "
              << sgcl::collector::get_live_object_count() << " live objects\n";
    return erased == 2 && queue.size() == 2 ? 0 : 1;
}
```

## See also

- [set](set.md) for unique keys, [multimap](multimap.md) for key-value pairs, [unordered_multiset](unordered_multiset.md) for a hash table
- [tracked_ptr](tracked_ptr.md), [make_tracked](make_tracked.md)
- [README: Containers](../README.md#containers), [README: The rules](../README.md#the-rules)
