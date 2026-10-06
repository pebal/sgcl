// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle of crypto::jose's tests: JOSE written by hand over Go's standard
// library alone (Go has no JOSE package), from RFC 7515-7518, 7638 and 8037.
// One command a run, its answer on stdout:
//
//	verify <jwk> <compact JWS>             ok <payload hex> | fail <why>
//	sign <alg> <private jwk> <payload hex> <compact JWS>
//	decrypt <private jwk> <compact JWE>    ok <plaintext hex> | fail <why>
//	encrypt <alg> <enc> <jwk> <hex>        <compact JWE>
//	thumbprint <jwk>                       <base64url SHA-256>
package main

import (
	"bytes"
	"crypto"
	"crypto/aes"
	"crypto/cipher"
	"crypto/ecdh"
	"crypto/ecdsa"
	"crypto/ed25519"
	"crypto/elliptic"
	"crypto/hmac"
	"crypto/rand"
	"crypto/rsa"
	"crypto/sha1"
	"crypto/sha256"
	"crypto/sha512"
	"crypto/subtle"
	"encoding/base64"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"hash"
	"math/big"
	"os"
	"strings"
)

var b64 = base64.RawURLEncoding

type jwk map[string]any

func (k jwk) str(name string) string {
	s, _ := k[name].(string)
	return s
}

func (k jwk) bytes(name string) []byte {
	b, err := b64.DecodeString(k.str(name))
	if err != nil {
		panic("jwk member " + name + ": " + err.Error())
	}
	return b
}

func (k jwk) num(name string) *big.Int {
	return new(big.Int).SetBytes(k.bytes(name))
}

func parseJwk(text string) jwk {
	var k jwk
	if err := json.Unmarshal([]byte(text), &k); err != nil {
		panic(err)
	}
	return k
}

func curve(crv string) elliptic.Curve {
	switch crv {
	case "P-256":
		return elliptic.P256()
	case "P-384":
		return elliptic.P384()
	case "P-521":
		return elliptic.P521()
	}
	panic("curve " + crv)
}

func ecdsaPublic(k jwk) *ecdsa.PublicKey {
	return &ecdsa.PublicKey{Curve: curve(k.str("crv")), X: k.num("x"), Y: k.num("y")}
}

func ecdsaPrivate(k jwk) *ecdsa.PrivateKey {
	return &ecdsa.PrivateKey{PublicKey: *ecdsaPublic(k), D: k.num("d")}
}

func rsaPublic(k jwk) *rsa.PublicKey {
	return &rsa.PublicKey{N: k.num("n"), E: int(k.num("e").Int64())}
}

func rsaPrivate(k jwk) *rsa.PrivateKey {
	p := &rsa.PrivateKey{PublicKey: *rsaPublic(k), D: k.num("d"), Primes: []*big.Int{k.num("p"), k.num("q")}}
	p.Precompute()
	return p
}

func hashOf(alg string) (crypto.Hash, func() hash.Hash) {
	switch alg[2:] {
	case "384":
		return crypto.SHA384, sha512.New384
	case "512":
		return crypto.SHA512, sha512.New
	}
	return crypto.SHA256, sha256.New
}

func digest(h crypto.Hash, data []byte) []byte {
	x := h.New()
	x.Write(data)
	return x.Sum(nil)
}

func sign(alg string, k jwk, input []byte) []byte {
	h, newHash := hashOf(alg)
	switch alg[:2] {
	case "HS":
		m := hmac.New(newHash, k.bytes("k"))
		m.Write(input)
		return m.Sum(nil)
	case "RS":
		s, err := rsa.SignPKCS1v15(rand.Reader, rsaPrivate(k), h, digest(h, input))
		if err != nil {
			panic(err)
		}
		return s
	case "PS":
		s, err := rsa.SignPSS(rand.Reader, rsaPrivate(k), h, digest(h, input), &rsa.PSSOptions{SaltLength: rsa.PSSSaltLengthEqualsHash})
		if err != nil {
			panic(err)
		}
		return s
	case "ES":
		key := ecdsaPrivate(k)
		r, s, err := ecdsa.Sign(rand.Reader, key, digest(h, input))
		if err != nil {
			panic(err)
		}
		n := (key.Curve.Params().BitSize + 7) / 8
		out := make([]byte, 2*n)
		r.FillBytes(out[:n])
		s.FillBytes(out[n:])
		return out
	case "Ed":
		return ed25519.Sign(ed25519.NewKeyFromSeed(k.bytes("d")), input)
	}
	panic("alg " + alg)
}

