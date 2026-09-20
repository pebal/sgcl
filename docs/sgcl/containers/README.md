# sgcl::containers

The containers of the standard library with their nodes and buffers on the managed heap, the observers built on them (`weak_map`, `weak_set`, `expiry_queue`), and the immutable containers of `sgcl::im` ([im/](im/README.md)): a container lives where a `tracked_ptr` may live, its elements are destroyed exactly when `std` destroys them, and the collector reclaims the memory. `#include "sgcl/containers/containers.h"` brings the module in; it depends on [`core`](../core/README.md) only.; the index of the whole interface is [`docs/sgcl/`](../README.md); the containers against `std`, in numbers, are on [benchmarks](benchmarks.md).

## Containers
`vector`, `array`, `deque`, `list`, `forward_list`, `map`, `set`, `multimap`, `multiset`, `unordered_map`, `unordered_set`, `unordered_multimap`, `unordered_multiset` and the adapters `stack`, `queue`, `priority_queue`, in `sgcl::` and in `sgcl::`, follow the interfaces of their `std` namesakes, including iterator categories (`std::ranges` algorithms work on them), transparent lookup, node handles, `std::erase`/`std::erase_if`, and three-way comparison. They differ from the standard containers in where their memory lives and when elements die:

- A container holds its buffer or its root node by a `tracked_ptr`, so it lives where one may: on a stack or inside a managed object, never in `new`/`malloc` memory or in a standard container ([The pointer and its places](../core/README.md#the-pointer-and-its-places)). Iterators are plain pointers, valid exactly when their `std` counterparts are, and may live anywhere: the container roots every element it holds, and a raw pointer in a stack frame is a root of its own under the conservative scan.
- Nodes and buffers are managed objects: an `erase` unlinks a node and the collector reclaims it later; nothing is ever freed by hand, so a cycle through a container is collected like any other cycle.
- The collector reads the elements only where they may hold pointers. A buffer whose element type cannot hold a `tracked_ptr` (trivially default constructible, or smaller than a pointer: `int`, `double`, a plain struct) gets an empty pointer map when it is created, so the marking never reads its contents: a `vector<int>` of a million elements costs a cycle what one object does, and its buffer is not even zeroed on allocation. A node holds links and an element; the words of an element that turn out to be data leave the node type's map at the first node found holding some ([Pointer maps](../../garbage_collector/overview.md#pointer-maps)), and from then on the marking reads the links alone.
- The node containers (`list`, `forward_list`, the maps and sets) destroy an element the moment it is erased, cleared, assigned over or the container is destroyed, exactly like `std`. An iterator to an erased element is invalid as in `std`; it keeps the node's memory mapped but not the element. The one exception is a container dying in a sweep, inside a managed object nobody refers to any more: its nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it.
- `vector` and `array` destroy their elements themselves, exactly when `std` does: on removal (`erase`, `pop_back`, `clear`, `resize`, `assign`), on a reallocation (the moved-from elements), in the destructor, wherever that runs, on a stack or in a sweep inside a dying managed object. The collector never destroys a buffer: it only frees one nothing refers to, and it never needs to know how many elements a buffer holds. A buffer is referred to only through a pointer to its first element: a `tracked_ptr` or reference to an element does not keep it, so once the container is gone such a pointer dangles, as in `std`. `clear()` keeps the capacity, like `std::vector`; `shrink_to_fit()` on an empty vector drops the buffer. A `vector` is three words (the buffer, the count, the capacity), an `array<T>` two; the buffer's own header holds only its metadata and the capacity the size class granted.
- `array<T, N>` keeps its elements inline, like `std::array`: an aggregate (`sgcl::array<sgcl::tracked_ptr<T>, 4> roots = {}`) with the tuple interface and costs nothing beyond the elements. `array<T>` (no `N`) is a buffer whose size is fixed when it is created (`array<T>(n)`, `array<T>(n, value)`, from a range or an initializer list): the cheapest managed sequence, a single word to hold, copied deeply and moved by handing the buffer over.
- Elements aligned beyond 16 bytes are not supported in buffers; `vector<bool>` is a plain vector of `bool`.
- Past `std`: `ordered_map` and `ordered_set` are the hash containers iterated in insertion order (Java's `LinkedHashMap` and `LinkedHashSet`): the same table with every node on one more list, `front` the oldest element and `back` the newest, `to_back` and `to_front` to move one, which makes a cache with an eviction order (LRU) two lines over the map ([ordered_map](ordered_map.md)).

## Weak containers
`weak_map<Key, T>`, `weak_multimap<Key, T>` and `weak_set<Key>` are containers keyed by objects they do not keep alive: the key is the object itself, its identity and not its contents, looked up by a `tracked_ptr` to it and held by a `weak_ptr`. An entry whose object a cycle has found unreachable is dead: never found, passed over by the iteration, dropped by a sweep, which the container runs by itself every so many insertions (as many as it has entries) and on `sweep()`. Metadata attached to objects from outside, a cache keyed by the object, a registry that forgets. The iteration hands out the object as a strong pointer, held while the iterator stands on the entry, and the value by reference. A value holding a strong pointer to its own key keeps the key alive, and the entry with it: there are no ephemerons.

```cpp
struct Node { int value; };
sgcl::weak_map<Node, std::string> names;   // a name for any object, kept outside it
{
    sgcl::tracked_ptr node = sgcl::make_tracked<Node>(42);
    names[node] = "the answer";
    if (auto it = names.find(node); it != names.end()) {
        std::cout << it->key->value << ": " << it->value << "\n";   // 42: the answer
    }
}   // the last strong pointer is gone
sgcl::collector::clear_stack();          // the dead frame zeroed, so that the conservative scan keeps nothing
sgcl::collector::force_collect(true);    // optional, for the demonstration: the cycle clears the key
std::cout << names.sweep() << " entry gone\n";   // 1
```

The entries are hashed and compared by the object's address, read from the weak pointer's cell without a lock: the cell holds the address while the object lives, and the weak phase clears it before the sweep frees the slot, so an address in a cell never names a slot's earlier occupant, a dead entry equals nothing (its own key included), and the object that takes the slot next gets an entry of its own. A `sgcl::multimap<const Node*, ...>` would not do: a raw address in a managed container is a word holding a heap address, which the pointer map built by elimination ([Pointer maps](../../garbage_collector/overview.md#pointer-maps)) follows like a `tracked_ptr`, so the map would keep every node alive by its key. The lookups cost those of `unordered_map` plus a load of the cell per key compared; the sweeps a pass over the entries, paid for by the insertions between them.

## Pages

### Sequences and associative containers

| page | header | `std` counterpart |
|---|---|---|
| [vector](vector.md) | `vector.h` | `std::vector` |
| [array](array.md) | `array.h` | `std::array`, with the braces of an aggregate and the mixins of a range |
| [dynamic_array](dynamic_array.md) | `dynamic_array.h` | a count fixed at creation in a managed buffer that never moves: Java's `new T[n]`; the rings of the channels |
| [deque](deque.md) | `deque.h` | `std::deque` |
| [list](list.md) | `list.h` | `std::list` |
| [forward_list](forward_list.md) | `forward_list.h` | `std::forward_list` |
| [stack](stack.md) | `stack.h` | `std::stack` |
| [queue, priority_queue](queue.md) | `queue.h` | `std::queue`, `std::priority_queue` |
| [map](map.md) | `map.h` | `std::map` |
| [multimap](multimap.md) | `multimap.h` | `std::multimap` |
| [set](set.md) | `set.h` | `std::set` |
| [multiset](multiset.md) | `multiset.h` | `std::multiset` |
| [unordered_map](unordered_map.md) | `unordered_map.h` | `std::unordered_map` |
| [unordered_multimap](unordered_multimap.md) | `unordered_multimap.h` | `std::unordered_multimap` |
| [unordered_set](unordered_set.md) | `unordered_set.h` | `std::unordered_set` |
| [unordered_multiset](unordered_multiset.md) | `unordered_multiset.h` | `std::unordered_multiset` |
| [ordered_map](ordered_map.md) | `ordered_map.h` | `unordered_map` iterated in insertion order: Java's `LinkedHashMap`; `front`, `back`, `to_back`, `to_front` |
| [ordered_set](ordered_set.md) | `ordered_set.h` | `unordered_set` iterated in insertion order: Java's `LinkedHashSet` |

The questions, the order and the writes of a range (`contains`, `index_of`, `find_if`, `sort`, `reverse`, `min`, `for_each`...) are members of every container that iterates, from the mixins of `core` ([the mixins and the concepts](../core/mixin/README.md)); the maps read by their key through [m_lookup](../core/mixin/m_lookup.md).

### Weak containers

| page | header | what it is |
|---|---|---|
| [weak_map, weak_multimap](weak_map.md) | `weak_map.h` | values attached to objects the map does not keep alive: keyed by the object, an entry dies with it |
| [weak_set](weak_set.md) | `weak_set.h` | a set of objects it does not keep alive |
| [expiry_queue](expiry_queue.md) | `expiry_queue.h` | a callback for an object the collector found unreachable, with the object alive again for the call |

### Immutable containers

A family of their own in the namespace `sgcl::im` and the directory `im/`, with a [README](im/README.md) that is their guide (the model: every operation a new version sharing all but the path it changed; the state of a program as a value): [im::vector](im/vector.md), [im::list](im/list.md), [im::map](im/map.md), [im::set](im/set.md), and their [benchmarks](im/benchmarks.md) against immer and `std`.
