5G-CASH integration/staging tree
===========================

What is 5G-CASH?
----------------
5G-CASH (VGC) is the fifth-generation, open-source hybrid privacy-focused cryptocurrency with PoW, PoS, Masternodes, Dandelion++, and Tor integration designed for privacy and real-world utility. It's built on a robust Bitcoin Core foundation with enhanced consensus rules integrating anonymity tools to deliver a secure and scalable payment network.

Why 5G-CASH is a Hybrid Project?
-------------------------------
5G-CASH (VGC) is called a hybrid network because it combines multiple consensus and privacy/security layers into one blockchain, instead of relying on just one mechanism. It fuses PoW, PoS and Masternodes at the consensus level blending privacy at the protocol + network layer. 


For more information, read the
[original whitepaper](https://docs.google.com/document/d/1MkiiMSJDvdiig38eD_IKgGLZGzLBm10IFURShO2Dw1E/edit?usp=drive_link).



### Key Features
    -Privacy (Anonymous and Untraceable)
    -Sigma Protocol 
    -Tor Protocol
    -Dandelion++ Protocol
    -Proof of Work and Proof of Stake (3.0) share 55% of the block reward
    -Masternode gets 45% of the block reward

### Specifications and Block Rewards -----> https://discord.gg/tmQSFV9


Linux Build Instructions and Notes
==================================

Dependencies
----------------------
The repository ships a Linux dependency installer that supports Ubuntu/Debian
18.04 and newer, plus other common Linux package managers. The default installs
headless daemon/test dependencies and avoids the legacy Berkeley DB 4.8 PPA so
modern distributions can build with their packaged DB libraries.

```bash
./scripts/install-linux-deps.sh --no-gui
```

For a Qt wallet build, include GUI dependencies:

```bash
./scripts/install-linux-deps.sh --gui
```

`depscript.sh` remains as a compatibility wrapper around the installer above.

Building 5G-CASH
----------------------
### 1. Headless Linux build using system packages
```bash
git clone https://github.com/5G-Cash/5G.git
cd 5G
./scripts/install-linux-deps.sh --no-gui
./scripts/build-linux.sh --no-gui
```

The build script runs `./autogen.sh`, configures with `--with-gui=no`, and
passes `--with-incompatible-bdb` so Ubuntu 18.04+ and modern Linux distributions
can use their packaged Berkeley DB versions.

### 2. Qt GUI build using system packages
```bash
./scripts/install-linux-deps.sh --gui
./scripts/build-linux.sh --gui
```

### 3. Repository-managed dependency build
For the broadest binary compatibility and to avoid distribution-specific library
versions, build the dependency prefix first. The dependency prefix now builds
OpenSSL 3.5 LTS instead of the legacy OpenSSL 1.0.x line.

```bash
./scripts/build-linux.sh --depends --no-gui
```

### 4. Unit tests
The build script runs unit tests by default. To compile without tests, pass
`--no-tests`; to re-run tests after a build, run:

```bash
make check
```

Setting up a Fivegnode
==================================

Read [contrib/masternode-setup-scripts/README.md](contrib/masternode-setup-scripts/README.md) for instructions.