func verify(alg string, k jwk, input, sig []byte) bool {
	h, newHash := hashOf(alg)
	switch alg[:2] {
	case "HS":
		m := hmac.New(newHash, k.bytes("k"))
		m.Write(input)
		return hmac.Equal(m.Sum(nil), sig)
	case "RS":
		return rsa.VerifyPKCS1v15(rsaPublic(k), h, digest(h, input), sig) == nil
	case "PS":
		return rsa.VerifyPSS(rsaPublic(k), h, digest(h, input), sig, &rsa.PSSOptions{SaltLength: rsa.PSSSaltLengthEqualsHash}) == nil
	case "ES":
		key := ecdsaPublic(k)
		n := (key.Curve.Params().BitSize + 7) / 8
		if len(sig) != 2*n {
			return false
		}
		return ecdsa.Verify(key, digest(h, input), new(big.Int).SetBytes(sig[:n]), new(big.Int).SetBytes(sig[n:]))
	case "Ed":
		return ed25519.Verify(ed25519.PublicKey(k.bytes("x")), input, sig)
	}
	return false
}

func header(b64text string) map[string]any {
	raw, err := b64.DecodeString(b64text)
	if err != nil {
		panic(err)
	}
	var h map[string]any
	if err := json.Unmarshal(raw, &h); err != nil {
		panic(err)
	}
	return h
}

func cekSize(enc string) int {
	switch enc {
	case "A128CBC-HS256":
		return 32
	case "A192CBC-HS384":
		return 48
	case "A256CBC-HS512":
		return 64
	case "A128GCM":
		return 16
	case "A192GCM":
		return 24
	}
	return 32
}

// RFC 7518 4.6.2
func concatKdf(z []byte, algID string, apu, apv []byte, size int) []byte {
	var out []byte
	for counter := uint32(1); len(out) < size; counter++ {
		h := sha256.New()
		var be [4]byte
		binary.BigEndian.PutUint32(be[:], counter)
		h.Write(be[:])
		h.Write(z)
		for _, part := range [][]byte{[]byte(algID), apu, apv} {
			binary.BigEndian.PutUint32(be[:], uint32(len(part)))
			h.Write(be[:])
			h.Write(part)
		}
		binary.BigEndian.PutUint32(be[:], uint32(size*8))
		h.Write(be[:])
		out = h.Sum(out)
	}
	return out[:size]
}

// AES key wrap (RFC 3394 2.2.1 and 2.2.2), written from the RFC
func kwWrap(kek, key []byte) []byte {
	block, err := aes.NewCipher(kek)
	if err != nil {
		panic(err)
	}
	n := len(key) / 8
	a := []byte{0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6}
	r := append([]byte(nil), key...)
	buf := make([]byte, 16)
	for j := 0; j < 6; j++ {
		for i := 0; i < n; i++ {
			copy(buf, a)
			copy(buf[8:], r[8*i:8*i+8])
			block.Encrypt(buf, buf)
			t := uint64(n*j + i + 1)
			binary.BigEndian.PutUint64(a, binary.BigEndian.Uint64(buf[:8])^t)
			copy(r[8*i:], buf[8:])
		}
	}
	return append(a, r...)
}

