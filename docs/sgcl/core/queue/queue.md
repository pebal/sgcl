[sgcl](../../README.md) › [core](../README.md) › [queue](../queue.md)

# sgcl::queue\<T, Container\>::queue

```cpp
/*(1)*/ queue()
            noexcept(std::is_nothrow_default_constructible_v<Container> &&
                     std::is_nothrow_move_constructible_v<Container>);
/*(2)*/ explicit queue(const Container& cont)
            noexcept(std::is_nothrow_copy_constructible_v<Container>);
/*(3)*/ explicit queue(Container&& cont) noexcept(std::is_nothrow_move_constructible_v<Container>);
/*(4)*/ template<std::input_iterator InputIt> queue(InputIt first, InputIt last);
/*(5)*/ queue(const queue& other);
/*(6)*/ queue(queue&& other);
```

Constructs a queue from one of the sources below.

1. An empty queue: an empty container, made and moved in.
2. A queue over a copy of `cont`, its first element at the front.
3. A queue over `cont` itself, moved in; `cont` is left as the container's move leaves it (an empty `deque` or
   `list`).
4. A queue whose container is built from the range `[first, last)`, the first element at the front.
5. A copy of `other`: its container copied.
6. Takes the container of `other` over, by the container's move.

(5) and (6) are the implicit ones, so they are those of the container: noexcept as its copy and its move are.

## Parameters

| Parameter | Description |
|---|---|
| `cont` | the container the queue is made of |
| `first`, `last` | the range the elements are copied from |
| `other` | the queue the container is copied or taken from |

## Complexity

- (1) Constant.
- (2) Linear in `cont.size()`.
- (3) Constant for `deque` and `list`.
- (4) Linear in the distance between `first` and `last`.
- (5) Linear in `other.size()`.
- (6) Constant for `deque` and `list`.

## Exceptions

- (1), (3), (6) What the default and the move constructor of `Container` throw; none for `deque` and `list`.
- (2), (4), (5) What the container's construction throws: the copy constructor of `T`.

When an element's constructor throws, the elements built before it are destroyed and the exception propagates.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    deque d = {1, 2, 3};
    queue<int> from_copy(d);
    println("{} {}", from_copy.front(), d.size());

    queue<int> from_move(std::move(d));
    println("{} {}", from_move.back(), d.size());

    vector src = {4, 5};
    queue from_range(src.begin(), src.end());  // deduced: queue<int>
    println("{} {}", from_range.front(), from_range.back());

    queue<int, list<int>> on_list;  // any managed sequence with push_back and pop_front
    on_list.push(6);
    queue copy = on_list;
    println("{} {}", copy.front(), on_list.size());
}
```

Output:

```text
1 3
3 0
4 5
6 1
```

## See also

- [operator=](operator_assign.md): assigns the contents
- [push](push.md): appends an element
- [sgcl::queue\<T, Container\>](../queue.md)
