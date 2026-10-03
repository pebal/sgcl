[sgcl](../../README.md) › [core](../README.md)

# sgcl::weak_ptr\<T\>

```cpp
#include "sgcl/core/weak_ptr.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T>
    class weak_ptr;
}
```

`sgcl::weak_ptr<T>` is a pointer that keeps nothing alive: the object lives as long as something else reaches it
through [tracked_ptr](../tracked_ptr/README.md)s or a [unique_ptr](../unique_ptr/README.md), and [lock](lock.md) says whether
it still does. `lock()` is the object as a `tracked_ptr` while it is reachable, and null once a cycle has found it
unreachable; [expired](expired.md) is the same question without the pointer. It is what `std::weak_ptr` is
to `std::shared_ptr`, without the counts, and what Go's `weak.Pointer` is to a pointer: a back pointer, a cache
entry, an observer that must not extend a lifetime.

It is one word: a `tracked_ptr` to a small cell on the managed heap that holds the target as a word the collector
clears instead of tracing. A `weak_ptr` made from a strong pointer allocates a cell of its own (16 bytes); copies
share it, and the cell is collected with the last copy. Unlike `std::weak_ptr`, a `weak_ptr` converts to another
`weak_ptr` only of its own type with `const` added: a base may lie at an offset that only the live object gives, so a
`weak_ptr` of a base is made from a strong pointer (`weak_ptr<Base>(w.lock())`), and no conversion locks the object
behind the program's back ([(constructor)](weak_ptr.md), Notes). That word is a `tracked_ptr`, so a `weak_ptr` lives where
one may (a [root_ptr](../root_ptr/README.md) holds one anywhere). The clearing is a phase of the cycle, after the marking and
before the sweep: `lock()` never hands out an object the sweep will destroy or the slot it will be reused for, and a
`lock()` that races with the clearing either sees the null or wins, holding the object for at least one more cycle.
Between the object becoming unreachable and the cycle that notices, `lock()` still returns it: the lag of any
garbage collector.

## Rules

- A `weak_ptr<T>` is a `tracked_ptr` (to a cell), so it lives where one may: on a stack or inside a managed object,
  never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame
  ([The rules](../README.md#the-rules), 1 and 4).
- It addresses an object no `unique_ptr` owns: a `tracked_ptr` cannot address one either, and the owner's delete
  would leave the cell dangling. Debug builds assert it.
- Threads share a `weak_ptr` the way they share a `tracked_ptr`: one written by one thread and read by another needs
  the program's own synchronization ([The rules](../README.md#the-rules), 6). `lock()` itself is safe against the
  collector clearing the cell at the same time.
- In a destructor, a `weak_ptr` member is a `tracked_ptr` member: the cell may be dying in the same sweep, so it is
  not to be read there ([The rules](../README.md#the-rules), 5).
- A `weak_ptr` in a cycle does not keep it: two objects pointing at each other through a `tracked_ptr` and a
  `weak_ptr` are collected when nothing else reaches them.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type pointed to: an object type, `const` or not, or `void`. |

## Member types

| Type | Definition |
|---|---|
| `element_type` | `T` |
| `pointer_type` | `tracked_ptr<T>`, what [lock](lock.md) returns |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](weak_ptr.md) | constructs the pointer, a cell of its own or a shared one |
| `(destructor)` | drops the cell; the cell is collected with its last copy |
| [operator=](operator_assign.md) | assigns the pointer |

#### Modifiers

| Function | Description |
|---|---|
| [reset](reset.md) | drops the cell |
| [swap](swap.md) | exchanges the cells of two pointers |

#### Observers

| Function | Description |
|---|---|
| [lock](lock.md) | the object as a `tracked_ptr`, or null once it was found unreachable |
| [expired](expired.md) | checks whether the cell has been cleared |

## Non-member functions

| Function | Description |
|---|---|
| [swap](swap.md) | exchanges the cells of two pointers |

## Deduction guides

```cpp
template<class T>
weak_ptr(const tracked_ptr<T>&) -> weak_ptr<T>;

template<class T>
weak_ptr(const root_ptr<T>&) -> weak_ptr<T>;
```

`weak_ptr w = item` is a `weak_ptr<T>` for a `tracked_ptr<T> item`. The explicit argument is needed for a base
class, an empty `weak_ptr` or a member declaration.

## Complexity

Every operation is constant. A `weak_ptr` made or assigned from a strong pointer allocates its cell; a copy shares
the cell and allocates nothing ([Benchmarks: Weak pointers](../benchmarks.md#weak-pointers)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

// a tree owned downward, with back pointers that keep nothing: dropping a
// parent never keeps it alive through its children
struct Node {
    explicit Node(int id) : id(id) {}
    int id;
    vector<tracked_ptr<Node>> children;
    weak_ptr<Node> parent;
};

tracked_ptr<Node> add_child(const tracked_ptr<Node>& parent, int id) {
    tracked_ptr child = make_tracked<Node>(id);
    child->parent = parent;
    parent->children.push_back(child);
    return child;
}

void print_path(const tracked_ptr<Node>& node) {
    print("{}", node->id);
    for (tracked_ptr n = node->parent.lock(); n; n = n->parent.lock()) {
        print(" {}", n->id);
    }
    println();
}

int main() {
    tracked_ptr root = make_tracked<Node>(0);
    tracked_ptr branch = add_child(root, 1);
    tracked_ptr leaf = add_child(branch, 2);
    print_path(leaf);

    root = nullptr;  // the branch's back pointer expires, the leaf's still locks
    collector::force_collect(true);  // optional, for the demonstration
    println("{} {}", branch->parent.expired(), leaf->parent.lock() == branch);
    print_path(leaf);
}
```

Output:

```text
2 1 0
true true
2 1
```

## See also

- [tracked_ptr](../tracked_ptr/README.md), [unique_ptr](../unique_ptr/README.md), [make_tracked](../make_tracked.md)
- [weak_map](../weak_map/README.md), [weak_set](../weak_set/README.md): containers keyed by objects they do not keep alive
- [expiry_queue](../expiry_queue/README.md): a `weak_ptr` plus a function called with the object, alive one last time, when
  it is found unreachable
- [collector](../collector/README.md): `force_collect`
- [Benchmarks: Weak pointers](../benchmarks.md#weak-pointers)
- [README: Pointers](../README.md#pointers), [README: The rules](../README.md#the-rules)
