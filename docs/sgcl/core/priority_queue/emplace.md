[sgcl](../../README.md) › [core](../README.md) › [priority_queue](../priority_queue.md)

# sgcl::priority_queue\<T, Container, Compare\>::emplace

```cpp
template<class... A>
void emplace(A&&... a) noexcept(noexcept(c.emplace_back(std::forward<A>(a)...)) &&
                                std::is_nothrow_move_constructible_v<value_type> &&
                                std::is_nothrow_move_assignable_v<value_type>);
```

Constructs an element from `a...` at the end of the container, `c.emplace_back(a...)`, and sifts it up the heap
with `std::push_heap`.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments of the element's constructor |

## Return value

None, as in `std`: the new element need not be on top.

## Complexity

Logarithmic in the size: at most one comparison per level of the heap, plus the container's `emplace_back`
(amortized constant for `vector`).

## Exceptions

What the constructor of `T` throws, and its move constructor and move assignment while the element is sifted
up; none when they are noexcept.

If the construction of the element throws, the container's `emplace_back` leaves the priority queue as it was,
for `vector` and `deque`. If a move of the sift throws, the elements stay valid, in an order that may no longer
be a heap.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct ByValue {
    bool operator()(const tracked_ptr<int>& a, const tracked_ptr<int>& b) const noexcept {
        return *a < *b;
    }
};

int main() {
    priority_queue<tracked_ptr<int>, vector<tracked_ptr<int>>, ByValue> pq;
    pq.push(make_tracked<int>(3));
    pq.emplace(make_tracked<int>(7));
    println("{}", *pq.top());

    priority_queue<string> words;
    words.emplace(3, 'z');  // string(3, 'z') made in place
    words.emplace("apple");
    println("{}", words.top());
}
```

Output:

```text
7
zzz
```

## See also

- [push](push.md): inserts a value
- [sgcl::priority_queue\<T, Container, Compare\>](../priority_queue.md)
