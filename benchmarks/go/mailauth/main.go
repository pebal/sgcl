// DKIM in Go's standard library alone: Go has no DKIM (golang.org/x has
// none either), so this side is a minimal signer and verifier written by
// hand from RFC 6376 — relaxed canonicalization of the head and the body,
// SHA-256, crypto/rsa's PKCS #1 v1.5 and crypto/ed25519 — the same work
// net::dkim does without its DNS lookup (benchmarks/net/mail.cpp has the
// SGCL side, the same cases). Prints one line, ns per operation.
//
//	dkim_sign_rsa [n]       a message of 4 KB signed, relaxed/relaxed, RSA 2048: per message
//	dkim_sign_ed25519 [n]   the same with Ed25519: per message
//	dkim_verify_rsa [n]     its signature field parsed, the key's record read (base64, the
//	                        SubjectPublicKeyInfo), the body and the head hashed again, the
//	                        signature verified: per message (no DNS)
//	dkim_verify_ed25519 [n] the same with Ed25519: per message
//	dkim_body [n]           a body of 1 MB canonicalized (relaxed) into SHA-256: per body
package main

import (
	"bytes"
	"crypto"
	"crypto/ed25519"
	"crypto/rand"
	"crypto/rsa"
	"crypto/sha256"
	"crypto/x509"
	"encoding/base64"
	"fmt"
	"hash"
	"os"
	"strconv"
	"strings"
	"time"
)

func report(what string, d time.Duration, n int) {
	ns := float64(d.Nanoseconds()) / float64(n)
	fmt.Printf("mail %s ns/op=%.1f ops/s=%.0f wall=%.2fs\n", what, ns, float64(n)/d.Seconds(), d.Seconds())
}

func count(i, def int) int {
	if len(os.Args) > i {
		if n, err := strconv.Atoi(os.Args[i]); err == nil {
			return n
		}
	}
	return def
}

// The message both sides sign: the same bytes
func message() []byte {
	var b bytes.Buffer
	b.WriteString("From: Alice <alice@example.com>\r\nTo: Bob <bob@example.org>\r\nSubject: Quarterly  report\r\n")
	b.WriteString("Date: Tue, 06 Oct 2026 12:00:00 +0000\r\nMessage-ID: <bench@example.com>\r\n")
	b.WriteString("MIME-Version: 1.0\r\nContent-Type: text/plain; charset=utf-8\r\n\r\n")
	for b.Len() < 4096 {
		b.WriteString("The quick brown fox  jumps over\tthe lazy dog, line after line.  \r\n")
	}
	b.WriteString("\r\n\r\n")
	return b.Bytes()
}

func bigBody() []byte {
	var b bytes.Buffer
	for b.Len() < 1<<20 {
		b.WriteString("The quick brown fox  jumps over\tthe lazy dog, line after line.  \r\n")
	}
	return b.Bytes()
}

func isWSP(c byte) bool { return c == ' ' || c == '\t' }

// relaxed body canonicalization (RFC 6376 §3.4.4) into h
func bodyHash(h hash.Hash, body []byte) {
	empty := 0
	var line []byte
	for len(body) > 0 {
		i := bytes.Index(body, []byte("\r\n"))
		var l []byte
		if i < 0 {
			l, body = body, nil
		} else {
			l, body = body[:i], body[i+2:]
		}
		for len(l) > 0 && isWSP(l[len(l)-1]) {
			l = l[:len(l)-1]
		}
		if len(l) == 0 {
			empty++
			continue
		}
		for ; empty > 0; empty-- {
			h.Write([]byte("\r\n"))
		}
		line = line[:0]
		sp := false
		for _, c := range l {
			if isWSP(c) {
				sp = true
				continue
			}
			if sp {
				line = append(line, ' ')
				sp = false
			}
			line = append(line, c)
		}
		line = append(line, '\r', '\n')
		h.Write(line)
	}
}

type field struct {
	name string
	raw  []byte
}

