#!/bin/sh
clear
echo "Starting Fiveg dependencies shell installer"
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