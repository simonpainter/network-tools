# Contributing

Thanks for helping improve `network-tools`.

## Before you start

- Keep changes focused and small.
- Preserve existing command-line flags and output formats unless the change is intentional.
- Update `README.md` when behavior or usage changes.

## Development

These tools are Bash scripts, so changes should stay compatible with the existing shell style.

- Quote variables consistently.
- Prefer clear helper functions over repeated logic.
- Avoid adding external dependencies unless they are widely available on macOS and Linux.

## Validation

Test the tool you changed with a small, representative command.

- `dnsping`: `./dnsping -c 1 example.com`
- `httping`: `./httping -c 1 https://example.com`
- `ntpping`: `./ntpping -c 1 pool.ntp.org`

## Pull requests

- Describe the user-visible change.
- Mention any dependency or environment requirements.
- Note any output changes so reviewers can check them quickly.
