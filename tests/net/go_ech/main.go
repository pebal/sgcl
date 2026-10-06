// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle of net::tls's Encrypted Client Hello (tests/net/tls_ech.cpp):
// Go's crypto/tls, RFC 9849, on the loopback.
//
//	server <cert.pem> <key.pem> <ech config hex> <ech private key hex> <n>
//	    listens on 127.0.0.1:0, prints "port <n>", answers n connections
//	    with one line "ech=<accepted> sni=<server name>"
//	client <host:port> <server name> <ca.pem> <ech config list hex>
//	    prints "ok ech=<accepted> line=<the server's line>" or "error <text>"
package main

import (
	"bufio"
	"crypto/tls"
	"crypto/x509"
	"encoding/hex"
	"errors"
	"fmt"
	"net"
	"os"
	"strconv"
	"strings"
)

func unhex(s string) []byte {
	b, err := hex.DecodeString(s)
	if err != nil {
		panic(err)
	}
	return b
}

func main() {
	a := os.Args[1:]
	switch a[0] {
	case "server":
		cert, err := tls.LoadX509KeyPair(a[1], a[2])
		if err != nil {
			panic(err)
		}
		n, _ := strconv.Atoi(a[5])
		cfg := &tls.Config{
			Certificates: []tls.Certificate{cert},
			EncryptedClientHelloKeys: []tls.EncryptedClientHelloKey{{
				Config: unhex(a[3]), PrivateKey: unhex(a[4]), SendAsRetry: true,
			}},
			MinVersion: tls.VersionTLS13,
		}
		if len(a) > 6 && a[6] == "x25519" {
			cfg.CurvePreferences = []tls.CurveID{tls.X25519}
		}
		l, err := tls.Listen("tcp", "127.0.0.1:0", cfg)
		if err != nil {
			panic(err)
		}
		fmt.Printf("port %d\n", l.Addr().(*net.TCPAddr).Port)
		for i := 0; i < n; i++ {
			c, err := l.Accept()
			if err != nil {
				return
			}
			tc := c.(*tls.Conn)
			if err := tc.Handshake(); err != nil {
				fmt.Fprintln(os.Stderr, "handshake:", err)
				c.Close()
				continue
			}
			st := tc.ConnectionState()
			fmt.Fprintf(c, "ech=%v sni=%s\n", st.ECHAccepted, st.ServerName)
			c.Close()
		}
	case "client":
		pool := x509.NewCertPool()
		pem, err := os.ReadFile(a[3])
		if err != nil {
			panic(err)
		}
		pool.AppendCertsFromPEM(pem)
		cfg := &tls.Config{
			ServerName:                     a[2],
			RootCAs:                        pool,
			EncryptedClientHelloConfigList: unhex(a[4]),
			MinVersion:                     tls.VersionTLS13,
		}
		if len(a) > 5 && a[5] == "p384" {
			cfg.CurvePreferences = []tls.CurveID{tls.CurveP384, tls.X25519}
		}
		c, err := tls.Dial("tcp", a[1], cfg)
		if err != nil {
			var rej *tls.ECHRejectionError
			if errors.As(err, &rej) {
				fmt.Printf("error rejected retry=%s\n", hex.EncodeToString(rej.RetryConfigList))
				return
			}
			fmt.Printf("error %s\n", strings.ReplaceAll(err.Error(), "\n", " "))
			return
		}
		line, _ := bufio.NewReader(c).ReadString('\n')
		fmt.Printf("ok ech=%v line=%s\n", c.ConnectionState().ECHAccepted, strings.TrimSpace(line))
		c.Close()
	}
}
