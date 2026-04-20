# network-tools

A collection of Bash network diagnostic utilities.

## Tools

### dnsping

DNS query "ping" using `dig`. Sends repeated DNS queries to a resolver and reports the response status (NOERROR, NXDOMAIN, SERVFAIL, etc.) and round-trip time for each query, with summary statistics on exit.

**Usage:**
```bash
./dnsping/dnsping example.com
./dnsping/dnsping -s 1.1.1.1 -c 5 cloudflare.com
./dnsping/dnsping -t MX -d google.com     # show first answer
./dnsping/dnsping -s 9.9.9.9 -T example.com  # force TCP
./dnsping/dnsping -x 1.1.1.1             # reverse lookup
```

**Options:**
| Flag | Description |
|------|-------------|
| `-s SERVER` | DNS server to query (default: system resolver) |
| `-c COUNT` | Stop after COUNT queries (default: run forever) |
| `-i INTERVAL` | Seconds between queries (default: 1) |
| `-t TYPE` | Query type: A, AAAA, MX, NS, etc. (default: A) |
| `-T` | Force TCP instead of UDP |
| `-x` | Reverse (PTR) lookup |
| `-d` | Show first answer in response |

**Requirements:** `dig` (from `bind-utils` or `dnsutils`)

---

### httping

HTTP/HTTPS "ping" using `curl`. Sends repeated HEAD (or GET) requests to a URL and reports the HTTP status code and round-trip time for each request, with summary statistics on exit.

**Usage:**
```bash
./httping/httping https://example.com
./httping/httping -c 5 -i 0.5 https://example.com
./httping/httping -d https://example.com        # detailed timings
./httping/httping -G -c 10 https://api.example.com/health
```

**Options:**
| Flag | Description |
|------|-------------|
| `-c COUNT` | Stop after COUNT requests (default: run forever) |
| `-i INTERVAL` | Seconds between requests (default: 1) |
| `-t TIMEOUT` | Per-request timeout in seconds (default: 5) |
| `-G` | Use GET instead of HEAD |
| `-d` | Show detailed timing breakdown |
| `--no-color` | Disable colour output |

**Requirements:** `curl`

## Requirements

- Bash 4+
- `dig` (for dnsping)
- `curl` (for httping)

## License

MIT
