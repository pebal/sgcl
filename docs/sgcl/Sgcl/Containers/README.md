# Sgcl::Containers

The containers of the standard library with the names of a collection library, their nodes and buffers on the managed heap, and the observers built on them (`WeakDictionary`, `WeakHashSet`, `ExpiryQueue`): a container lives where a `Ptr` may live, its elements are destroyed exactly when `std` destroys them, and the collector reclaims the memory. `#include "sgcl/Sgcl/Containers/Containers.h"` brings the module in; it is a facade over [`sgcl/containers/`](../../containers/README.md), whose README is the guide of the module (what the classes are, the rules, what to reach for), the same under these names and depends on [`Core`](../Core/README.md) only. The index of the whole interface is [`docs/sgcl/Sgcl/`](../README.md).

## Sequences and associative containers

| page | header | `std` counterpart |
|---|---|---|
| [List](List.md) | `List.h` | `std::vector` |
| [Array](Array.md) | `Array.h` | `std::array`, and a buffer sized at creation |
| [Deque](Deque.md) | `Deque.h` | `std::deque` |
| [LinkedList](LinkedList.md) | `LinkedList.h` | `std::list` |
| [ForwardList](ForwardList.md) | `LinkedList.h` | `std::forward_list` |
| [Stack](Stack.md) | `Queue.h` | `std::stack` |
| [Queue, PriorityQueue](Queue.md) | `Queue.h` | `std::queue`, `std::priority_queue` |
| [SortedDictionary](SortedDictionary.md) | `SortedDictionary.h` | `std::map` |
| [SortedMultiDictionary](SortedMultiDictionary.md) | `SortedDictionary.h` | `std::multimap` |
| [SortedSet](SortedSet.md) | `SortedSet.h` | `std::set` |
| [SortedMultiSet](SortedMultiSet.md) | `SortedSet.h` | `std::multiset` |
| [Dictionary](Dictionary.md) | `Dictionary.h` | `std::unordered_map` |
| [MultiDictionary](MultiDictionary.md) | `Dictionary.h` | `std::unordered_multimap` |
| [HashSet](HashSet.md) | `HashSet.h` | `std::unordered_set` |
| [HashMultiSet](HashMultiSet.md) | `HashSet.h` | `std::unordered_multiset` |
| [OrderedDictionary](OrderedDictionary.md) | `OrderedDictionary.h` | a `Dictionary` iterated in the order of adding: Java's `LinkedHashMap`; `First`, `Last`, `MoveToLast`, `MoveToFirst`, `RemoveFirst` |
| [OrderedSet](OrderedSet.md) | `OrderedSet.h` | a `HashSet` iterated in the order of adding: Java's `LinkedHashSet` |

The algorithms of a sequence (`Contains`, `IndexOf`, `Find`, `Sort`, `Reverse`, `Min`, `ForEach`...) are members of every sequence, from one mixin:

| page | header | what it is |
|---|---|---|
| [MSequence](MSequence.md) | `MSequence.h` | the mixin (`M`): the algorithms as members of `List`, `Array`, `Deque`, `LinkedList`, `ForwardList`; static, no virtual method, no converting to it |

## Weak containers

| page | header | what it is |
|---|---|---|
| [WeakDictionary, WeakMultiDictionary](WeakDictionary.md) | `WeakDictionary.h` | values attached to objects the dictionary does not keep alive: keyed by the object, an entry dies with it |
| [WeakHashSet](WeakHashSet.md) | `WeakDictionary.h` | a set of objects it does not keep alive |
| [ExpiryQueue](ExpiryQueue.md) | `ExpiryQueue.h` | a callback for an object the collector found unreachable, with the object alive again for the call |
