[sgcl](../../README.md) › [core](../README.md) › [tracked_ptr](../tracked_ptr.md)

# sgcl::tracked_ptr\<T\>::store

```cpp
void store(const tracked_ptr& p, barrier::off_t) noexcept;
```

Stores the word of `p` without the write barrier: a relaxed store of the word alone. For an immutable structure
copying one of its nodes ([immutable](../../immutable/README.md)). The write barrier's promise is that whatever a
pointer is stored to is reachable in the current cycle; a node that never changes holds exactly the words its copy
holds, so the copy may take the words without the barrier, `dst.store(src, barrier::off)`, one word at a time,
and then, the copy complete, one [shade](shade.md) of a pointer to the source makes the source reachable in this
cycle, and the marking, visiting it, marks every child the copy holds: one barrier for the node in place of one per
word.

`barrier` is a struct of `sgcl` (`sgcl/core/types.h`) with the tag type `off_t` and its value `off`: the store
without the barrier is an overload of its own, chosen at compile time, and every other store has the barrier and
needs no tag. The constructor `tracked_ptr(p, barrier::off)` does the same for a word of a node built in place
([(constructor)](tracked_ptr.md), 10).

## Parameters

| Parameter | Description |
|---|---|
| `p` | the pointer whose word is copied; the second parameter is the tag `barrier::off` |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Notes

Two rules make it sound. The source is held by the caller through the copy and the shade (the version being copied
holds it), and the shade comes after the copy is complete, never before: the copies of the immutable containers made
with a shade of the root ahead of them lost nodes. The words are stored one at a time because the collector may
read the copy meanwhile: a word, never a torn vector store.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Branch {
    Branch() = default;
    Branch(const Branch& from) noexcept {
        for (int i : range(4)) {
            children[i].store(from.children[i], barrier::off);
        }
    }
    tracked_ptr<int> children[4];
};

int main() {
    tracked_ptr original = make_tracked<Branch>();
    for (int i : range(4)) {
        original->children[i] = make_tracked<int>(i * 10);
    }

    tracked_ptr copy = make_tracked<Branch>(*original);  // the words without the barrier
    original.shade();                                     // then one barrier for the source
    println("{} {}", *copy->children[3], copy->children[1] == original->children[1]);
}
```

Output:

```text
30 true
```

## See also

- [shade](shade.md): the write barrier for the target, on demand
- [operator=](operator_assign.md): a store with the barrier
- [sgcl::tracked_ptr\<T\>](../tracked_ptr.md)
