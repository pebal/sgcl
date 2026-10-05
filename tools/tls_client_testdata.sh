#!/bin/sh
# The client certificates of the mTLS tests (tests/net/tls_auth.cpp,
# tests/net/tls_interop.cpp): three leaves for the client authentication
# (extendedKeyUsage clientAuth) signed by the CA of tools/tls_testdata.sh —
# ECDSA P-256, Ed25519 and RSA 2048 — and a second CA with a client leaf of
# its own (ECDSA P-256), which a server that trusts only the first refuses.
# Each key in PKCS #8. Test keys only: they guard nothing. From the root of
# the tree, after tools/tls_testdata.sh:
#
#     sh tools/tls_client_testdata.sh tests/net/tls_testdata
set -e
out=${1:-tests/net/tls_testdata}
openssl=${OPENSSL:-/opt/homebrew/opt/openssl@3/bin/openssl}
cd "$out"
printf "basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=clientAuth\n" > client.ext
for kind in ecdsa ed25519 rsa; do
    case $kind in
        ed25519) "$openssl" genpkey -algorithm ED25519 -out client_$kind.key ;;
        ecdsa) "$openssl" genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-256 -out client_$kind.key ;;
        rsa) "$openssl" genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out client_$kind.key ;;
    esac
    "$openssl" req -new -key client_$kind.key -subj "/CN=sgcl test client $kind" -out client_$kind.csr
    "$openssl" x509 -req -in client_$kind.csr -CA ca.pem -CAkey ca.key -CAcreateserial -days 36500 -sha256 -extfile client.ext -out client_$kind.pem
    rm client_$kind.csr
done
"$openssl" genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-256 -out other_ca.key
"$openssl" req -x509 -new -key other_ca.key -sha256 -days 36500 -subj "/CN=sgcl other test CA" \
    -addext "basicConstraints=critical,CA:TRUE" -addext "keyUsage=critical,keyCertSign,cRLSign" -out other_ca.pem
"$openssl" genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-256 -out other_client.key
"$openssl" req -new -key other_client.key -subj "/CN=sgcl other test client" -out other_client.csr
"$openssl" x509 -req -in other_client.csr -CA other_ca.pem -CAkey other_ca.key -CAcreateserial -days 36500 -sha256 -extfile client.ext -out other_client.pem
rm -f other_client.csr client.ext ca.srl other_ca.srl
