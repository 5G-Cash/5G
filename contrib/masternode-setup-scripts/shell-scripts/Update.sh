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
cd && cd 5G/contrib/masternode-setup-scripts/shell-scripts
echo "Downloading 5G-CASH latest release update..."
wget -N https://github.com/5G-Cash/5G/releases/download/v1.2.2.0-unk/fiveg-u18-daemon.tar
sudo tar -c /usr/local/bin -zxvf fiveg-u18-daemon.tar
echo "Setting permissions..."
cd && sudo chmod +x /usr/local/bin/fiveg*
sudo chmod +x /usr/local/bin/tor*
echo "Launching fivegd..."
cd && cd /usr/local/bin
fivegd -daemon
echo "Cleaning up..."
cd && cd 5G/contrib/masternode-setup-scripts/shell-scripts
rm -rf fiveg-u18-daemon.tar
echo "Fivegnode Updated Successfully!"
