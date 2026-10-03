[sgcl](../../README.md) › [core](../README.md) › [variant](../variant.md)

# sgcl::variant\<Ts...\>::emplace

```cpp
/*(1)*/ template<class T, class... A>
        requires std::is_constructible_v<T, A...>
        T& emplace(A&&... a) noexcept(std::is_nothrow_constructible_v<T, A...>);
/*(2)*/ template<class T, class U, class... A>
        requires std::is_constructible_v<T, std::initializer_list<U>&, A...>
        T& emplace(std::initializer_list<U> il, A&&... a)
            noexcept(std::is_nothrow_constructible_v<T, std::initializer_list<U>&, A...>);
/*(3)*/ template<size_t I, class... A>
        requires std::is_constructible_v<T_I, A...>
        T_I& emplace(A&&... a) noexcept(std::is_nothrow_constructible_v<T_I, A...>);
/*(4)*/ template<size_t I, class U, class... A>
        requires std::is_constructible_v<T_I, std::initializer_list<U>&, A...>
        T_I& emplace(std::initializer_list<U> il, A&&... a)
            noexcept(std::is_nothrow_constructible_v<T_I, std::initializer_list<U>&, A...>);
```

Destroys the alternative held, if any, and constructs a new one in its place. `T_I` is the alternative at index
`I`, `variant_alternative_t<I, variant>`.

1. The alternative `T`, from `a...`. Takes part only when `T` is exactly one of `Ts`.
2. The same, from `il` and `a...`.
3. The alternative at index `I`, from `a...`. Takes part only when `I` is below the number of alternatives.
4. The same, from `il` and `a...`.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments the alternative is constructed from |
| `il` | the initializer list the alternative is constructed from |

## Return value

A reference to the new alternative.

## Complexity

Constant, plus the destruction of the old alternative and the construction of the new one.

## Exceptions

What the constructor of the new alternative throws; none when it is noexcept.

The old alternative is destroyed first: if the construction throws, the variant is valueless
([valueless_by_exception](valueless_by_exception.md)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    variant<int, string, vector<int>> v;
    string& s = v.emplace<string>(3, 'a');
    println("{} {}", v.index(), s);

    vector<int>& numbers = v.emplace<2>({1, 2});
    numbers.push_back(3);
    println("{} {}", v.index(), get<2>(v));
}
```

Output:

```text
1 aaa
2 [1, 2, 3]
```

## See also

- [operator=](operator_assign.md): assigns a value made beforehand
- [(constructor)](variant.md): constructs the variant with an alternative in place
- [sgcl::variant\<Ts...\>](../variant.md)
