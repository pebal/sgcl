[sgcl](../../README.md) › [core](../README.md) › [stack](../stack.md)

# sgcl::stack\<T, Container\>::emplace

```cpp
template<class... A>
decltype(auto) emplace(A&&... a) noexcept(noexcept(c.emplace_back(std::forward<A>(a)...)));
```

Constructs an element at the top from `a...`: `c.emplace_back(a...)`.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments of the element's constructor |

## Return value

What the container's `emplace_back` returns: a reference to the new element for the containers of the library.

## Complexity

What the container's `emplace_back` costs: amortized constant over a `vector`, constant over a `deque` or a
`list`.

## Exceptions

What the container's `emplace_back` throws: for the containers of the library, what the constructor of `T` from
`a...` throws; none when it is noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Frame {
    string name;
    int depth;
};

int main() {
    stack<Frame> frames;
    frames.emplace("main", 0);
    Frame& call = frames.emplace("parse", 1);  // a reference to the element on top
    call.depth = 2;
    println("{} {}", frames.top().name, frames.top().depth);

    stack<tracked_ptr<int>> s;
    s.push(make_tracked<int>(1));
    int& two = *s.emplace(make_tracked<int>(2));
    println("{} {}", two, s.size());
}
```

Output:

```text
parse 2
2 2
```

## See also

- [push](push.md): inserts an element at the top
- [sgcl::stack\<T, Container\>](../stack.md)
