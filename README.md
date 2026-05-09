# network-tools

A collection of Bash network diagnostic utilities.

## Tools

### dnsping

DNS query "ping" using `dig`. Sends repeated DNS queries to a resolver and reports the response status (NOERROR, NXDOMAIN, SERVFAIL, etc.) and round-trip time for each query, with summary statistics on exit.

**Usage:**
```bash
./dnsping example.com
./dnsping -s 1.1.1.1 -c 5 cloudflare.com
./dnsping -t MX -d google.com     # show first answer
./dnsping -s 9.9.9.9 -T example.com  # force TCP
./dnsping -x 1.1.1.1             # reverse lookup
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
./httping https://example.com
./httping -c 5 -i 0.5 https://example.com
./httping -d https://example.com        # detailed timings
./httping -G -c 10 https://api.example.com/health
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

---

### ntpping

NTP query "ping" using `sntp`. Sends repeated SNTP queries to a time server and reports the clock offset and round-trip delay for each query, with summary statistics on exit.

**Usage:**
```bash
./ntpping pool.ntp.org
./ntpping -c 5 time.cloudflare.com
./ntpping -i 0.5 -W 2 time.google.com
./ntpping -4 0.uk.pool.ntp.org      # force IPv4
```

**Options:**
| Flag | Description |
|------|-------------|
| `-c COUNT` | Stop after COUNT queries (default: run forever) |
| `-i INTERVAL` | Seconds between queries (default: 1) |
| `-W TIMEOUT` | Per-query timeout in seconds (default: 2) |
| `-4` | Force IPv4 |
| `-6` | Force IPv6 |
| `-n` | Disable colour output |

Offset is the local clock's difference from the server (positive = local clock is ahead). Delay is the round-trip network delay reported by `sntp`. Lines are colour-coded by absolute offset (green ≤ 10ms, yellow ≤ 100ms, red > 100ms).

**Requirements:** `sntp` (from `ntp`, `ntpsec`, or `sntp` package; pre-installed on macOS)

---

### uping

> **uping has moved** — it now lives in its own repository: [simonpainter/uping](https://github.com/simonpainter/uping)

---

## Requirements

- Bash 4+
- `dig` (for dnsping)
- `curl` (for httping)
- `sntp` (for ntpping)

## Installation

Clone the repo and install the tools to your `PATH`.

```bash
# Install dependencies (curl and sntp ship with macOS; dig comes from bind)
brew install bind

# Install the bash tools
git clone https://github.com/simonpainter/network-tools.git
cd network-tools
sudo install -m 0755 dnsping httping ntpping /usr/local/bin/
```

If you'd rather not use `sudo`, copy them to `~/bin` (and make sure that's on your `PATH`):

```bash
mkdir -p ~/bin
install -m 0755 dnsping httping ntpping ~/bin/
```

After installation, verify with:

```bash
dnsping -c 1 example.com
httping -c 1 https://example.com
ntpping -c 1 pool.ntp.org
```

### Uninstall

```bash
sudo rm /usr/local/bin/{dnsping,httping,ntpping}
```

## License

MIT
