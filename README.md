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
You can use the "depscript.sh" to automatically install Dependencies to build VGC or manually install them using the syntax below

1.  Update packages

        sudo apt-get update

2.  Install required packages
        
        sudo apt-get install build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils libboost-all-dev libzmq3-dev libminizip-dev

3.  Install Berkeley DB 4.8

        sudo add-apt-repository ppa:bitcoin/bitcoin && sudo apt-get update && sudo apt-get install libdb4.8-dev libdb4.8++-dev
    
5.  Install QT 5

        sudo apt-get install libminiupnpc-dev && sudo apt-get install libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler libqrencode-dev
        
        

Building 5G-CASH
----------------------
### 1. Static compile
    git clone https://github.com/5G-Cash/5G.git
    chmod -R +rwx 5G 
    cd 5G/depends
    make HOST=x86_64-linux-gnu
    cd ..
    ./autogen.sh
    ./configure --prefix=$PWD/depends/x86_64-linux-gnu
    make 


#### 2. Shared binary
    git clone https://github.com/5G-Cash/5G.git
    chmod -R +rwx 5G
    cd 5G
    ./autogen.sh
    ./configure
    make
    
### 3.  It is recommended to build and run the unit tests:
    make check


Setting up a Fivegnode
==================================

Read [contrib/masternode-setup-scripts/README.md](contrib/masternode-setup-scripts/README.md) for instructions.

Rosetta API
==================================

A lightweight Rosetta Data API server is provided in `contrib/rosetta`.
See `doc/rosetta.md` for details on running the server.

