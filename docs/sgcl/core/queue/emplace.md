[sgcl](../../README.md) › [core](../README.md) › [queue](../queue.md)

# sgcl::queue\<T, Container\>::emplace

```cpp
template<class... A>
decltype(auto) emplace(A&&... a) noexcept(noexcept(c.emplace_back(std::forward<A>(a)...)));
```

Constructs an element at the end of the queue from `a...`: `c.emplace_back(a...)`.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments of the element's constructor |

## Return value

What the container's `emplace_back` returns: a reference to the new element for `deque` and `list`.

## Complexity

The container's `emplace_back`: constant for `deque` and `list`.

## Exceptions

What the constructor of `T` throws; none when it is noexcept and the container's `emplace_back` is noexcept
with it, as for `deque` and `list`.

If an exception is thrown, the container's `emplace_back` leaves the queue as it was, for `deque` and `list`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    queue<tracked_ptr<int>> q;
    q.push(make_tracked<int>(1));
    int& two = *q.emplace(make_tracked<int>(2));  // a reference to the element at the back
    two *= 10;
    println("{} {}", *q.front(), *q.back());

    queue<string> words;
    words.emplace(3, 'x');  // string(3, 'x') made in place
    println("{}", words.front());
}
```

Output:

```text
1 20
xxx
```

## See also

- [push](push.md): appends a value
- [sgcl::queue\<T, Container\>](../queue.md)
