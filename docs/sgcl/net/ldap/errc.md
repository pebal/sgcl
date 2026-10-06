[sgcl](../../README.md) › [net](../README.md) › [ldap](README.md)

# sgcl::net::ldap::errc

```cpp
#include "sgcl/net/ldap/error.h"   // or "sgcl/net/ldap.h"

namespace sgcl::net::ldap {
    enum class errc {
        operations_error = 1,
        protocol_error = 2,
        time_limit_exceeded = 3,
        size_limit_exceeded = 4,
        auth_method_not_supported = 7,
        stronger_auth_required = 8,
        referral = 10,
        admin_limit_exceeded = 11,
        unavailable_critical_extension = 12,
        confidentiality_required = 13,
        sasl_bind_in_progress = 14,
        no_such_attribute = 16,
        undefined_attribute_type = 17,
        inappropriate_matching = 18,
        constraint_violation = 19,
        attribute_or_value_exists = 20,
        invalid_attribute_syntax = 21,
        no_such_object = 32,
        alias_problem = 33,
        invalid_dn_syntax = 34,
        alias_dereferencing_problem = 36,
        inappropriate_authentication = 48,
        invalid_credentials = 49,
        insufficient_access_rights = 50,
        busy = 51,
        unavailable = 52,
        unwilling_to_perform = 53,
        loop_detect = 54,
        naming_violation = 64,
        object_class_violation = 65,
        not_allowed_on_non_leaf = 66,
        not_allowed_on_rdn = 67,
        entry_already_exists = 68,
        object_class_mods_prohibited = 69,
        affects_multiple_dsas = 71,
        other = 80,
        malformed = 1000,
        invalid_filter
    };
}
```

The result codes of LDAP (RFC 4511 Appendix A) by their values, and the client's own failures past them, in the
category `"ldap"` ([category](category.md)). An error's path is the server's diagnostic message;
[result_of](result_of.md) gives the matched DN and the referrals beside it. Success (0), compareFalse (5),
compareTrue (6) and saslBindInProgress of a step the client takes are answers, not errors.

| Value | Description |
|---|---|
| `operations_error` | 1, "operations error": the server could not do the operation in the state it is in |
| `protocol_error` | 2, "protocol error": a message the server could not read, an operation it does not know |
| `time_limit_exceeded` | 3, "time limit exceeded": a search past its time limit (a search gives its entries with `truncated`) |
| `size_limit_exceeded` | 4, "size limit exceeded": a search past its size limit (a search gives its entries with `truncated`) |
| `auth_method_not_supported` | 7, "authentication method not supported": a SASL mechanism the server does not offer |
| `stronger_auth_required` | 8, "stronger authentication required": the operation needs a stronger bind |
| `referral` | 10, "referral": another server holds the entry: the URLs in [result_of](result_of.md) |
| `admin_limit_exceeded` | 11, "administrative limit exceeded": a limit the server sets |
| `unavailable_critical_extension` | 12, "unavailable critical extension": a critical control the server does not know |
| `confidentiality_required` | 13, "confidentiality required": the operation needs TLS |
| `sasl_bind_in_progress` | 14, "SASL bind in progress": a SASL step this client does not take |
| `no_such_attribute` | 16, "no such attribute": a value removed that is not there |
| `undefined_attribute_type` | 17, "undefined attribute type": an attribute the schema does not have |
| `inappropriate_matching` | 18, "inappropriate matching": a matching rule the attribute has none of |
| `constraint_violation` | 19, "constraint violation": a value the server refuses (a password too short) |
| `attribute_or_value_exists` | 20, "attribute or value exists": a value added that is there |
| `invalid_attribute_syntax` | 21, "invalid attribute syntax": a value of the wrong syntax |
| `no_such_object` | 32, "no such object": the entry is not there: the matched DN in [result_of](result_of.md) |
| `alias_problem` | 33, "alias problem": an alias that names no entry |
| `invalid_dn_syntax` | 34, "invalid DN syntax": a DN that does not read |
| `alias_dereferencing_problem` | 36, "alias dereferencing problem": an alias that may not be followed |
| `inappropriate_authentication` | 48, "inappropriate authentication": a bind of a kind the entry cannot take |
| `invalid_credentials` | 49, "invalid credentials": a DN or a password refused |
| `insufficient_access_rights` | 50, "insufficient access rights": the identity may not do it |
| `busy` | 51, "busy": the server is too busy |
| `unavailable` | 52, "unavailable": the server is going away (a Notice of Disconnection) |
| `unwilling_to_perform` | 53, "unwilling to perform": the server will not do it |
| `loop_detect` | 54, "loop detected": aliases or referrals in a loop |
| `naming_violation` | 64, "naming violation": a DN the server's structure rules refuse |
| `object_class_violation` | 65, "object class violation": attributes the object classes do not allow, or lack |
| `not_allowed_on_non_leaf` | 66, "not allowed on non-leaf": an entry with children removed or renamed |
| `not_allowed_on_rdn` | 67, "not allowed on RDN": the RDN's value removed |
| `entry_already_exists` | 68, "entry already exists": an entry added or renamed to a DN taken |
| `object_class_mods_prohibited` | 69, "object class modifications prohibited": an entry's structural class changed |
| `affects_multiple_dsas` | 71, "affects multiple DSAs": an operation across servers |
| `other` | 80, "other": any other failure of the server |
| `malformed` | 1000, "malformed LDAP message": a message of the server's that breaks RFC 4511, which ends the session |
| `invalid_filter` | 1001, "invalid LDAP filter": a filter's text that breaks RFC 4515, refused before anything is sent |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ldap.h"

using namespace sgcl;

int main() {
    net::ldap::client::options o;
    o.security = net::ldap::security::none;  // slapd on this machine, no TLS
    net::ldap::client c = net::ldap::client::connect("ldap://localhost:3890", o).value();
    auto r = c.bind("uid=alice,ou=people,dc=example,dc=com", "wrong");
    println("{}", r.error().code() == net::ldap::errc::invalid_credentials);
    println("{}", r.error().code().message());
}
```

Output:

```text
true
invalid credentials
```

## See also

- [result_of](result_of.md)
- [category](category.md)
