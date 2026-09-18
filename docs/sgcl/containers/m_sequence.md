# sgcl::m_sequence

```cpp
#include "sgcl/containers/m_sequence.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    inline constexpr size_t npos;             // the position that is no position
    template<class Derived>
    class m_sequence;
}
```

The same class in the `Sgcl` interface: [MSequence](../Sgcl/Containers/MSequence.md).

`m_sequence<Derived>` is the mixin that gives a sequence the algorithms of `<algorithm>` as members: `contains`, `index_of`, `find`, `sort`, `reverse`, `min`, `for_each` and the rest, written once and shared by [vector](vector.md), [array](array.md), [deque](deque.md), [list](list.md) and [forward_list](forward_list.md), so that `v.sort()` reads as `v.push_back(x)` does and a program does not reach for `std::ranges::sort(v)` when it means the container. The `m_` says what it is: a mixin, a static interface brought in by the curiously recurring template (`Derived` is the container, the one template argument; `begin()` and `end()` of `Derived` are all it uses, and the element type is never named: `contains`, `index_of` and `fill` take anything an element compares with or is assigned from, `find`, `min` and `max` return what the iterator gives), with no virtual method and no object of its own. The mixin has no state, and its constructor and destructor are protected: it exists only as the base of the container that names itself as `Derived`. A `vector<T>&` converts to an `m_sequence<vector<T>>&`, and such a reference can do exactly what the vector can, nothing else and nothing less; there is no polymorphism in it (`m_sequence<vector<T>>` and `m_sequence<deque<T>>` are two types), and `auto& m = v;` is the way to write it. The `m_` is the convention: a static interface; an `i_` will name a polymorphic one, of virtual methods, where a module needs it.

A container of one's own gets the same set by deriving from `m_sequence<Own>`: one line, nothing else; `begin()` and `end()` must exist. A member the container declares itself (`sort` on a list) hides the mixin's. `array<T, N>` is an aggregate and cannot carry a base: it forwards to the same algorithms (`detail::Sequence`) by hand, `constexpr`.

## Rules

- A member is the `std` algorithm over `begin()` to `end()`: what the algorithm requires of the iterator, the member requires of the sequence (`reverse` a bidirectional one, `sort` a random-access one; `list` and `forward_list` have their own `reverse` and `sort`, on the nodes, and do not expose these).
- `min` and `max` on an empty sequence are undefined, as `front()` is; nothing is checked.
- Thread safety is the container's: the members read or write the elements as the algorithms do.

## Members

```cpp
bool contains(const auto& value) const;    // anything an element compares with
size_t index_of(const auto& value) const;                 // the first equal element's position, npos when none
size_t last_index_of(const auto& value) const;            // the last one's
template<class Pred> size_t find_index(Pred pred) const;   // the first the predicate accepts
template<class Pred> T* find(Pred pred) noexcept;      // that element, or null; and const
template<class Pred> bool exists(Pred pred) const;     // some element satisfies pred
template<class Pred> bool all(Pred pred) const;        // every element does
template<class Pred> size_t count_of(Pred pred) const;
template<class F> void for_each(F f);                  // and const
const T& min() const;  template<class Compare> const T& min(Compare cmp) const;
const T& max() const;  template<class Compare> const T& max(Compare cmp) const;
void fill(const auto& value);
void reverse() noexcept;
void sort();  template<class Compare> void sort(Compare cmp);
bool is_sorted() const;  template<class Compare> bool is_sorted(Compare cmp) const;
bool binary_search(const auto& value) const;  size_t sorted_index_of(const auto& value) const;   // on a sorted sequence: whether the value is there, its position (npos when not); and with a comparator
auto lower_bound(const auto& value);  auto upper_bound(const auto& value);   // the first position not less than the value, the first greater; and const, and with a comparator
```

```cpp
sgcl::vector v = {5, 3, 9, 3};
assert(v.contains(9) && v.index_of(3) == 1 && v.last_index_of(3) == 3 && v.index_of(7) == sgcl::npos);
v.sort();                                        // 3 3 5 9
assert(v.is_sorted() && v.min() == 3 && v.count_of([](int x) { return x == 3; }) == 2);
assert(v.binary_search(5) && v.sorted_index_of(9) == 3 && v.sorted_index_of(4) == sgcl::npos);
assert(*v.lower_bound(4) == 5 && v.upper_bound(9) == v.end());
```

`binary_search`, `sorted_index_of`, `lower_bound` and `upper_bound` are the lookups of a sorted sequence, O(log n) comparisons (a linked list still advances n): a sorted `vector` with them is the flat map of this library, the lookups of a `map` with the memory of a `vector`, for a set of keys built once and searched often. `lower_bound` is where a new value goes to keep the order: `v.insert(v.lower_bound(x), x)`.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

// A sequence of one's own with the algorithms mixed in: a ring of the
// last N values, a managed buffer under it; contains, max and sort come
// from the mixin, begin and end are all it asks for.
template<class T, size_t N>
class ring : public sgcl::m_sequence<ring<T, N>> {
public:
    void push(const T& value) {
        if (_values.size() < N) {
            _values.push_back(value);
        } else {
            _values[_next] = value;
        }
        _next = (_next + 1) % N;
    }

    auto begin() { return _values.begin(); }
    auto end() { return _values.end(); }
    auto begin() const { return _values.begin(); }
    auto end() const { return _values.end(); }

private:
    sgcl::vector<T> _values;
    size_t _next = 0;
};

int main() {
    ring<int, 4> last;
    for (int x : {3, 9, 1, 7, 5}) {
        last.push(x);                                // 5 9 1 7: the 3 overwritten
    }
    std::cout << last.max() << (last.contains(3) ? " with 3" : " without 3") << "\n";   // 9 without 3
    last.sort();
    last.for_each([](int x) { std::cout << x << " "; });   // 1 5 7 9
    std::cout << "\n";
    return last.max() == 9 && !last.contains(3) ? 0 : 1;
}
```

The output:

```
9 without 3
1 5 7 9 
```

## See also

- [vector](vector.md), [array](array.md), [deque](deque.md), [list](list.md), [forward_list](forward_list.md): the sequences that carry the mixin
- `tests/containers/m_sequence.cpp`: every member above, checked on every sequence.