func kwUnwrap(kek, wrapped []byte) ([]byte, error) {
	block, err := aes.NewCipher(kek)
	if err != nil {
		return nil, err
	}
	if len(wrapped)%8 != 0 || len(wrapped) < 24 {
		return nil, errors.New("kw length")
	}
	n := len(wrapped)/8 - 1
	a := append([]byte(nil), wrapped[:8]...)
	r := append([]byte(nil), wrapped[8:]...)
	buf := make([]byte, 16)
	for j := 5; j >= 0; j-- {
		for i := n - 1; i >= 0; i-- {
			t := uint64(n*j + i + 1)
			binary.BigEndian.PutUint64(buf, binary.BigEndian.Uint64(a)^t)
			copy(buf[8:], r[8*i:8*i+8])
			block.Decrypt(buf, buf)
			copy(a, buf[:8])
			copy(r[8*i:], buf[8:])
		}
	}
	if subtle.ConstantTimeCompare(a, []byte{0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6}) != 1 {
		return nil, errors.New("kw integrity")
	}
	return r, nil
}

func kwSize(alg string) int {
	switch {
	case strings.Contains(alg, "A128KW"):
		return 16
	case strings.Contains(alg, "A192KW"):
		return 24
	}
	return 32
}

func ecdhCurve(crv string) ecdh.Curve {
	switch crv {
	case "P-256":
		return ecdh.P256()
	case "P-384":
		return ecdh.P384()
	case "P-521":
		return ecdh.P521()
	}
	return ecdh.X25519()
}

func ecdhPoint(k jwk) []byte {
	if k.str("kty") == "OKP" {
		return k.bytes("x")
	}
	return append(append([]byte{4}, k.bytes("x")...), k.bytes("y")...)
}

func epkOf(pub *ecdh.PublicKey, crv string) map[string]any {
	b := pub.Bytes()
	if crv == "X25519" {
		return map[string]any{"kty": "OKP", "crv": crv, "x": b64.EncodeToString(b)}
	}
	n := (len(b) - 1) / 2
	return map[string]any{"kty": "EC", "crv": crv, "x": b64.EncodeToString(b[1 : 1+n]), "y": b64.EncodeToString(b[1+n:])}
}

func cbcTag(enc string, macKey, aad, iv, ct []byte) []byte {
	newHash := sha256.New
	switch enc {
	case "A192CBC-HS384":
		newHash = sha512.New384
	case "A256CBC-HS512":
		newHash = sha512.New
	}
	m := hmac.New(newHash, macKey)
	m.Write(aad)
	m.Write(iv)
	m.Write(ct)
	var al [8]byte
	binary.BigEndian.PutUint64(al[:], uint64(len(aad))*8)
	m.Write(al[:])
	return m.Sum(nil)[:len(macKey)]
}

func seal(enc string, cek, plaintext, aad []byte) (iv, ct, tag []byte) {
	if strings.HasSuffix(enc, "GCM") {
		block, _ := aes.NewCipher(cek)
		g, _ := cipher.NewGCM(block)
		iv = make([]byte, 12)
		rand.Read(iv)
		sealed := g.Seal(nil, iv, plaintext, aad)
		return iv, sealed[:len(plaintext)], sealed[len(plaintext):]
	}
	half := len(cek) / 2
	block, _ := aes.NewCipher(cek[half:])
	iv = make([]byte, 16)
	rand.Read(iv)
	pad := 16 - len(plaintext)%16
	ct = append(append([]byte{}, plaintext...), bytes.Repeat([]byte{byte(pad)}, pad)...)
	cipher.NewCBCEncrypter(block, iv).CryptBlocks(ct, ct)
	return iv, ct, cbcTag(enc, cek[:half], aad, iv, ct)
}