func split(m []byte) ([]field, []byte) {
	var fields []field
	for len(m) > 0 {
		if bytes.HasPrefix(m, []byte("\r\n")) {
			return fields, m[2:]
		}
		end := 0
		for {
			i := bytes.Index(m[end:], []byte("\r\n"))
			if i < 0 {
				end = len(m)
				break
			}
			end += i + 2
			if end >= len(m) || !isWSP(m[end]) {
				break
			}
		}
		colon := bytes.IndexByte(m[:end], ':')
		fields = append(fields, field{strings.TrimRight(string(m[:colon]), " \t"), m[:end]})
		m = m[end:]
	}
	return fields, nil
}

// relaxed header canonicalization (§3.4.2)
func relaxedField(out []byte, f field) []byte {
	out = append(out, strings.ToLower(f.name)...)
	out = append(out, ':')
	v := f.raw[bytes.IndexByte(f.raw, ':')+1:]
	sp, any := false, false
	for _, c := range v {
		if c == '\r' || c == '\n' {
			continue
		}
		if isWSP(c) {
			sp = true
			continue
		}
		if sp && any {
			out = append(out, ' ')
		}
		sp, any = false, true
		out = append(out, c)
	}
	return append(out, '\r', '\n')
}

func headData(fields []field, names []string, own []byte) []byte {
	var data []byte
	next := map[string]int{}
	for _, n := range names {
		ln := strings.ToLower(n)
		from, ok := next[ln]
		if !ok {
			from = len(fields)
		}
		for i := from - 1; i >= 0; i-- {
			if strings.EqualFold(fields[i].name, n) {
				data = relaxedField(data, fields[i])
				from = i
				break
			}
			if i == 0 {
				from = 0
			}
		}
		next[ln] = from
	}
	// the signature's own field without b='s value
	s := string(own)
	if i := strings.Index(s, "b="); i >= 0 {
		j := strings.LastIndex(s, "; b=")
		if j >= 0 {
			i = j + 4
		} else {
			i += 2
		}
		k := strings.IndexByte(s[i:], ';')
		end := len(s) - 2
		if k >= 0 {
			end = i + k
		}
		s = s[:i] + s[end:]
	}
	d := relaxedField(data, field{"DKIM-Signature", []byte(s)})
	return d[:len(d)-2]
}

var signedNames = []string{"from", "from", "to", "to", "subject", "subject", "date", "date", "message-id", "message-id", "mime-version", "mime-version", "content-type", "content-type"}

func sign(m []byte, rsaKey *rsa.PrivateKey, edKey ed25519.PrivateKey) []byte {
	fields, body := split(m)
	h := sha256.New()
	bodyHash(h, body)
	bh := base64.StdEncoding.EncodeToString(h.Sum(nil))
	alg := "rsa-sha256"
	if edKey != nil {
		alg = "ed25519-sha256"
	}
	f := "DKIM-Signature: v=1; a=" + alg + "; c=relaxed/relaxed; d=example.com; s=s1; t=1791246000;\r\n\th=" +
		strings.Join(signedNames, ":") + ";\r\n\tbh=" + bh + "; b="
	digest := sha256.Sum256(headData(fields, signedNames, []byte(f+"\r\n")))
	var sig []byte
	if edKey != nil {
		sig = ed25519.Sign(edKey, digest[:])
	} else {
		sig, _ = rsa.SignPKCS1v15(nil, rsaKey, crypto.SHA256, digest[:])
	}
	out := make([]byte, 0, len(m)+len(f)+400)
	out = append(out, f...)
	out = append(out, base64.StdEncoding.EncodeToString(sig)...)
	out = append(out, "\r\n"...)
	return append(out, m...)
}

func tags(v string) map[string]string {
	t := map[string]string{}
	for _, spec := range strings.Split(v, ";") {
		k, val, ok := strings.Cut(spec, "=")
		if ok {
			t[strings.TrimSpace(k)] = strings.TrimSpace(val)
		}
	}
	return t
}

