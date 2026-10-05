// Go's golang.org/x/crypto/acme client against sgcl's acme::test_server,
// the interop oracle of tests/net/acme/go_client.cpp: an account (with an
// external account binding when a key id is given), an order of a name and
// a wildcard, each authorization's first challenge accepted (the server
// validates nothing: skip_validation), the order finalized, the chain and
// its alternates fetched, the certificate revoked, the account's key rolled
// over, read, updated and deactivated. One line per step on stdout, "done"
// at the end. With the mode "values" it prints, for the account key read
// from a PEM file and the token given, what x/crypto computes of a
// challenge: the http-01 response, the dns-01 record, the acmeIdentifier of
// tls-alpn-01's certificate. Built by the test in a module of its own (go.mod
// written there, golang.org/x/crypto v0.31.0 from the module cache).
package main

import (
	"context"
	"crypto"
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/asn1"
	"encoding/base64"
	"encoding/hex"
	"encoding/pem"
	"fmt"
	"net"
	"os"
	"time"

	"golang.org/x/crypto/acme"
)

func fail(step string, err error) {
	fmt.Printf("fail %s: %v\n", step, err)
	os.Exit(1)
}

func values(keyFile, token string) {
	b, err := os.ReadFile(keyFile)
	if err != nil {
		fail("read key", err)
	}
	block, _ := pem.Decode(b)
	k, err := x509.ParsePKCS8PrivateKey(block.Bytes)
	if err != nil {
		fail("parse key", err)
	}
	c := &acme.Client{Key: k.(crypto.Signer)}
	h, _ := c.HTTP01ChallengeResponse(token)
	d, _ := c.DNS01ChallengeRecord(token)
	fmt.Println("http01", h)
	fmt.Println("dns01", d)
	fmt.Println("path", c.HTTP01ChallengePath(token))
	cert, err := c.TLSALPN01ChallengeCert(token, "example.test")
	if err != nil {
		fail("alpn", err)
	}
	leaf, err := x509.ParseCertificate(cert.Certificate[0])
	if err != nil {
		fail("alpn parse", err)
	}
	for _, e := range leaf.Extensions {
		if e.Id.Equal(asn1.ObjectIdentifier{1, 3, 6, 1, 5, 5, 7, 1, 31}) {
			fmt.Println("alpn", e.Critical, hex.EncodeToString(e.Value), leaf.DNSNames[0])
		}
	}
}

func main() {
	if len(os.Args) == 4 && os.Args[1] == "values" {
		values(os.Args[2], os.Args[3])
		return
	}
	if len(os.Args) < 2 {
		fail("args", fmt.Errorf("usage: go_acme <directory URL> [eab kid] [eab key]"))
	}
	ctx, cancel := context.WithTimeout(context.Background(), 60*time.Second)
	defer cancel()
	key, _ := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	c := &acme.Client{Key: key, DirectoryURL: os.Args[1]}
	dir, err := c.Discover(ctx)
	if err != nil {
		fail("discover", err)
	}
	fmt.Println("ok discover", dir.Terms != "")
	acct := &acme.Account{Contact: []string{"mailto:go@example.test"}}
	if len(os.Args) == 4 {
		mac, err := base64.RawURLEncoding.DecodeString(os.Args[3])
		if err != nil {
			fail("eab key", err)
		}
		acct.ExternalAccountBinding = &acme.ExternalAccountBinding{KID: os.Args[2], Key: mac}
	}
	a, err := c.Register(ctx, acct, acme.AcceptTOS)
	if err != nil {
		fail("register", err)
	}
	fmt.Println("ok register", a.Status)
	o, err := c.AuthorizeOrder(ctx, []acme.AuthzID{{Type: "dns", Value: "example.test"}, {Type: "dns", Value: "*.example.test"}, {Type: "ip", Value: "127.0.0.1"}})
	if err != nil {
		fail("order", err)
	}
	fmt.Println("ok order", o.Status, len(o.AuthzURLs))
	for _, u := range o.AuthzURLs {
		z, err := c.GetAuthorization(ctx, u)
		if err != nil {
			fail("authorization", err)
		}
		if _, err := c.Accept(ctx, z.Challenges[0]); err != nil {
			fail("accept", err)
		}
		if _, err := c.WaitAuthorization(ctx, u); err != nil {
			fail("wait authorization", err)
		}
	}
	fmt.Println("ok authorizations")
	o, err = c.WaitOrder(ctx, o.URI)
	if err != nil {
		fail("wait order", err)
	}
	fmt.Println("ok ready", o.Status)
	ck, _ := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	ip := []byte{127, 0, 0, 1}
	csr, err := x509.CreateCertificateRequest(rand.Reader, &x509.CertificateRequest{Subject: pkix.Name{CommonName: "example.test"}, DNSNames: []string{"example.test", "*.example.test"}, IPAddresses: []net.IP{ip}}, ck)
	if err != nil {
		fail("csr", err)
	}
	der, certURL, err := c.CreateOrderCert(ctx, o.FinalizeURL, csr, true)
	if err != nil {
		fail("finalize", err)
	}
	leaf, err := x509.ParseCertificate(der[0])
	if err != nil {
		fail("parse", err)
	}
	fmt.Println("ok certificate", len(der), leaf.DNSNames[0], len(leaf.IPAddresses))
	alts, err := c.ListCertAlternates(ctx, certURL)
	if err != nil {
		fail("alternates", err)
	}
	for _, alt := range alts {
		chain, err := c.FetchCert(ctx, alt, true)
		if err != nil || len(chain) != 2 {
			fail("alternate", err)
		}
	}
	fmt.Println("ok alternates", len(alts))
	if err := c.RevokeCert(ctx, nil, der[0], acme.CRLReasonSuperseded); err != nil {
		fail("revoke", err)
	}
	fmt.Println("ok revoke")
	next, _ := ecdsa.GenerateKey(elliptic.P384(), rand.Reader)
	if err := c.AccountKeyRollover(ctx, next); err != nil {
		fail("rollover", err)
	}
	fmt.Println("ok rollover")
	got, err := c.GetReg(ctx, "")
	if err != nil {
		fail("get", err)
	}
	got.Contact = []string{"mailto:other@example.test"}
	up, err := c.UpdateReg(ctx, got)
	if err != nil {
		fail("update", err)
	}
	fmt.Println("ok update", up.Contact[0])
	if err := c.DeactivateReg(ctx); err != nil {
		fail("deactivate", err)
	}
	fmt.Println("ok deactivate")
	fmt.Println("done")
}
