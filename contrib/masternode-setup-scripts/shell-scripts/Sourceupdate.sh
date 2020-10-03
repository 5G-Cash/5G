#!/bin/sh
#!/bin/sh
# Copyright (c) 2020 The Fiveg Core Developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


clear
echo "Starting Source Code Updater script"
echo "Deleting old source code..."
cd && sudo rm -rf Fiveg
echo "Downloading latest source code..."
git clone https://github.com/FivegXProject/Fiveg
echo "Setting permissions..."
sudo chmod -R 755 Fiveg
echo "Fiveg Source Code Updated Successfully!"