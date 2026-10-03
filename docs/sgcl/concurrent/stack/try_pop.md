[sgcl](../../README.md) › [concurrent](../README.md) › [stack](../stack.md)

# sgcl::concurrent::stack\<T\>::try_pop

```cpp
optional<T> try_pop() noexcept(std::is_nothrow_move_constructible_v<T>);
```

Takes the top element, or nothing when the stack is empty. The pop loads the head and swings it to the node below
with a compare-exchange, retrying against the pushes and pops of other threads; the element is then moved out of
the node into the `optional` returned and destroyed in the node. A null head at the load means the stack was
empty at that moment.

## Parameters

None.

## Return value

The top element, moved out, or `nullopt` when the stack was empty. `optional` is the alias of `std::optional`
([aliases](../../core/aliases.md)).

## Complexity

Constant, plus the retries of a lost compare-exchange when other threads push or pop at once.

## Exceptions

What the move constructor of `T` throws; none when it is noexcept. The node is taken off the stack before the element is moved: when its
move throws, the element is lost, left in its node for the collector, and the stack is otherwise intact.

## Notes

Lock-free, and linearizable at the compare-exchange on the head: of two threads loading the same head, exactly
one takes its element, and the other retries from the head it found. A lost exchange backs off as
[push](push.md) does. The element is destroyed on the thread that pops it, as `std::stack::pop` destroys it, and
the node is garbage from that moment.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::stack<string> words;
    words.push("first");
    words.push("second");

    while (auto w = words.try_pop()) {
        println("{}", *w);
    }
    println("{}", words.try_pop());
}
```

Output:

```text
second
first
nullopt
```

## See also

- [pop](pop.md): waits for an element
- [push](push.md): puts an element on the top
- [sgcl::concurrent::stack\<T\>](../stack.md)
