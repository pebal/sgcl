# sgcl::unordered_multiset

```cpp
#include "sgcl/containers/unordered_multiset.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class unordered_multiset;
}
```

The same class in the `Sgcl` interface: [HashMultiSet](../Sgcl/Containers/HashMultiSet.md).

`sgcl::unordered_multiset<Key, Hash, KeyEqual>` is `std::unordered_multiset` over managed nodes: the same hash table as [unordered_set](unordered_set.md), with equivalent keys allowed. The interface is the one of `std::unordered_multiset` (constructors, `insert`, `emplace`, `erase`, `extract`, `merge`, node handles, the lookups with transparent hash and equality, forward iterators, the bucket interface, `load_factor`/`max_load_factor`/`rehash`/`reserve`, `hash_function`/`key_eq`, `swap`, `==`, deduction guides, `std::erase_if`), and so is the behaviour: equal elements are adjacent in the iteration order and in their bucket, `erase(key)` removes all of them, `count` counts them, an element is destroyed the moment it is erased. Within a run of equal elements a new one goes in front of those already there.

What differs from `std` is where the memory lives. The container holds two `tracked_ptr`s (the bucket array and a sentinel node), the counts, the hasher and the equality, so it lives where a `tracked_ptr` may live; the elements are nodes on the managed heap, forming one chain linked by tracked pointers and traced from the sentinel, so elements that are or hold `tracked_ptr`s are traced and a cycle through the container is collected like any other. Nothing is freed by hand: an `erase` destroys the element and unlinks the node, the collector reclaims the node later. The hash of each key is cached in its node. The bucket count is 0 or a power of two, and the table grows when the size reaches `bucket_count() * max_load_factor()`, doubling at least, to eight buckets at the least. Iterators are one raw node pointer each, trivially copyable, storable anywhere, valid across rehashes and until their element is erased. As in `std`, `iterator` and `const_iterator` are one type, yielding `const Key&`: a key is never modified in place; `extract` it and insert it back. Lookups and iteration pay no write barrier; insertions, erasures and rehashes store tracked pointers and pay the barrier on each link they relink ([README: Containers](README.md#containers)).

## Rules

- An `unordered_multiset` holds tracked pointers, so it lives on a stack or inside a managed object: never in `new`/`malloc` memory, a `std` container, a global, a `thread_local` or a plain coroutine frame ([The rules](../core/README.md#the-rules), 1). The same holds for a node handle.
- The elements may be, or hold, tracked pointers: the nodes are managed objects, so those pointers are traced.
- An element is destroyed the moment it is erased, cleared, assigned over, or the container is destroyed, exactly as in `std`. The one exception is a container dying in a sweep, inside a managed object nobody refers to any more: its nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it, on a collector thread.
- An iterator, a reference or a pointer to an element is valid while the element is in the container, across insertions, rehashes, erasures of other elements, `swap`, `merge` and a move of the container. An iterator to an erased element is invalid as in `std`.
- A key cannot be modified through an iterator (they yield `const Key&`); `extract` it, change `value()` of the handle and insert it back.
- A `tracked_ptr` may point at an element (a node is a managed object); it keeps the node alive, not the element.
- Thread safety is that of `std::unordered_multiset`: concurrent readers, or one writer, with the program's own synchronization ([The rules](../core/README.md#the-rules), 6).

## Members

### Types

```cpp
using key_type = Key;
using value_type = Key;
using hasher = Hash;
using key_equal = KeyEqual;
using size_type = size_t;
using difference_type = ptrdiff_t;
using reference = value_type&;
using const_reference = const value_type&;
using pointer = value_type*;
using const_pointer = const value_type*;
using const_iterator = /* forward, one raw node pointer, yields const Key& */;
using iterator = const_iterator;
using const_local_iterator = /* forward, stops at the end of its bucket */;
using local_iterator = const_local_iterator;
using node_type = /* the node handle, below */;
struct insert_return_type { iterator position; bool inserted; node_type node; };   // unused: every insert returns an iterator
static constexpr bool unique = false;
```

`iterator` converts to `const_iterator`, `local_iterator` to `const_local_iterator`, not back.

### Constructors

```cpp
unordered_multiset();
explicit unordered_multiset(size_type bucket_count, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual());
template<std::input_iterator InputIt>
unordered_multiset(InputIt first, InputIt last, size_type bucket_count = 0, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual());
unordered_multiset(std::initializer_list<value_type> ilist, size_type bucket_count = 0, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual());
unordered_multiset(const unordered_multiset& other);
unordered_multiset(unordered_multiset&& other);
```

The default constructor allocates nothing (`bucket_count() == 0`). A bucket count is rounded up to a power of two. The range constructor, given a forward range, sizes the table for the distance first; every element is kept. A copy reproduces `other`'s bucket count, order and `max_load_factor`; a move takes the table over and leaves `other` empty. A constructor or hasher that throws destroys the elements built so far.

```cpp
sgcl::unordered_multiset rolls = {4, 2, 4, 6, 2};                    // five elements
sgcl::unordered_multiset<int> sized(100);                                  // 128 buckets
sgcl::vector src = {3, 1, 3};
sgcl::unordered_multiset from_range(src.begin(), src.end());               // deduced: unordered_multiset<int>
sgcl::unordered_multiset<int> taken = std::move(rolls);                    // rolls is empty now
```

### Destructor

```cpp
~unordered_multiset();
```

Destroys the elements when the container dies on a stack or inside a managed object destroyed by hand; in a sweep it leaves the nodes to the same sweep, which destroys the elements. The nodes, the bucket array and the sentinel are reclaimed by the collector in both cases.

### operator=

```cpp
unordered_multiset& operator=(const unordered_multiset& other);
unordered_multiset& operator=(unordered_multiset&& other);
unordered_multiset& operator=(std::initializer_list<value_type> ilist);
```

Copy assignment builds a copy of `other` and swaps it in; move assignment clears this container (destroying its elements at once) and takes the table over; the list form builds a new table with this container's hasher, equality and `max_load_factor` and swaps it in.

```cpp
sgcl::unordered_multiset<int> a = {1, 1}, b;
b = a;
b = {5};                     // the old elements die here
a = std::move(b);            // a is {5}, b is empty
```

### Iterators

```cpp
iterator begin() noexcept;                const_iterator begin() const noexcept;    const_iterator cbegin() const noexcept;
iterator end() noexcept;                  const_iterator end() const noexcept;      const_iterator cend() const noexcept;
```

Forward iterators over one chain of nodes; `end()` is a null iterator. Equal elements are adjacent. An iterator is a raw node pointer: copying and advancing it costs a load, and it may be kept in unmanaged memory while its element is in the container.

```cpp
sgcl::unordered_multiset<sgcl::string> s = {"a", "b", "a"};
sgcl::string joined;
for (const auto& key : s) {
    joined += key;                         // "aab" or "baa"
}
```

### empty, size, max_size

```cpp
bool empty() const noexcept;
size_type size() const noexcept;
size_type max_size() const noexcept;
```

`size()` is a stored count, O(1).

### clear

```cpp
void clear() noexcept;
```

Destroys every element at once and unlinks every node; the bucket array, the hasher, the equality and `max_load_factor` stay.

```cpp
sgcl::unordered_multiset<sgcl::string> s = {"a", "a"};
s.clear();                     // both strings are destroyed here
bool gone = s.empty();         // true
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

Always inserts, and returns the new element; an element already present gets the new one in front of its equivalents. The `P&&` forms build the element through `emplace`. The hint is ignored. The table grows before the node is linked when the size has reached the threshold. The node-handle forms link the node of `nh` without copying the element and leave `nh` empty; an empty handle inserts nothing and returns `end()`.

```cpp
sgcl::unordered_multiset<sgcl::string> s;
s.insert("a");
auto it = s.insert("a");                              // in front of the first "a"
s.insert(s.end(), "z");                               // the hint is ignored
s.insert({"b", "b"});
sgcl::unordered_multiset<sgcl::string> other = {"q"};
s.insert(other.extract("q"));                         // relinked, no copy
bool front = s.find("a") == it;                       // true
```

### emplace, emplace_hint

```cpp
template<class... A> iterator emplace(A&&... a);
template<class... A> iterator emplace_hint(const_iterator hint, A&&... a);
```

Builds the key from `a...` in a new node and links it in front of its equivalents. The hint is ignored. A hasher or equality that throws destroys the new element and leaves the container as it was.

```cpp
sgcl::unordered_multiset<sgcl::string> s;
s.emplace(3, 'x');                          // "xxx"
s.emplace(3, 'x');                          // a second "xxx"
s.emplace_hint(s.end(), "zzz");
```

### erase

```cpp
iterator erase(iterator pos);
iterator erase(const_iterator pos);
iterator erase(const_iterator first, const_iterator last);
size_type erase(const key_type& key);
template<class K> size_type erase(K&& key);   // when Hash and KeyEqual are transparent, and K is not an iterator
```

Destroys the element at once, unlinks the node (the collector reclaims it later) and returns the iterator after it. The key forms erase every equal element and return how many.

```cpp
sgcl::unordered_multiset s = {1, 1, 2, 3};
auto erased = s.erase(1);                          // 2
s.erase(s.find(2));                                // one element: 3 is left
```

### swap

```cpp
void swap(unordered_multiset& other) noexcept(std::is_nothrow_swappable_v<Hash> && std::is_nothrow_swappable_v<KeyEqual>);
friend void swap(unordered_multiset& lhs, unordered_multiset& rhs) noexcept(noexcept(lhs.swap(rhs)));
```

Exchanges the tables, counts, load factors, hashers and equalities; no element is touched, and every iterator keeps pointing at its element, now in the other container.

```cpp
sgcl::unordered_multiset<int> a = {1}, b = {2};
auto it = a.begin();
swap(a, b);                       // it still points at 1, which is in b now
bool moved = it == b.find(1);     // true
```

### extract

```cpp
node_type extract(const_iterator pos);
node_type extract(const key_type& key);
template<class K> node_type extract(K&& key);   // when Hash and KeyEqual are transparent, and K is not an iterator
```

Unlinks the node and hands it over in a node handle, the element untouched; the handle destroys the element if it dies unused. The key forms extract the first equal element, or return an empty handle. This is the way to change a key. See [node_type](#node_type-the-node-handle).

```cpp
sgcl::unordered_multiset<sgcl::string> s = {"a", "a"};
auto nh = s.extract("a");         // s holds one "a"
nh.value() = "b";
s.insert(std::move(nh));          // "a" and "b"
```

### merge

```cpp
template<class Traits2> void merge(detail::HashTable<Traits2>& source);    // any sgcl::unordered_set or unordered_multiset<Key, H2, E2>
template<class Traits2> void merge(detail::HashTable<Traits2>&& source);
```

Relinks every node of `source` into this container (a multi table takes them all), rehashing with this container's hasher, and leaves `source` empty. No element is copied or destroyed; iterators follow their nodes. `source` may be a `sgcl::unordered_set` or `sgcl::unordered_multiset` with the same `Key` and any hasher and equality.

```cpp
sgcl::unordered_multiset a = {1, 3};
sgcl::unordered_set b = {2, 3};
a.merge(b);                       // a: 1 2 3 3;  b is empty
```

### count, find, contains, equal_range

```cpp
size_type count(const key_type& key) const;
iterator find(const key_type& key);
const_iterator find(const key_type& key) const;
bool contains(const key_type& key) const;
std::pair<iterator, iterator> equal_range(const key_type& key);
std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const;
template<class K> size_type count(const K& key) const;                     // when Hash::is_transparent and KeyEqual::is_transparent
template<class K> iterator find(const K& key);                              //   "
template<class K> const_iterator find(const K& key) const;                  //   "
template<class K> bool contains(const K& key) const;                        //   "
template<class K> std::pair<iterator, iterator> equal_range(const K& key);  //   "  (and the const form)
```

`find` returns the first element of the key's run, `equal_range` the run, `count` its length (O(1 + count) on average). The `K` overloads exist when both `Hash` and `KeyEqual` declare `is_transparent`.

```cpp
sgcl::unordered_multiset<sgcl::string> s = {"a", "a"};   // std::hash and std::equal_to of a string are transparent
auto n = s.count("a");             // 2, no sgcl::string built for the literal
sgcl::string text = "a b";
auto m = s.count(text.view(0, 1)); // 2: a view of another string, nothing built either
```

### Bucket interface

```cpp
size_type bucket_count() const noexcept;
size_type max_bucket_count() const noexcept;
size_type bucket_size(size_type n) const;
size_type bucket(const key_type& key) const;
template<class K> size_type bucket(const K& key) const;      // when Hash and KeyEqual are transparent
local_iterator begin(size_type n);              const_local_iterator begin(size_type n) const;   const_local_iterator cbegin(size_type n) const;
local_iterator end(size_type n);                const_local_iterator end(size_type n) const;     const_local_iterator cend(size_type n) const;
```

As in `std`. `bucket_count()` is 0 or a power of two; `bucket(key)` is the hash masked by `bucket_count() - 1` (0 while there are no buckets). A local iterator walks the nodes of one bucket, equal elements adjacent, and stops at its end; for an `n` beyond `bucket_count()` the range is empty.

```cpp
sgcl::unordered_multiset s = {1, 1, 2};
size_t n = s.bucket(1);
size_t in_bucket = 0;
for (auto it = s.begin(n); it != s.end(n); ++it) {
    ++in_bucket;
}
bool same = in_bucket == s.bucket_size(n);        // true, and at least 2
```

### Hash policy

```cpp
float load_factor() const noexcept;
float max_load_factor() const noexcept;
void max_load_factor(float z);
void rehash(size_type count);
void reserve(size_type count);
```

`load_factor()` is `size() / bucket_count()` (0 with no buckets); `max_load_factor()` defaults to 1.0. `max_load_factor(z)` takes effect on the next insertion (a value that is not positive, or not a number, is ignored). `rehash(count)` makes the bucket count the smallest power of two not below `count` and not below `size() / max_load_factor()`; `reserve(count)` is `rehash` for `count` elements. A rehash relinks the nodes in chain order, so runs of equal elements stay together and in order, hashes nothing and invalidates no iterator.

```cpp
sgcl::unordered_multiset<int> s;
s.reserve(1000);                                  // 1024 buckets
for (int i : sgcl::range(1000)) {
    s.insert(i % 10);                             // ten runs of a hundred
}
bool fits = s.load_factor() <= s.max_load_factor();   // true
```

### hash_function, key_eq

```cpp
hasher hash_function() const;
key_equal key_eq() const;
```

Copies of the hasher and the equality.

### Comparisons

```cpp
friend bool operator==(const unordered_multiset& lhs, const unordered_multiset& rhs);
```

Equal sizes and, for every run of equal elements in `lhs`, a run of the same length in `rhs` that is a permutation of it, whatever the bucket counts and orders. `!=` follows; there is no ordering.

```cpp
sgcl::unordered_multiset<int> a = {1, 1, 2}, b = {2, 1, 1};
bool same = a == b;                               // true
```

### node_type (the node handle)

```cpp
class node_type {
public:
    using key_type = Key;
    using value_type = Key;
    node_type() noexcept;
    node_type(node_type&&) noexcept;
    node_type& operator=(node_type&&);
    ~node_type();
    bool empty() const noexcept;
    explicit operator bool() const noexcept;
    value_type& value() const;             // writable: the node is out of any container
    void swap(node_type& other) noexcept;
    friend void swap(node_type& lhs, node_type& rhs) noexcept;
};
```

Owns one unlinked node: movable, not copyable; the element is destroyed when the handle dies without having been inserted. The handle holds the node through a `tracked_ptr`, so it lives on a stack or inside a managed object. It is the same handle type as `sgcl::unordered_set<Key>::node_type`.

### erase_if_impl, std::erase_if

```cpp
template<class Pred> size_type erase_if_impl(Pred& pred);   // member: what the free function calls

namespace sgcl {
    template<class Key, class Hash, class KeyEqual, class Pred>
    size_t erase_if(unordered_multiset<Key, Hash, KeyEqual>& c, Pred pred);
}
namespace std { using sgcl::erase_if; }
```

Erases every element for which `pred(*it)` is true and returns how many.

```cpp
sgcl::unordered_multiset s = {1, 2, 2, 3};
auto n = std::erase_if(s, [](int x) { return x == 2; });   // 2; s holds 1 and 3
```

### Deduction guides

```cpp
template<std::input_iterator InputIt, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>   // Key from the iterator
unordered_multiset(InputIt, InputIt, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual()) -> unordered_multiset<Key, Hash, KeyEqual>;
template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
unordered_multiset(std::initializer_list<Key>, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual()) -> unordered_multiset<Key, Hash, KeyEqual>;
```

```cpp
sgcl::vector<sgcl::string> src = {"a", "a"};
sgcl::unordered_multiset from_range(src.begin(), src.end());      // unordered_multiset<sgcl::string>
sgcl::unordered_multiset from_list = {1, 1, 2};                   // unordered_multiset<int>
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

struct Sample {
    sgcl::string source;
    sgcl::tracked_ptr<Sample> previous;     // traced through the node that holds the Sample
};

int main() {
    // A bag of readings keyed by value: several samples may read the same
    sgcl::unordered_multiset<int> readings;
    for (int r : {3, 7, 3, 3, 9, 7}) {
        readings.insert(r);
    }
    std::cout << "3 read " << readings.count(3) << " times, 7 read " << readings.count(7) << " times\n";

    // A bag of traced pointers inside a managed object: the samples live as
    // long as the bag's owner does
    struct Owner {
        sgcl::unordered_multiset<sgcl::tracked_ptr<Sample>> bag;
    };
    sgcl::tracked_ptr owner = sgcl::make_tracked<Owner>();
    sgcl::tracked_ptr first = sgcl::make_tracked<Sample>("a");
    owner->bag.insert(first);
    owner->bag.insert(first);                                   // the same pointer twice: a multiset allows it
    owner->bag.insert(sgcl::make_tracked<Sample>("b", first));
    auto duplicates = owner->bag.count(first);                  // 2

    // Erasing every copy of the pointer destroys those elements; the Sample
    // itself stays while `first` or "b" refers to it
    owner->bag.erase(first);
    first = nullptr;
    owner = nullptr;                                            // the bag, "b" and then "a" are garbage
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    sgcl::collector::force_collect(true);
    std::cout << sgcl::collector::get_live_object_count() << " live objects\n";     // the nodes, buckets and sentinel of `readings`
    return readings.count(3) == 3 && duplicates == 2 ? 0 : 1;
}
```

The output:

```
3 read 3 times, 7 read 2 times
8 live objects
```

## See also

- [unordered_set](unordered_set.md) for unique keys, [unordered_multimap](unordered_multimap.md) for key-value pairs, [multiset](multiset.md) for an ordered tree
- [tracked_ptr](../core/tracked_ptr.md), [make_tracked](../core/make_tracked.md)
- [README: Containers](README.md#containers), [README: The rules](../core/README.md#the-rules)
