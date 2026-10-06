[sgcl](../../README.md) › [net](../README.md) › [amqp](README.md)

# sgcl::net::amqp::find

```cpp
optional<field> find(const table& t, const string& name);
```

The value of a name in a field table, the first when it is there more than once.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the table |
| `name` | the name, compared as it is |


## Return value

The value; none for a name the table lacks.

## Complexity

Linear in the table.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    net::amqp::table headers{{"attempt", net::amqp::field(3)}, {"source", net::amqp::field("web")}};
    println("{} {}", *net::amqp::find(headers, "attempt")->as_int(), bool(net::amqp::find(headers, "missing")));
}
```

Output:

```text
3 false
```

## See also

- [field](field/README.md)
- [properties](properties.md)
