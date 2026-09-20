# sgcl::optional, pair, tuple, error_code

```cpp
#include "sgcl/core/aliases.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    using std::optional;  using std::nullopt;  using std::nullopt_t;  using std::make_optional;
    using std::pair;      using std::make_pair;
    using std::tuple;     using std::make_tuple;  using std::tie;  using std::forward_as_tuple;
    using std::tuple_size;  using std::tuple_size_v;  using std::tuple_element;  using std::tuple_element_t;
    using std::error_code;  using std::error_category;  using std::error_condition;
}
```

The standard types that hold a `tracked_ptr` or a `weak_ptr` correctly as they are, under the library's names so that the safe set is one namespace. Nothing is added and nothing wrapped: `sgcl::optional<T>` is `std::optional<T>`, `sgcl::pair` is `std::pair`, `sgcl::tuple` is `std::tuple`, with their helpers (`make_optional`, `make_pair`, `make_tuple`, `tie`, `forward_as_tuple`, `tuple_size`, `tuple_element`). What makes them safe is their layout: each keeps every value at a fixed offset of its own, one value per place, so a pointer inside never shares its word with data, and the collector's pointer map, built by elimination, finds a pointer or null at that offset in every object and keeps following it ([README: Pointer maps](../../garbage_collector/overview.md#pointer-maps)). `std::variant`, `std::any`, `std::function` and `std::expected` do not lay their contents out that way, and the library has types of its own for them: [variant](variant.md), [any](any.md), [function](function.md), [expected](expected.md). Nothing here for `std::shared_ptr` and `std::weak_ptr`: a managed object is held by a `tracked_ptr`, and shared from unmanaged memory through `tracked_ptr::to_shared()`.

Where the library hands one back: `optional<T>` from [channel](../async/channel.md)'s `receive` and `try_receive` (empty once the channel is closed and drained), from `try_pop` of [concurrent_queue](../concurrent/concurrent_queue.md) and [concurrent_stack](../concurrent/concurrent_stack.md), and from `co_await g.next()` of an [async_generator](../async/coroutine.md) (empty at the end); `pair` is what the maps hold; `tuple` is what [when_all](../async/when.md) returns.

`error_code` (with `error_category` and `error_condition`) is the standard's error code under the library's name, so that the public interface of a module names no `std` type: what an [io::error](../io/error.md) carries — a value of `errno` in the system category, or a code of a category of the module's own — compared with `std::errc` conditions as `std::error_code` is.

## Rules

- An `optional`, a `pair` or a `tuple` with a `tracked_ptr` inside lives where a `tracked_ptr` may: on a stack or in a managed object, never in unmanaged memory ([README: The rules](README.md#the-rules), 1); with `sgcl::tracked_ptr`s inside, anywhere. It is the pointer's rule, and these types add none.
- A `tracked_ptr` in an `optional` that is empty is not there: the word holds nothing the collector follows, and the object it held is released with `reset()` or the assignment of `nullopt`, as it would be by a `tracked_ptr` reset.
- Thread safety is `std`'s: none. A value shared between threads is held by an [atomic](../concurrent/atomic.md) or handed over through a [channel](../async/channel.md).

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

struct Node {
    int value;
    tracked_ptr<Node> next;
};

// The first node with a value above the limit, or nothing: an optional of
// a tracked_ptr, the pointer at a fixed offset of its own, so that the
// node found stays alive as long as the optional holds it.
optional<tracked_ptr<Node>> first_above(tracked_ptr<Node> n, int limit) {
    for (; n; n = n->next) {
        if (n->value > limit) {
            return n;
        }
    }
    return nullopt;
}

int main() {
    tracked_ptr head = make_tracked<Node>(1);
    head->next = make_tracked<Node>(5);
    head->next->next = make_tracked<Node>(9);

    if (auto found = first_above(head, 4)) {
        std::cout << (*found)->value << "\n";                     // 5
    }
    std::cout << first_above(head, 10).has_value() << "\n";       // 0

    pair<tracked_ptr<Node>, int> counted{head, 3};    // a pointer and a count, each in a word of its own
    auto [node, count] = counted;
    std::cout << node->value << " " << count << "\n";             // 1 3
    return 0;
}
```

The output:

```
5
0
1 3
```

## See also

- [variant](variant.md), [any](any.md), [function](function.md), [expected](expected.md): the `std` types that needed a version of their own
- [tracked_ptr](tracked_ptr.md): the pointer these types hold; [channel](../async/channel.md), [concurrent_queue](../concurrent/concurrent_queue.md): where an `optional` comes from
- [README: Pointer maps](../../garbage_collector/overview.md#pointer-maps), [README: The rules](README.md#the-rules)