func open(enc string, cek, iv, ct, tag, aad []byte) ([]byte, error) {
	if strings.HasSuffix(enc, "GCM") {
		block, _ := aes.NewCipher(cek)
		g, _ := cipher.NewGCM(block)
		return g.Open(nil, iv, append(append([]byte{}, ct...), tag...), aad)
	}
	half := len(cek) / 2
	if subtle.ConstantTimeCompare(cbcTag(enc, cek[:half], aad, iv, ct), tag) != 1 {
		return nil, errors.New("tag")
	}
	block, _ := aes.NewCipher(cek[half:])
	out := make([]byte, len(ct))
	cipher.NewCBCDecrypter(block, iv).CryptBlocks(out, ct)
	pad := int(out[len(out)-1])
	if pad < 1 || pad > 16 {
		return nil, errors.New("padding")
	}
	return out[:len(out)-pad], nil
}

func encrypt(alg, enc string, k jwk, plaintext []byte) string {
	h := map[string]any{"alg": alg, "enc": enc}
	size := cekSize(enc)
	var cek, encKey []byte
	switch {
	case alg == "dir":
		cek = k.bytes("k")
	case strings.HasPrefix(alg, "RSA-OAEP"):
		cek = make([]byte, size)
		rand.Read(cek)
		var hh hash.Hash = sha1.New()
		if alg == "RSA-OAEP-256" {
			hh = sha256.New()
		}
		var err error
		encKey, err = rsa.EncryptOAEP(hh, rand.Reader, rsaPublic(k), cek, nil)
		if err != nil {
			panic(err)
		}
	case strings.HasSuffix(alg, "GCMKW"):
		cek = make([]byte, size)
		rand.Read(cek)
		block, _ := aes.NewCipher(k.bytes("k"))
		g, _ := cipher.NewGCM(block)
		iv := make([]byte, 12)
		rand.Read(iv)
		sealed := g.Seal(nil, iv, cek, nil)
		encKey = sealed[:size]
		h["iv"] = b64.EncodeToString(iv)
		h["tag"] = b64.EncodeToString(sealed[size:])
	case strings.HasPrefix(alg, "ECDH-ES"):
		c := ecdhCurve(k.str("crv"))
		peer, err := c.NewPublicKey(ecdhPoint(k))
		if err != nil {
			panic(err)
		}
		eph, _ := c.GenerateKey(rand.Reader)
		z, err := eph.ECDH(peer)
		if err != nil {
			panic(err)
		}
		h["epk"] = epkOf(eph.PublicKey(), k.str("crv"))
		if alg == "ECDH-ES" {
			cek = concatKdf(z, enc, nil, nil, size)
		} else {
			cek = make([]byte, size)
			rand.Read(cek)
			encKey = kwWrap(concatKdf(z, alg, nil, nil, kwSize(alg)), cek)
		}
	case strings.HasSuffix(alg, "KW"):
		cek = make([]byte, size)
		rand.Read(cek)
		encKey = kwWrap(k.bytes("k"), cek)
	default:
		panic("alg " + alg)
	}
	hj, _ := json.Marshal(h)
	protected := b64.EncodeToString(hj)
	iv, ct, tag := seal(enc, cek, plaintext, []byte(protected))
	return strings.Join([]string{protected, b64.EncodeToString(encKey), b64.EncodeToString(iv), b64.EncodeToString(ct), b64.EncodeToString(tag)}, ".")
}

