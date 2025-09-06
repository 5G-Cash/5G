# OpenSSL 3.0 Compatibility

5G-CASH still relies on OpenSSL 1.1 style APIs in several bundled components.
To compile against OpenSSL 3.0, the build defines `OPENSSL_API_COMPAT=0x10100000L`,
which enables legacy interfaces while linking to the newer library. Ensure the
system has OpenSSL 3.0 development headers installed (`libssl-dev` on Debian-based
systems). Older or incompatible OpenSSL versions may require additional patches.
