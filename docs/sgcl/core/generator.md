# sgcl::generator

```cpp
#include "sgcl/core/generator.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T> class generator;
}
```

`generator<T>` is used like `std::generator<T>` of C++23 (a range-for over the values a coroutine `co_yield`s), but it is a C++20 type with its frame on the managed heap ([managed_frame](coroutine.md)), an input iterator, and nothing else. The parameters and locals of the coroutine are roots while it is suspended between two values, so a generator may walk a structure it builds as it goes, or one handed to it, with nothing else holding it. It runs where `next()` is called, the consumer's thread, and needs no scheduler: `next()` resumes it, a `co_yield` gives control back. A generator that waits between its values (a channel, a sleep) is an [`async::generator`](../async/coroutine.md#async::generator) of the async module. The value type is always spelled, `generator<int>`, `generator<tracked_ptr<Node>>`.

A coroutine that `co_yield`s values, consumed with a range-for or with `next()`/`value()`; an exception it throws comes out of `next()` (or the iterator's `++`, or `begin()`). Move-only, two words (a `frame_ptr<promise_type>`); a default-constructed `generator` is empty and yields nothing. Single pass: the coroutine advances with every `next()`, and `begin()` advances it too.

## Members

### promise_type

```cpp
struct promise_type : managed_frame {
    std::optional<T> value;
    std::exception_ptr error;

    generator get_return_object();
    std::suspend_always initial_suspend() noexcept;
    std::suspend_always final_suspend() noexcept;
    std::suspend_always yield_value(T v);     // value.emplace(std::move(v)), then suspend
    void return_void() noexcept;
    void unhandled_exception() noexcept;      // error = std::current_exception()
};
```

The promise of a `generator`. Lazy: nothing runs until the first `next()` (or `begin()`). `co_yield v` moves `v` into `value` and suspends; the coroutine ends with `co_return;` or by falling off its end, and may not `co_return` a value.

```cpp
generator<int> squares(int n) {
    for (int i : range(1, n + 1)) {
        co_yield i * i;                       // value, then suspended until the next next()
    }
}
```

### iterator

```cpp
class iterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = const T*;
    using reference = const T&;

    iterator() noexcept = default;
    reference operator*() const noexcept;     // the generator's value()
    pointer operator->() const noexcept;
    iterator& operator++();                   // next(); equal to end() when it returns false
    void operator++(int);
    bool operator==(const iterator& o) const noexcept;
    bool operator!=(const iterator& o) const noexcept;
};
```

An input iterator over the values, for the range-for. Dereferencing gives the current value as `const T&`; `++` runs the coroutine to its next `co_yield` and turns into `end()` when the coroutine ends. Post-increment returns nothing. Two iterators are equal when they refer to the same generator or are both `end()`. An exception the coroutine throws comes out of `++`.

```cpp
generator<int> g = squares(3);
for (auto it = g.begin(); it != g.end(); ++it) {
    int v = *it;                              // 1, 4, 9
}
```

### Constructor

```cpp
generator() noexcept = default;
```

An empty generator: `next()` returns `false`, `begin() == end()`, `value()` may not be called. A generator with a coroutine comes from calling a coroutine function that returns one; move constructible and move assignable, not copyable.

```cpp
generator<int> g;                         // empty: no values
g = squares(4);                               // the coroutine, not started yet
```

### next

```cpp
bool next();
```

Runs the coroutine to its next `co_yield`: `true`, and `value()` is the yielded value; or to its end: `false`. On an empty or finished generator, `false` at once. An exception the coroutine throws is rethrown by `next()`, after which the generator is finished.

```cpp
generator<int> g = squares(4);
while (g.next()) {
    int v = g.value();                        // 1, 4, 9, 16
}
```

### value

```cpp
const T& value() const noexcept;
```

The value of the last `co_yield`, a reference into the frame (the promise's `value`): overwritten by the next `co_yield`, gone when the generator is destroyed. Precondition: the last `next()` returned `true`.

```cpp
generator<int> g = squares(4);
if (g.next()) {
    const int& first = g.value();             // 1
}
```

### begin, end

```cpp
iterator begin();
iterator end() noexcept;
```

The range-for support. `begin()` calls `next()`, so it runs the coroutine to its first value and returns an iterator to it, or `end()` when there is none; it is not repeatable: a second `begin()` advances the coroutine again. `end()` is the empty iterator.

```cpp
for (int v : squares(4)) {                    // the temporary generator lives for the loop
    // v: 1, 4, 9, 16
}
```

### destroy

```cpp
void destroy() noexcept;
```

Destroys the coroutine, running the destructors of its locals and promise, and leaves the generator empty; the destructor does the same. A generator abandoned in the middle of its values is destroyed the same way, wherever it was suspended.

```cpp
generator<int> g = squares(100);
g.next();
g.destroy();                                  // the other 99 never happen; g is empty
```

## Example

```cpp
#include "sgcl/sgcl.h"

#include <iostream>

using namespace sgcl;

struct Node {
    Node(int v, tracked_ptr<Node> n) : value(v), next(n) {}
    int value;
    tracked_ptr<Node> next;
};

// Yields the nodes of a chain it builds as it goes: the local keeps
// the whole chain alive while the generator is suspended
generator<tracked_ptr<Node>> chain(int count) {
    tracked_ptr<Node> last;                         // a local in a managed frame: a root
    for (int i : range(1, count + 1)) {
        tracked_ptr n = make_tracked<Node>(i, last);
        last = n;
        co_yield n;
    }
}

int main() {
    int sum = 0;
    for (auto& n : chain(4)) {
        collector::force_collect();                 // optional, only to show the point: the chain survives
        int length = 0;
        for (auto p = n; p; p = p->next) {
            ++length;                               // the earlier nodes, held by the frame's local
        }
        sum += length;
    }
    std::cout << sum << '\n';                       // 1 + 2 + 3 + 4
}
```

The output:

```
10
```

## See also

- [managed_frame, frame_ptr](coroutine.md): the managed frame under the generator
- [coroutine](../async/coroutine.md): `task` and `async::generator`, the coroutine types of the async module
- [range](range.md), [tracked_ptr](tracked_ptr.md)
- `tests/core/generator.cpp`: the chain of the yielded nodes held by the frame across the yields and collections
