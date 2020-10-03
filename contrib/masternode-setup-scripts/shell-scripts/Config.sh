#!/bin/sh
# Copyright (c) 2020 The Fiveg Core Developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


clear
echo "Starting Fivegnode Auto Config script"
cd && cd /usr/local/bin
echo "Stopping fivegd..."
fiveg-cli stop
cd ~/.fiveg
rm -rf fiveg.conf
echo "Editing fiveg.conf..."
cat >> fiveg.conf <<'EOF'
rpcuser=username
rpcpassword=password
rpcallowip=127.0.0.1
debug=1
txindex=1
daemon=1
server=1
listen=1
maxconnections=24
fivegnode=1
fivegnodeprivkey=XXXXXXXXXXXXXXXXX  ## Replace with your fivegnode private key
externalip=XXX.XXX.XXX.XXX:23020 ## Replace with your node external IP
EOF
echo "Running fivegd..."
cd && cd /usr/local/bin
fivegd -daemon
echo "Fivegnode Configuration Completed!"