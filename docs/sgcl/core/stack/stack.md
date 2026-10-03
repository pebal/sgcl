[sgcl](../../README.md) › [core](../README.md) › [stack](../stack.md)

# sgcl::stack\<T, Container\>::stack

```cpp
stack() noexcept(std::is_nothrow_default_constructible_v<Container> &&                         // (1)
                 std::is_nothrow_move_constructible_v<Container>);
explicit stack(const Container& cont)                                                          // (2)
    noexcept(std::is_nothrow_copy_constructible_v<Container>);
explicit stack(Container&& cont) noexcept(std::is_nothrow_move_constructible_v<Container>);    // (3)
template<std::input_iterator InputIt> stack(InputIt first, InputIt last);                      // (4)
```

Constructs a stack.

1. An empty stack: an empty container, made and moved in.
2. A stack over a copy of `cont`; `cont` is unchanged.
3. A stack over `cont` itself, moved in; `cont` is left as the container's move leaves it, empty for the
   containers of the library.
4. A stack whose container is built from the range `[first, last)`, the first element at the bottom.

- (2–3) The last element of `cont` is the top.

The copy and the move constructors and assignments are implicitly declared: those of the container.

## Parameters

| Parameter | Description |
|---|---|
| `cont` | the container the stack is made over |
| `first`, `last` | the range the elements are copied from |

## Complexity

- (1), (3) Constant for the containers of the library.
- (2) Linear in `cont.size()`.
- (4) Linear in the distance between `first` and `last`.

## Exceptions

- (1), (3) None for the containers of the library: what the container's construction and move throw.
- (2) What the copy of the container throws: the copy constructor of `T`.
- (4) What the container's constructor from a range throws: `length_error`, the constructor of `T`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    deque d = {1, 2, 3};
    stack<int> from_copy(d);
    println("{} {}", from_copy.top(), d.size());

    stack<int> from_move(std::move(d));
    println("{} {}", from_move.top(), d.empty());

    vector src = {4, 5};
    stack<int> from_range(src.begin(), src.end());
    println("{}", from_range.top());

    stack<int, vector<int>> on_vector;  // any managed sequence with push_back
    stack deduced(vector{7, 8});  // stack<int, vector<int>>
    println("{} {}", on_vector.empty(), deduced.top());
}
```

Output:

```text
3 3
3 true
5
true 8
```

## See also

- [push](push.md): inserts an element at the top
- [sgcl::stack\<T, Container\>](../stack.md)
