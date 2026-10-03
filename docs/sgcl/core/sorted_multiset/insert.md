[sgcl](../../README.md) › [core](../README.md) › [sorted_multiset](../sorted_multiset.md)

# sgcl::sorted_multiset\<Key, Compare\>::insert

```cpp
/*(1)*/ iterator insert(const value_type& value)
            noexcept(std::is_nothrow_copy_constructible_v<value_type>);
/*(2)*/ iterator insert(value_type&& value)
            noexcept(std::is_nothrow_move_constructible_v<value_type>);
/*(3)*/ iterator insert(const_iterator hint, const value_type& value)
            noexcept(std::is_nothrow_copy_constructible_v<value_type>);
/*(4)*/ iterator insert(const_iterator hint, value_type&& value)
            noexcept(std::is_nothrow_move_constructible_v<value_type>);
/*(5)*/ template<std::input_iterator InputIt>
        void insert(InputIt first, InputIt last);
/*(6)*/ void insert(std::initializer_list<value_type> ilist);
/*(7)*/ iterator insert(node_type&& nh) noexcept;
/*(8)*/ iterator insert(const_iterator hint, node_type&& nh) noexcept;
```

Inserts elements, as `std::multiset::insert` does: every element goes in, an equivalent key or not.

1. Inserts a copy of `value`, after the elements with an equivalent key.
2. Inserts `value`, moved, after the elements with an equivalent key.
3. Inserts a copy of `value` as close to `hint` as the order allows.
4. Inserts `value`, moved, as close to `hint` as the order allows.
5. Inserts the elements of the range `[first, last)`, one by one, with `end()` as the hint.
6. Inserts the elements of `ilist`, as (5).
7. Links the node `nh` owns into the tree, after the elements with an equivalent key: the element is neither copied
   nor moved.
8. The same, as close to `hint` as the order allows.

- (3–4), (8) The element goes right before `hint` when its key fits there, and right after the largest element
  when `hint` is `end()` and the key is not less than it: an append in sorted order at `end()` costs one
  comparison. Otherwise the key is searched from the root, and the element goes after its equivalent keys when
  `hint` is after them, before them when `hint` is before them.
- (5–6) Equivalent keys keep the order of the range. An element of the type `Key` is compared where it is and
  copied into its node once its place is known. An element of another type (a `string_view` for `string` keys) is
  converted once, into a new node, and the node's key compared, as [emplace_hint](emplace_hint.md) does: the
  source need not be comparable with the keys, and no temporary key is built per comparison.
- (7–8) An empty `nh` inserts nothing; otherwise `nh` is empty afterwards.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the element to insert |
| `hint` | an iterator to the element before which the new one is expected, or `end()` |
| `first`, `last` | the range of the elements to insert |
| `ilist` | the list of the elements to insert |
| `nh` | a node handle, from [extract](extract.md) of a sorted_multiset or a sorted_set of the same `Key`, whatever its `Compare` |

## Return value

- (1–4) An iterator to the inserted element.
- (5–6) None.
- (7–8) An iterator to the inserted element, or `end()` for an empty `nh`.

## Complexity

- (1–2), (7) Logarithmic in the size of the multiset.
- (3–4), (8) Amortized constant when the element goes right before `hint`, logarithmic otherwise.
- (5–6) *N* log(*size* + *N*) for *N* elements; linear in *N* when they come sorted.

## Exceptions

- (1–4) What the copy or the move of `Key` throws; none when it is noexcept.
- (5–6) What the construction of an element from `*first` (from an element of `ilist`) throws.
- (7–8) None.

If an exception is thrown, nothing is inserted by that element: (1–4) leave the multiset as it was, (5–6) keep the
elements inserted before it.

## Notes

An insertion stores the tracked pointers of the links it changes and pays the write barrier on each, the
rebalancing included; the search before it reads raw pointers only. No iterator is invalidated.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Task {
    int priority;
    string name;
};

struct ByPriority {
    bool operator()(const Task& a, const Task& b) const noexcept {
        return a.priority < b.priority;
    }
};

int main() {
    sorted_multiset<Task, ByPriority> tasks;
    tasks.insert({2, "test"});
    tasks.insert({1, "build"});
    tasks.insert({2, "lint"});  // after "test"
    tasks.insert(tasks.begin(), {2, "format"});  // the hint is before the 2s: the first of them
    tasks.insert(tasks.end(), {3, "deploy"});  // an append: one comparison

    vector<string> names;
    for (const auto& task : tasks) {
        names.push_back(task.name);
    }
    println("{}", names);

    sorted_multiset<Task, ByPriority> later = {{2, "review"}};
    auto it = tasks.insert(later.extract(later.begin()));  // relinked after "lint", nothing copied
    println("{} {} {}", it->name, tasks.size(), later.empty());
}
```

Output:

```text
["build", "format", "test", "lint", "deploy"]
review 6 true
```

## See also

- [emplace](emplace.md): constructs the element in place
- [extract](extract.md): takes a node out of a multiset
- [merge](merge.md): relinks the nodes of another multiset
- [sgcl::sorted_multiset\<Key, Compare\>](../sorted_multiset.md)
