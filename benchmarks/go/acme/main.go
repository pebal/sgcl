// The ACME client's counterparts in Go: golang.org/x/crypto/acme v0.31.0
// (Go's standard library has no ACME), a module of its own (go.mod written
// by compare.sh when missing, x/crypto from the module cache: GOFLAGS=-mod=mod
// GOPROXY=off), excluded from compare.sh's build of the other Go benchmarks
// as codec is. One case per run; prints one line, ns per operation, in the
// form of benchmarks/net/net.cpp (the SGCL side, `net acme_*`).
//
//	acme_order [n]  a full order of one name against `bench_net acme_server` at
//	                $SGCL_ACME_DIRECTORY (both sides' clients against the one
//	                server), the account registered once before the loop:
//	                AuthorizeOrder, GetAuthorization, the http-01 challenge
//	                accepted, WaitOrder, a P-256 key and its CSR, CreateOrderCert
//	                (finalize, the order polled, the chain fetched): per order
//	acme_jws [n]    one request's JWS as x/crypto/acme signs it, written out with
//	                the standard library (its jwsEncodeJSON is not exported): the
//	                protected header (alg, kid, nonce, url) by encoding/json, the
//	                payload {"csr":...} of 410 bytes, both base64url, ECDSA P-256
//	                over SHA-256 as R || S, the flattened JSON: per request
package main

import (
	"context"
	"crypto"
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/sha256"
	"crypto/x509"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"os"
	"strconv"
	"strings"
	"syscall"
	"time"

	"golang.org/x/crypto/acme"
)

func cpuSeconds() float64 {
	var ru syscall.Rusage
	syscall.Getrusage(syscall.RUSAGE_SELF, &ru)
	return float64(ru.Utime.Sec) + float64(ru.Utime.Usec)*1e-6 + float64(ru.Stime.Sec) + float64(ru.Stime.Usec)*1e-6
}

func report(what string, wall float64, ops float64, extra string) {
	fmt.Printf("net %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs%s\n", what, wall*1e9/ops, ops/wall, wall, cpuSeconds(), extra)
}

func order(ctx context.Context, c *acme.Client, i int64) bool {
	name := "host" + strconv.FormatInt(i, 10) + ".example.test"
	o, err := c.AuthorizeOrder(ctx, acme.DomainIDs(name))
	if err != nil {
		return false
	}
	z, err := c.GetAuthorization(ctx, o.AuthzURLs[0])
	if err != nil {
		return false
	}
	for _, ch := range z.Challenges {
		if ch.Type == "http-01" {
			if _, err := c.Accept(ctx, ch); err != nil {
				return false
			}
		}
	}
	o, err = c.WaitOrder(ctx, o.URI)
	if err != nil {
		return false
	}
	k, _ := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	csr, err := x509.CreateCertificateRequest(rand.Reader, &x509.CertificateRequest{DNSNames: []string{name}}, k)
	if err != nil {
		return false
	}
	der, _, err := c.CreateOrderCert(ctx, o.FinalizeURL, csr, true)
	return err == nil && len(der) == 2
}

type protected struct {
	Alg   string `json:"alg"`
	KID   string `json:"kid"`
	Nonce string `json:"nonce"`
	URL   string `json:"url"`
}

func jws(key *ecdsa.PrivateKey, kid, nonce, url string, payload []byte) []byte {
	h, _ := json.Marshal(protected{Alg: "ES256", KID: kid, Nonce: nonce, URL: url})
	phead := base64.RawURLEncoding.EncodeToString(h)
	pay := base64.RawURLEncoding.EncodeToString(payload)
	digest := sha256.Sum256([]byte(phead + "." + pay))
	r, s, err := ecdsa.Sign(rand.Reader, key, digest[:])
	if err != nil {
		panic(err)
	}
	sig := make([]byte, 64)
	r.FillBytes(sig[:32])
	s.FillBytes(sig[32:])
	out := struct {
		Protected string `json:"protected"`
		Payload   string `json:"payload"`
		Sig       string `json:"signature"`
	}{phead, pay, base64.RawURLEncoding.EncodeToString(sig)}
	b, _ := json.Marshal(&out)
	return b
}

func main() {
	if len(os.Args) < 2 {
		fmt.Fprintln(os.Stderr, "usage: acme <acme_order|acme_jws> [n]")
		os.Exit(2)
	}
	what := os.Args[1]
	var n int64
	if len(os.Args) > 2 {
		n, _ = strconv.ParseInt(os.Args[2], 10, 64)
	}
	ok := true
	switch what {
	case "acme_order":
		if n == 0 {
			n = 300
		}
		dir := os.Getenv("SGCL_ACME_DIRECTORY")
		if dir == "" {
			fmt.Fprintln(os.Stderr, "acme_order: SGCL_ACME_DIRECTORY (the URL of `bench_net acme_server`) is not set")
			os.Exit(2)
		}
		ctx := context.Background()
		key, _ := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
		c := &acme.Client{Key: crypto.Signer(key), DirectoryURL: dir}
		if _, err := c.Register(ctx, &acme.Account{}, acme.AcceptTOS); err != nil {
			fmt.Fprintln(os.Stderr, "acme_order:", err)
			os.Exit(1)
		}
		t0 := time.Now()
		var good int64
		for i := int64(0); i < n; i++ {
			if order(ctx, c, i) {
				good++
			}
		}
		report("acme_order", time.Since(t0).Seconds(), float64(n), "")
		ok = good == n
	case "acme_jws":
		if n == 0 {
			n = 50000
		}
		key, _ := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
		payload := []byte(`{"csr":"` + strings.Repeat("A", 400) + `"}`)
		check := 0
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			check += len(jws(key, "https://ca.example.test/acme/account/12345678", "Zm9vYmFyYmF6cXV4cXV1eA", "https://ca.example.test/acme/finalize/12345678/87654321", payload))
		}
		report("acme_jws", time.Since(t0).Seconds(), float64(n), "")
		ok = check > 0
	default:
		fmt.Fprintln(os.Stderr, "unknown case", what)
		os.Exit(2)
	}
	if !ok {
		os.Exit(1)
	}
}
