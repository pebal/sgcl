[sgcl](../../README.md) › [core](../README.md) › [stack](README.md)

# sgcl::stack\<T, Container\>::pop

```cpp
void pop() noexcept(noexcept(c.pop_back()));
```

Removes the top element: `c.pop_back()`, which destroys it there and then, as with `std::stack`. The stack must
not be empty.

## Parameters

None.

## Return value

None.

## Complexity

Constant for the containers of the library.

## Exceptions

None for the containers of the library, whose `pop_back` is noexcept; what the container's `pop_back` throws
otherwise.

## Notes

`pop` returns nothing, as `std::stack::pop`: the element is read with [top](top.md) first. Popping a
`tracked_ptr` destroys the pointer, not its object; the object lives on while another pointer reaches it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    stack<char> s;
    for (char c : string("abc")) {
        s.push(c);
    }

    vector<char> reversed;
    while (!s.empty()) {
        reversed.push_back(s.top());
        s.pop();
    }
    println("{}", reversed);
}
```

Output:

```text
['c', 'b', 'a']
```

## See also

- [top](top.md): the top element
- [push](push.md): inserts an element at the top
- [sgcl::stack\<T, Container\>](README.md)
