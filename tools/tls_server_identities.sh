#!/bin/sh
# The identities of the server's tests (tests/net/tls_server.cpp): a key of
# each kind the module signs with and a self-signed certificate for it,
# made by OpenSSL 3 and written as tests/net/tls_server_identities.h (the
# key as PKCS #8 DER, the certificate as DER, both hex):
#
#   tools/tls_server_identities.sh > tests/net/tls_server_identities.h
#
# ed25519, P-256 and P-384 for "example.test", RSA 2048 for "example.test"
# and "*.example.test", a second P-256 for "other.test" (the choice by
# SNI), and RSA 1024 for "example.test" (too small for RSA-PSS under
# SHA-512). Valid from 2026 for 100 years.
set -e
openssl=${OPENSSL:-/opt/homebrew/opt/openssl@3/bin/openssl}
dir=$(mktemp -d)
trap 'rm -rf "$dir"' EXIT
make() {
    name=$1 alg=$2 opts=$3 san=$4
    "$openssl" genpkey -algorithm "$alg" $opts -out "$dir/$name.key" 2>/dev/null
    "$openssl" req -x509 -new -key "$dir/$name.key" -subj "/CN=${san%%,*}" -days 36500 \
        -addext "subjectAltName=$(echo "$san" | sed 's/\([^,]*\)/DNS:\1/g')" \
        -addext "basicConstraints=critical,CA:FALSE" -addext "keyUsage=critical,digitalSignature" \
        -out "$dir/$name.pem" 2>/dev/null
    "$openssl" pkcs8 -topk8 -nocrypt -in "$dir/$name.key" -outform DER -out "$dir/$name.key.der"
    "$openssl" x509 -in "$dir/$name.pem" -outform DER -out "$dir/$name.cert.der"
    printf '    {"%s", "%s", "%s"},\n' "$name" "$(xxd -p "$dir/$name.key.der" | tr -d '\n')" "$(xxd -p "$dir/$name.cert.der" | tr -d '\n')"
}
cat <<'EOF'
// Made by tools/tls_server_identities.sh (OpenSSL 3): do not edit.
#pragma once

namespace tls_identities {
    struct Identity {
        const char *name, *key, *certificate;   // PKCS #8 DER, the certificate's DER; hex
    };

    inline const Identity all[] = {
EOF
make ed25519 ED25519 "" "example.test"
make p256 EC "-pkeyopt ec_paramgen_curve:P-256" "example.test"
make p384 EC "-pkeyopt ec_paramgen_curve:P-384" "example.test"
make rsa RSA "-pkeyopt rsa_keygen_bits:2048" "example.test,*.example.test"
make other EC "-pkeyopt ec_paramgen_curve:P-256" "other.test"
make rsa1024 RSA "-pkeyopt rsa_keygen_bits:1024" "example.test"
cat <<'EOF'
    };
}
EOF
