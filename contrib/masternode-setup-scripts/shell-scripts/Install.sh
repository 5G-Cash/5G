#!/bin/sh
# Copyright (c) 2020 The Fiveg Core Developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


clear
echo "Starting Fivegnode auto download and install script"
echo "Updating the machine..."
sudo apt-get update
echo "Machine successfully updated"
echo "Installing required packages.."
sudo apt-get install build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils libboost-all-dev libzmq3-dev libminizip-dev
echo -e "Successfully installed build-essential\n
		 Successfully installed libtool\n
		 Successfully installed autotools-dev\n
		 Successfully installed automake\n
		 Successfully installed pkg-config\n
		 Successfully installed libssl-dev\n
		 Successfully installed libevent-dev\n
		 Successfully installed bsdmainutils\n
		 Successfully installed libboost-all-dev\n
		 Successfully installed libzmq3-dev\n
		 Successfully installed libminizip-dev\n"
echo "Installing Berkeley DB 4.8..."
sudo add-apt-repository ppa:bitcoin/bitcoin && sudo apt-get update && sudo apt-get install libdb4.8-dev libdb4.8++-dev
echo "Sucessfully installed Berkeley DB 4.8"
echo "Installing QT 5..."
sudo apt-get install libminiupnpc-dev && sudo apt-get install libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler libqrencode-dev
echo -e "Successfully installed libminiupnpc-dev\n
		 Successfully installed libqt5gui5\n
		 Successfully installed libqt5core5a\n
		 Successfully installed libqt5dbus5\n
		 Successfully installed qttools5-dev\n
		 Successfully installed qttools5-dev-tools\n
		 Successfully installed libprotobuf-dev\n
		 Successfully installed protobuf-compiler\n
		 Successfully installed libqrencode-dev"
echo "Successfully installed required dependencies"
echo "Updating/Upgrading OS..."
sudo apt update && sudo apt upgrade -y
echo "Downloading 5G-CASH latest build..."
wget -N https://github.com/5G-Cash/5G/releases/download/v1.2.2.0-unk/fiveg-u18-daemon.tar
echo "Extracting build..."
sudo tar -C /usr/local/bin -zxvf fiveg-u18-daemon.tar
echo "Setting permissions..."
cd && sudo chmod +x /usr/local/bin/fiveg*
sudo chmod +x /usr/local/bin/tor*
echo "Creating .fiveg directory..."
mkdir ~/.fiveg
cd ~/.fiveg
echo "Setting up and enabling fail2ban..."
sudo apt-get install fail2ban -y
sudo ufw allow ssh
sudo ufw allow 22020
sudo ufw enable
echo "Launching fivegd..."
cd && cd /usr/local/bin
fivegd -daemon
echo "Cleaning up..."
cd && cd Fiveg/contrib/masternode-setup-scripts/shell-scripts
rm -rf fiveg-u18-daemon.tar
echo "Fivegnode Installed Successfully!"
