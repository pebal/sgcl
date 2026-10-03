[sgcl](../../README.md) › [core](../README.md) › [priority_queue](README.md)

# sgcl::priority_queue\<T, Container, Compare\>::priority_queue

```cpp
priority_queue()                                                                               // (1)
    noexcept(std::is_nothrow_default_constructible_v<Container> &&
             std::is_nothrow_default_constructible_v<Compare>);
explicit priority_queue(const Compare& compare)                                                // (2)
    noexcept(std::is_nothrow_default_constructible_v<Container> &&
             std::is_nothrow_copy_constructible_v<Compare>);
priority_queue(const Compare& compare, const Container& cont)                                  // (3)
    noexcept(std::is_nothrow_copy_constructible_v<Container> &&
             std::is_nothrow_copy_constructible_v<Compare> &&
             std::is_nothrow_move_constructible_v<value_type> &&
             std::is_nothrow_move_assignable_v<value_type>);
priority_queue(const Compare& compare, Container&& cont)                                       // (4)
    noexcept(std::is_nothrow_move_constructible_v<Container> &&
             std::is_nothrow_copy_constructible_v<Compare> &&
             std::is_nothrow_move_constructible_v<value_type> &&
             std::is_nothrow_move_assignable_v<value_type>);
template<std::input_iterator InputIt>
priority_queue(InputIt first, InputIt last, const Compare& compare = Compare());               // (5)
template<std::input_iterator InputIt>
priority_queue(InputIt first, InputIt last, const Compare& compare, const Container& cont);    // (6)
template<std::input_iterator InputIt>
priority_queue(InputIt first, InputIt last, const Compare& compare, Container&& cont);         // (7)
priority_queue(const priority_queue& other);                                                   // (8)
priority_queue(priority_queue&& other);                                                        // (9)
```

Constructs a priority queue from one of the sources below. (3)–(7) then make the container a heap under
`compare` with `std::make_heap`; the empty container of (1) and (2) is a heap already, and no element is moved.

1. An empty priority queue, `Compare()` its order.
2. An empty priority queue, ordered by a copy of `compare`.
3. A priority queue over a copy of `cont`.
4. A priority queue over `cont` itself, moved in; `cont` is left as the container's move leaves it (an empty
   `vector` or `deque`).
5. A priority queue whose container is built from the range `[first, last)`.
6. A priority queue over a copy of `cont`, with the elements of the range `[first, last)` appended.
7. A priority queue over `cont` itself, moved in, with the elements of the range `[first, last)` appended.
8. A copy of `other`: its container and its comparator copied.
9. Takes the container of `other` over, by the container's move, and moves its comparator.

(8) and (9) are the implicit ones: noexcept as the copy and the move of the container and the comparator are.

## Parameters

| Parameter | Description |
|---|---|
| `compare` | the comparator that orders the heap |
| `cont` | the container the priority queue is made of |
| `first`, `last` | the range the elements are copied from |
| `other` | the priority queue the container is copied or taken from |

## Complexity

- (1–2) Constant.
- (3–4) Linear in `cont.size()`: the heap is made with at most `3 × cont.size()` comparisons.
- (5) Linear in the distance between `first` and `last`.
- (6–7) Linear in `cont.size()` and the distance between `first` and `last`.
- (8) Linear in `other.size()`.
- (9) Constant for `vector` and `deque`.

## Exceptions

- (1–2) What the construction of `Container` (empty) and of `Compare` throws; none for `vector`, `deque` and the
  function objects of `std`.
- (3), (6), (8) What the copy constructor of `T` throws, in the copy of the container.
- (5–7) What the constructor of `T` throws, and `length_error` from `vector` when the elements would pass its
  `max_size()`.
- (3–7) What the move constructor and the move assignment of `T` throw, while the heap is made.
- (4), (7), (9) What the move constructor of `Container` throws; none for `vector` and `deque`.

When an element's constructor throws, the elements built before it are destroyed and the exception propagates.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <functional>

using namespace sgcl;

int main() {
    vector values = {5, 1, 4, 1, 3};
    priority_queue max_heap(values.begin(), values.end());  // deduced: priority_queue<int>
    priority_queue<int, vector<int>, std::greater<int>> min_heap(values.begin(), values.end());
    priority_queue<int, deque<int>> on_deque(std::less<int>(), deque<int>{2, 9, 4});
    priority_queue by_greater(std::greater<int>(), vector{7, 3, 8});  // the comparator first
    println("{} {} {} {}", max_heap.top(), min_heap.top(), on_deque.top(), by_greater.top());

    priority_queue<int> empty;
    priority_queue copy = max_heap;
    copy.pop();
    println("{} {} {}", empty.empty(), max_heap.top(), copy.top());
}
```

Output:

```text
5 1 9 3
true 5 4
```

## See also

- [operator=](operator_assign.md): assigns the contents
- [push](push.md): inserts an element
- [sgcl::priority_queue\<T, Container, Compare\>](README.md)
