5G-CASH integration/staging tree
===========================

What is 5G-CASH?
----------------
5G-CASH (VGC) is an experimental digital currency that enables instant payments to
anyone, anywhere in the world. It uses different key features technology to operate
with no central authority allowing everyone to operate the way they want to with new 
state of the art privacy coin that uses Sigma and Dandelion++ Protocol along with TOR.

For more information, as well as an immediately useable, binary version of
this software, see https://fiveg.cash/download/, or read the
[original whitepaper](https://fiveg.cash/5g-cash.pdf).



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
4.  Install QT 5

        
        sudo apt-get install libminiupnpc-dev && sudo apt-get install libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler    libqrencode-dev
        
        

Building 5G-CASH
----------------------

1. Static compile

    git clone https://github.com/5G-Cash/5G.git
     
    cd 5G/depends
    
    make HOST=x86_64-linux-gnu
    
    cd ..
    
    ./autogen.sh
    
    ./configure --prefix=`pwd`/depends/x86_64-linux-gnu
    
    make


2. Shared binary

    git clone https://github.com/5G-Cash/5G.git
    
    cd 5G
    
    ./autogen.sh
    
    ./configure
    
    make
    
3.  It is recommended to build and run the unit tests:

        make check


Setting up a Fivegnode
==================================

Read [contrib/masternode-setup-scripts/README.md](contrib/masternode-setup-scripts/README.md) for instructions.
