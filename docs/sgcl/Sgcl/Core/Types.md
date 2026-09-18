# Optional, None, Pair

```cpp
#include "sgcl/Sgcl/Core/Types.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T> using Optional = sgcl::optional<T>;    // std::optional<T>
    inline constexpr sgcl::nullopt_t None = sgcl::nullopt;   // the empty Optional
    template<class A, class B> using Pair = sgcl::pair<A, B>;   // std::pair<A, B>
}
```

The same names in the `sgcl` interface: [optional, pair, tuple](../../core/aliases.md).

The standard types the interface hands back, under its names: `Optional<T>` is `std::optional<T>`, `None` the empty one (`std::nullopt`), `Pair<A, B>` is `std::pair<A, B>`. Nothing is added and nothing wrapped: `HasValue()` is not there, `has_value()`, `value()`, `*`, `->`, `value_or` and the comparisons are the standard's, and an `Optional` converts to `bool` in an `if`. They hold a `Ptr` correctly as they are, because each keeps every value at a fixed offset of its own, one value per place, so a pointer inside never shares its word with data and the collector keeps following it ([README: Pointer maps](../../../garbage_collector/overview.md#pointer-maps)); `std::variant`, `std::any`, `std::function` and `std::expected` do not, and the interface has [Variant](Variant.md), [Any](Any.md), [Function](Function.md) and [Expected](Expected.md) for them. A tuple is `std::tuple` (what [WhenAll](../Async/When.md) returns); the range a lookup of several values hands back is a [Range](Range.md).

Where the interface hands one back: `Optional<T>` from [Channel](../Async/Channel.md)'s `Receive` and `TryReceive` (`None` once the channel is closed and drained), from `TryDequeue` of [ConcurrentQueue](../Concurrent/ConcurrentQueue.md) and `TryPop` of [ConcurrentStack](../Concurrent/ConcurrentStack.md), from `TryGet` of [ConcurrentDictionary](../Concurrent/ConcurrentDictionary.md) and [ConcurrentSortedDictionary](../Concurrent/ConcurrentSortedDictionary.md), and from `co_await g.Next()` of an [AsyncGenerator](../Async/Task.md) (`None` at the end); `Pair<Key, Value>` is what a dictionary holds and what its initializer list is made of.

## Rules

- An `Optional` or a `Pair` with a `Ptr` inside lives where a `Ptr` may: on a stack or in a managed object, never in unmanaged memory ([README: The rules](../../core/README.md#the-rules), 1); with `RootPtr`s inside, anywhere. It is the pointer's rule, and these types add none.
- A `Ptr` in an `Optional` that is `None` is not there: the word holds nothing the collector follows, and the object it held is released with `reset()` or the assignment of `None`, as it would be by a `Ptr` reset.
- Thread safety is `std`'s: none. A value shared between threads is held by an [Atomic](../Concurrent/Atomic.md) or handed over through a [Channel](../Async/Channel.md).

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

struct Node {
    int value;
    Ptr<Node> next;
};

// The first node with a value above the limit, or None: an Optional of a
// Ptr, the pointer at a fixed offset of its own, so that the node found
// stays alive as long as the Optional holds it.
Optional<Ptr<Node>> FirstAbove(Ptr<Node> n, int limit) {
    for (; n; n = n->next) {
        if (n->value > limit) {
            return n;
        }
    }
    return None;
}

int main() {
    Ptr head = Make<Node>(1);
    head->next = Make<Node>(5);
    head->next->next = Make<Node>(9);

    if (auto found = FirstAbove(head, 4)) {
        std::cout << (*found)->value << "\n";                 // 5
    }
    std::cout << FirstAbove(head, 10).has_value() << "\n";    // 0

    Pair<Ptr<Node>, int> counted{head, 3};                    // a pointer and a count, each in a word of its own
    auto [node, count] = counted;
    std::cout << node->value << " " << count << "\n";         // 1 3
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

- [Variant](Variant.md), [Any](Any.md), [Function](Function.md), [Expected](Expected.md): the `std` types that needed a version of their own; [Range](Range.md): a pair of iterators as a range
- [Ptr](Ptr.md): the pointer these types hold; [Channel](../Async/Channel.md), [ConcurrentQueue](../Concurrent/ConcurrentQueue.md): where an `Optional` comes from
- [README: Pointer maps](../../../garbage_collector/overview.md#pointer-maps), [README: The rules](../../core/README.md#the-rules)
