[sgcl](../../README.md) › [core](../README.md)

# sgcl::generator\<T\>

```cpp
#include "sgcl/core/generator.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T>
    class generator;
}
```

`sgcl::generator<T>` is a coroutine that `co_yield`s values, consumed with a range-for or with `next()` and
`value()`. It is used like `std::generator<T>` of C++23, but it is a C++20 type with its frame on the managed heap
([managed_frame](../managed_frame.md)), an input iterator and nothing else. The parameters and locals of the coroutine
are roots while it is suspended between two values, so a generator may walk a structure it builds as it goes, or
one handed to it, with nothing else holding it. It runs where `next()` is called, on the consumer's thread, and
needs no scheduler: `next()` resumes it, a `co_yield` gives control back. A generator that waits between its values
(a channel, a sleep) is an `async::generator` of the async module ([generator](../../async/generator/README.md)). The value
type is always spelled, `generator<int>`, `generator<tracked_ptr<Node>>`.

What differs from `std::generator`: the frame is managed and traced, `T` is the type of the value the promise keeps
(no reference type, no allocator parameter), and there is `next()` and `value()` beside the iterator. Go's nearest
form is the range-over-func iterator (`iter.Seq`), where the producer calls the loop's body; here the consumer
resumes the producer.

## Rules

- A generator is one [frame_ptr](../frame_ptr/README.md): it holds the frame by a root and lives anywhere, on a stack, in a
  managed object, in a `std` container, in a global.
- The parameters, locals and temporaries of the coroutine are roots while the frame is held: a `tracked_ptr`, a
  container, a string kept across a `co_yield` keep what they refer to.
- The coroutine is lazy: nothing runs until the first `next()` or the iterator's first look at a value. It runs on
  the thread that calls `next()`, one step per call, and a generator is resumed by one thread at a time.
- An exception the coroutine throws comes out of `next()` or the iterator's look that runs it; the generator is
  finished after it.
- Single pass: the coroutine advances with every `next()` and with every look of an iterator at a new value; `++` and
  `begin()` run nothing, so a range-for left by a `break` or a `std::views::take` goes on, in the next one, from the
  value after the last one looked at.
- Move-only: a move hands the frame over and leaves the source empty. Destroying a generator, or a move assignment
  over one, destroys the coroutine at once, wherever it was suspended.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the values: a type `co_yield` moves into the promise, held in an `optional<T>` there. |

## Member types

| Type | Definition |
|---|---|
| [promise_type](../generator-promise_type.md) | the promise of the coroutine, derived from `managed_frame` |
| [iterator](../generator-iterator.md) | a lazy input iterator over the values, for the range-for and the views |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](generator.md) | constructs an empty generator |
| `(destructor)` | destroys the coroutine, if any |
| `operator=` | the move assignment: destroys the coroutine held, takes the other's over |
| [next](next.md) | runs the coroutine to its next value |
| [value](value.md) | the value of the last `co_yield` |
| [destroy](destroy.md) | destroys the coroutine and leaves the generator empty |

#### Iterators

| Function | Description |
|---|---|
| [begin](begin.md) | an iterator at the next value, which runs the coroutine at its first look |
| [end](end.md) | the end of a range-for, `std::default_sentinel` |

## Complexity

`next()` is one resumption of the coroutine; the frame is allocated once, as a managed buffer, when the coroutine
function is called.

## Examples

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int value;
    tracked_ptr<Node> next;
};

// Yields the nodes of a chain it builds as it goes: the local keeps the whole chain alive while
// the generator is suspended
generator<tracked_ptr<Node>> chain(int count) {
    tracked_ptr<Node> last;  // a local in a managed frame: a root
    for (int i : range(1, count + 1)) {
        tracked_ptr n = make_tracked<Node>(i, last);
        last = n;
        co_yield n;
    }
}

int main() {
    int sum = 0;
    for (auto& n : chain(4)) {
        collector::force_collect();  // optional, only to show the point: the chain survives
        int length = 0;
        for (auto p = n; p; p = p->next) {
            ++length;  // the earlier nodes, held by the frame's local
        }
        sum += length;
    }
    println("{}", sum);
}
```

Output:

```text
10
```

A value the consumer takes over: a `std::unique_ptr` cannot be copied out of the frame, so the loop binds a
reference and moves from it.

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <memory>
#include <vector>

using namespace sgcl;

generator<std::unique_ptr<int>> numbers(int count) {
    for (int i : range(1, count + 1)) {
        co_yield std::make_unique<int>(i);
    }
}

int main() {
    std::vector<std::unique_ptr<int>> taken;
    for (auto& p : numbers(3)) {
        // the frame keeps an empty unique_ptr until the next co_yield
        taken.push_back(std::move(p));
    }
    println("{} {} {}", *taken[0], *taken[1], *taken[2]);
}
```

Output:

```text
1 2 3
```

## See also

- [managed_frame](../managed_frame.md), [frame_ptr](../frame_ptr/README.md): the managed frame under the generator
- [coroutine](../coroutine.md): the managed frames of coroutines
- [task](../../async/task/README.md), [generator](../../async/generator/README.md): `task` and `async::generator`, the coroutine types of the async module
- [range](../range/README.md), [tracked_ptr](../tracked_ptr/README.md)
