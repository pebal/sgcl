[sgcl](../../README.md) › [core](../README.md) › [list](README.md)

# sgcl::list\<T\>::sort

```cpp
void sort() noexcept(/* see below */);                                              // (1)
template<class Compare>
void sort(Compare comp) noexcept(std::is_nothrow_invocable_v<Compare&, T&, T&>);    // (2)
```

Sorts the elements, stable: equal elements keep their order.

1. By `<`.
2. By `comp`, which returns `true` when its first argument goes before the second.

A merge sort in place: the nodes are relinked within the list, never detached, and no element is moved or copied,
so iterators and references stay valid, naming the same elements at their new places. It hides the sorts of
[mixin::ordered](../mixin/ordered/README.md), which need random access; `sort_by` and `stable_sort` are not a list's.

## Parameters

| Parameter | Description |
|---|---|
| `comp` | the order of the elements, `bool comp(const T&, const T&)` |

## Return value

None.

## Complexity

`N log N` comparisons, where `N` is `size()`.

## Exceptions

- (1) What `<` of the elements throws; none when it is noexcept, as for `int`.
- (2) What `comp` throws; none when its call is noexcept.

If an exception is thrown, the list holds the same elements, in an order the sort had reached.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Task {
    int priority;
    string name;
};

int main() {
    list l = {3, 1, 2};
    l.sort();
    println("{}", l);

    l.sort(std::greater<>());
    println("{}", l);

    list<Task> tasks = {{2, "write"}, {1, "read"}, {2, "test"}, {1, "plan"}};
    tasks.sort([](const Task& a, const Task& b) { return a.priority < b.priority; });
    for (const Task& t : tasks) {
        println("{} {}", t.priority, t.name);  // equal priorities in the order they came
    }
}
```

Output:

```text
[1, 2, 3]
[3, 2, 1]
1 read
1 plan
2 write
2 test
```

## See also

- [merge](merge.md): merges two sorted lists
- [unique](unique.md): erases consecutive equal elements
- [reverse](reverse.md): reverses the order of the nodes
- [sgcl::list\<T\>](README.md)
