# Current StopWatch application

Hardware: **M5Stack StopWatch C152 V1.0**. See [firmware overview](../README.md), [manifest](manifest.json) and [checksums](SHA256SUMS).

Use the supplied `OBD-HUB-WATCH-v1.0-20261002-app.bin` only with the matching device partition layout. This is the verified current application, not an empty-board full image. Its build metadata predates the public cleanup commit; SHA-256 and source file hashes identify the tested code accurately.

Source hashes in `manifest.json` normalize CRLF to LF, so Git line-ending conversion does not change source identity. The application binary hash is calculated over its exact bytes.
