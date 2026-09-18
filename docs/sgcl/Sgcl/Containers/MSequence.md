# MSequence

```cpp
#include "sgcl/Sgcl/Containers/MSequence.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    inline constexpr size_t NoIndex;          // the position that is no position
    template<class Derived>
    class MSequence;
}
```

The same class in the `sgcl` interface: [m_sequence](../../containers/m_sequence.md).

`MSequence<Derived>` is the mixin that gives a sequence the algorithms of `<algorithm>` as members: `Contains`, `IndexOf`, `Find`, `Sort`, `Reverse`, `Min`, `ForEach` and the rest, written once and shared by [List](List.md), [Array](Array.md), [Deque](Deque.md), [LinkedList](LinkedList.md) and [ForwardList](ForwardList.md), so that `v.Sort()` reads as `v.Add(x)` does and a program does not reach for `std::ranges::sort(v)` when it means the container. The `M` says what it is: a mixin, a static interface brought in by the curiously recurring template (`Derived` is the container, the one template argument; `begin` and `end` of `Derived`, the free functions, are all it uses, and the element type is never named: `Contains`, `IndexOf` and `Fill` take anything an element compares with or is assigned from, `Find`, `Min` and `Max` return what the iterator gives), with no virtual method and no object of its own. The mixin has no state, and its constructor and destructor are protected: it exists only as the base of the container that names itself as `Derived`. A `List<T>&` converts to an `MSequence<List<T>>&`, and such a reference can do exactly what the list can, nothing else and nothing less; there is no polymorphism in it (`MSequence<List<T>>` and `MSequence<Deque<T>>` are two types), and `auto& m = v;` is the way to write it. The `M` is the convention: a static interface; an `I` will name a polymorphic one, of virtual methods, where a module needs it.

A container of one's own gets the same set by deriving from `MSequence<Own>`: one line, nothing else; `begin(own)` and `end(own)` must exist, as they do for every sequence of the interface. A member the container declares itself (`Sort` on a linked list) hides the mixin's.

## Rules

- A member is the `std` algorithm over the elements: what the algorithm requires of the iterator, the member requires of the sequence (`Reverse` a bidirectional one, `Sort` a random-access one; `LinkedList` and `ForwardList` have their own `Reverse` and `Sort`, on the nodes, and do not expose these).
- `Min` and `Max` on an empty sequence are undefined, as `First()` is; nothing is checked.
- Thread safety is the container's: the members read or write the elements as the algorithms do.

## Members

```cpp
bool Contains(const auto& value) const;   // anything an element compares with
SizeType IndexOf(const auto& value) const;                // the first equal element's position, NoIndex when none
SizeType LastIndexOf(const auto& value) const;            // the last one's
template<class Pred> SizeType FindIndex(Pred pred) const;   // the first the predicate accepts
template<class Pred> T* Find(Pred pred) noexcept;      // that element, or null; and const
template<class Pred> bool Exists(Pred pred) const;     // some element satisfies pred
template<class Pred> bool All(Pred pred) const;        // every element does
template<class Pred> SizeType CountOf(Pred pred) const;
template<class F> void ForEach(F f);                   // and const
const T& Min() const;  template<class Compare> const T& Min(Compare cmp) const;
const T& Max() const;  template<class Compare> const T& Max(Compare cmp) const;
void Fill(const auto& value);
void Reverse() noexcept;
void Sort();  template<class Compare> void Sort(Compare cmp);
bool IsSorted() const;  template<class Compare> bool IsSorted(Compare cmp) const;
SizeType BinarySearch(const auto& value) const;   // on a sorted sequence: the value's position, NoIndex when it is not there; and with a comparator
auto LowerBound(const auto& value);  auto UpperBound(const auto& value);   // the first position not less than the value, the first greater; and const, and with a comparator
```

```cpp
List v = {5, 3, 9, 3};
assert(v.Contains(9) && v.IndexOf(3) == 1 && v.LastIndexOf(3) == 3 && v.IndexOf(7) == NoIndex);
v.Sort();                                        // 3 3 5 9
assert(v.IsSorted() && v.Min() == 3 && v.CountOf([](int x) { return x == 3; }) == 2);
assert(v.BinarySearch(5) == 2 && v.BinarySearch(4) == NoIndex);
assert(*v.LowerBound(4) == 5 && v.UpperBound(9) == end(v));
```

`BinarySearch`, `LowerBound` and `UpperBound` are the lookups of a sorted sequence, O(log n) comparisons (a linked list still advances n), what C#'s `List.BinarySearch` is: a sorted `List` with them is the flat dictionary, the lookups of a dictionary with the memory of a list, for keys built once and searched often. `LowerBound` is where a new value goes to keep the order: `v.Insert(v.LowerBound(x) - begin(v), x)`.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// A sequence of one's own with the algorithms mixed in: a ring of the
// last N values, a List under it; Contains, Max and Sort come from the
// mixin, begin and end are all it asks for.
template<class T, size_t N>
class Ring : public MSequence<Ring<T, N>> {
public:
    void Push(const T& value) {
        if (_values.Count() < N) {
            _values.Add(value);
        } else {
            _values[_next] = value;
        }
        _next = (_next + 1) % N;
    }

    friend auto begin(Ring& r) { return begin(r._values); }
    friend auto end(Ring& r) { return end(r._values); }
    friend auto begin(const Ring& r) { return begin(r._values); }
    friend auto end(const Ring& r) { return end(r._values); }

private:
    List<T> _values;
    size_t _next = 0;
};

int main() {
    Ring<int, 4> last;
    for (int x : {3, 9, 1, 7, 5}) {
        last.Push(x);                                // 5 9 1 7: the 3 overwritten
    }
    std::cout << last.Max() << (last.Contains(3) ? " with 3" : " without 3") << "\n";   // 9 without 3
    last.Sort();
    last.ForEach([](int x) { std::cout << x << " "; });   // 1 5 7 9
    std::cout << "\n";
    return last.Max() == 9 && !last.Contains(3) ? 0 : 1;
}
```

The output:

```
9 without 3
1 5 7 9 
```

## See also

- [List](List.md), [Array](Array.md), [Deque](Deque.md), [LinkedList](LinkedList.md), [ForwardList](ForwardList.md): the sequences that carry the mixin
- `tests/Sgcl/sgcl.cpp`: every member above, checked on every sequence.
