# Fiveg
Fiveg Protocol (FIVEG) is a new state of the art privacy coin that uses Sigma and Dandelion++ Protocol along with TOR.



### Key Features
    -Privacy (Anonymous and Untraceable)
    -Sigma Protocol 
    -Tor Protocol
    -Dandelion++ Protocol
    -Proof of Work ()
    -Proof of Stake (PoS 3.0)
    -Masternode

### Specifications
| Specification | Value |
|:-----------|:-----------|


#### Block Rewards


Windows Build Instructions and Notes
==================================
The Windows wallet is build with QTs QMAKE. See [build-windows.md]() for instructions.

MACOS Build Instructions and Notes
==================================
The macOS wallet itself is build with QTs QMAKE. See [build-macos.md]() for instructions.

Linux Build Instructions and Notes
==================================

Dependencies
----------------------
You can use the ``depscript.sh`` to automatically install Dependencies to build Fiveg or manually install them using the syntax below

1.  Update packages

        ``sudo apt-get update``

2.  Install required packages
        
        ``sudo apt-get install build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils libboost-all-dev libzmq3-dev libminizip-dev``

3.  Install Berkeley DB 4.8

        ``sudo add-apt-repository ppa:bitcoin/bitcoin && sudo apt-get update && sudo apt-get install libdb4.8-dev libdb4.8++-dev``
4.  Install QT 5

        ``
        sudo apt-get install libminiupnpc-dev && sudo apt-get install libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler libqrencode-dev
        ``
        

Build
----------------------
1.  Clone the source:

        git clone 

2.  Build Fiveg core:

    Configure and build the headless Fiveg binaries as well as the GUI (if Qt is found).

    You can disable the GUI build by passing `--without-gui` to configure.

        ```
        ./autogen.sh
        ./configure
        make
        ```

3.  It is recommended to build and run the unit tests:

        ``make check``


Setting up a Fivegnode
==================================

Read [contrib/masternode-setup-scripts/README.md](contrib/masternode-setup-scripts/README.md) for instructions.
