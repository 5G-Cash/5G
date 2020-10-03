#!/bin/sh
#!/bin/sh
# Copyright (c) 2020 The Fiveg Core Developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


clear
echo "Starting Fivegnode update script"
cd && cd /usr/local/bin
echo "Stopping fivegd..."
fiveg-cli stop
cd && cd Fiveg/contrib/masternode-setup-scripts/shell-scripts
echo "Downloading Fiveg latest release update..."
wget -N https://github.com/FivegXProject/Fiveg/releases/download/v1.2.1/fiveg-1.2.1-x86_64-linux-gnu.tar.gz
sudo tar -c /usr/local/bin -zxvf fiveg-1.2.1-x86_64-linux-gnu.tar.gz
echo "Setting permissions..."
cd && sudo chmod +x /usr/local/bin/fiveg*
sudo chmod +x /usr/local/bin/tor*
echo "Launching fivegd..."
cd && cd /usr/local/bin
fivegd -daemon
echo "Cleaning up..."
cd && cd Fiveg/contrib/masternode-setup-scripts/shell-scripts
rm -rf fiveg-1.2.1-x86_64-linux-gnu.tar.gz
echo "Fivegnode Updated Successfully!"