func decrypt(k jwk, token string) ([]byte, error) {
	parts := strings.Split(token, ".")
	if len(parts) != 5 {
		return nil, errors.New("five parts")
	}
	h := header(parts[0])
	alg, _ := h["alg"].(string)
	enc, _ := h["enc"].(string)
	dec := func(s string) []byte {
		b, err := b64.DecodeString(s)
		if err != nil {
			panic(err)
		}
		return b
	}
	encKey, iv, ct, tag := dec(parts[1]), dec(parts[2]), dec(parts[3]), dec(parts[4])
	size := cekSize(enc)
	var cek []byte
	switch {
	case alg == "dir":
		cek = k.bytes("k")
	case strings.HasPrefix(alg, "RSA-OAEP"):
		var hh hash.Hash = sha1.New()
		if alg == "RSA-OAEP-256" {
			hh = sha256.New()
		}
		var err error
		cek, err = rsa.DecryptOAEP(hh, nil, rsaPrivate(k), encKey, nil)
		if err != nil {
			return nil, err
		}
	case strings.HasSuffix(alg, "GCMKW"):
		block, _ := aes.NewCipher(k.bytes("k"))
		g, _ := cipher.NewGCM(block)
		hs := func(n string) string { s, _ := h[n].(string); return s }
		var err error
		cek, err = g.Open(nil, dec(hs("iv")), append(encKey, dec(hs("tag"))...), nil)
		if err != nil {
			return nil, err
		}
	case strings.HasPrefix(alg, "ECDH-ES"):
		epkJSON, _ := json.Marshal(h["epk"])
		epk := parseJwk(string(epkJSON))
		c := ecdhCurve(k.str("crv"))
		peer, err := c.NewPublicKey(ecdhPoint(epk))
		if err != nil {
			return nil, err
		}
		priv, err := c.NewPrivateKey(k.bytes("d"))
		if err != nil {
			return nil, err
		}
		z, err := priv.ECDH(peer)
		if err != nil {
			return nil, err
		}
		if alg == "ECDH-ES" {
			cek = concatKdf(z, enc, nil, nil, size)
		} else {
			cek, err = kwUnwrap(concatKdf(z, alg, nil, nil, kwSize(alg)), encKey)
			if err != nil {
				return nil, err
			}
		}
	case strings.HasSuffix(alg, "KW"):
		var err error
		cek, err = kwUnwrap(k.bytes("k"), encKey)
		if err != nil {
			return nil, err
		}
	default:
		return nil, errors.New("alg " + alg)
	}
	return open(enc, cek, iv, ct, tag, []byte(parts[0]))
}

func thumbprint(k jwk) string {
	var canon string
	switch k.str("kty") {
	case "EC":
		canon = fmt.Sprintf(`{"crv":"%s","kty":"EC","x":"%s","y":"%s"}`, k.str("crv"), k.str("x"), k.str("y"))
	case "RSA":
		canon = fmt.Sprintf(`{"e":"%s","kty":"RSA","n":"%s"}`, k.str("e"), k.str("n"))
	case "OKP":
		canon = fmt.Sprintf(`{"crv":"%s","kty":"OKP","x":"%s"}`, k.str("crv"), k.str("x"))
	case "oct":
		canon = fmt.Sprintf(`{"k":"%s","kty":"oct"}`, k.str("k"))
	}
	d := sha256.Sum256([]byte(canon))
	return b64.EncodeToString(d[:])
}

func main() {
	args := os.Args[1:]
	switch args[0] {
	case "verify":
		k := parseJwk(args[1])
		parts := strings.Split(args[2], ".")
		if len(parts) != 3 {
			fmt.Println("fail parts")
			return
		}
		alg, _ := header(parts[0])["alg"].(string)
		sig, err := b64.DecodeString(parts[2])
		if err != nil || !verify(alg, k, []byte(parts[0]+"."+parts[1]), sig) {
			fmt.Println("fail signature")
			return
		}
		payload, _ := b64.DecodeString(parts[1])
		fmt.Println("ok " + hex.EncodeToString(payload))
	case "sign":
		k := parseJwk(args[2])
		payload, _ := hex.DecodeString(args[3])
		h, _ := json.Marshal(map[string]any{"alg": args[1]})
		input := b64.EncodeToString(h) + "." + b64.EncodeToString(payload)
		fmt.Println(input + "." + b64.EncodeToString(sign(args[1], k, []byte(input))))
	case "decrypt":
		pt, err := decrypt(parseJwk(args[1]), args[2])
		if err != nil {
			fmt.Println("fail " + err.Error())
			return
		}
		fmt.Println("ok " + hex.EncodeToString(pt))
	case "encrypt":
		pt, _ := hex.DecodeString(args[4])
		fmt.Println(encrypt(args[1], args[2], parseJwk(args[3]), pt))
	case "thumbprint":
		fmt.Println(thumbprint(parseJwk(args[1])))
	}
}