// the key's record read, as a verifier reads the TXT record it fetched
func publicKey(record string) (*rsa.PublicKey, ed25519.PublicKey) {
	t := tags(record)
	der, _ := base64.StdEncoding.DecodeString(t["p"])
	if t["k"] == "ed25519" {
		return nil, ed25519.PublicKey(der)
	}
	k, err := x509.ParsePKIXPublicKey(der)
	if err != nil {
		return nil, nil
	}
	return k.(*rsa.PublicKey), nil
}

func verify(m []byte, record string) bool {
	rsaPub, edPub := publicKey(record)
	fields, body := split(m)
	own := fields[0]
	t := tags(string(own.raw[bytes.IndexByte(own.raw, ':')+1:]))
	strip := func(s string) string {
		return strings.Map(func(r rune) rune {
			if r == ' ' || r == '\t' || r == '\r' || r == '\n' {
				return -1
			}
			return r
		}, s)
	}
	bh, _ := base64.StdEncoding.DecodeString(strip(t["bh"]))
	b, _ := base64.StdEncoding.DecodeString(strip(t["b"]))
	h := sha256.New()
	bodyHash(h, body)
	if !bytes.Equal(h.Sum(nil), bh) {
		return false
	}
	var names []string
	for _, n := range strings.Split(t["h"], ":") {
		names = append(names, strings.TrimSpace(n))
	}
	digest := sha256.Sum256(headData(fields, names, own.raw))
	if edPub != nil {
		return ed25519.Verify(edPub, digest[:], b)
	}
	return rsa.VerifyPKCS1v15(rsaPub, crypto.SHA256, digest[:], b) == nil
}

func main() {
	rsaKey, _ := rsa.GenerateKey(rand.Reader, 2048)
	edKey := ed25519.NewKeyFromSeed(bytes.Repeat([]byte{0x2a}, 32))
	m := message()
	switch os.Args[1] {
	case "dkim_sign_rsa", "dkim_sign_ed25519":
		ed := os.Args[1] == "dkim_sign_ed25519"
		n := count(2, 2000)
		if ed {
			n = count(2, 50000)
		}
		total := 0
		start := time.Now()
		for i := 0; i < n; i++ {
			if ed {
				total += len(sign(m, nil, edKey))
			} else {
				total += len(sign(m, rsaKey, nil))
			}
		}
		report(os.Args[1], time.Since(start), n)
		if total < n*len(m) {
			os.Exit(1)
		}
	case "dkim_verify_rsa", "dkim_verify_ed25519":
		ed := os.Args[1] == "dkim_verify_ed25519"
		var s []byte
		var record string
		if ed {
			s = sign(m, nil, edKey)
			record = "v=DKIM1; k=ed25519; p=" + base64.StdEncoding.EncodeToString(edKey.Public().(ed25519.PublicKey))
		} else {
			s = sign(m, rsaKey, nil)
			der, _ := x509.MarshalPKIXPublicKey(&rsaKey.PublicKey)
			record = "v=DKIM1; k=rsa; p=" + base64.StdEncoding.EncodeToString(der)
		}
		n := count(2, 50000)
		start := time.Now()
		for i := 0; i < n; i++ {
			if !verify(s, record) {
				fmt.Fprintln(os.Stderr, "does not verify")
				os.Exit(1)
			}
		}
		report(os.Args[1], time.Since(start), n)
	case "dkim_body":
		body := bigBody()
		n := count(2, 500)
		var sum byte
		start := time.Now()
		for i := 0; i < n; i++ {
			h := sha256.New()
			bodyHash(h, body)
			sum ^= h.Sum(nil)[0]
		}
		report("dkim_body", time.Since(start), n)
		if sum == 1 {
			fmt.Fprintln(os.Stderr, "")
		}
	default:
		fmt.Fprintln(os.Stderr, "usage: mailauth <dkim_sign_rsa|dkim_sign_ed25519|dkim_verify_rsa|dkim_verify_ed25519|dkim_body> [n]")
		os.Exit(2)
	}
}
