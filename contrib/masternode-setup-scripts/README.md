# Fiveg Protocol Masternode Setup Scripts
 Various scripts for setting up fivegnode.


## SHELL SCRIPTS
=============================================================
### BEFORE USING THESE SCRIPTS (PRE-SETUP)
Open your notepad or any text editor application on your pc and write this down as your ``Cheat Sheet``
```
1. FIVEGNODE NAME = SN1
2. COLLATERAL = 50000
3. FIVEG ADDRESS = SZRnGyyPv1FVGgMGn7JXbuHCGbsgiBRprq
4. FIVEGNODE GENKEY = 84qRmqujiRqJ1vepSacScUz1EuBTYoaPM3cD5n1211THemaRWms
5. FIVEGNODE OUTPUTS = 4873d0c50c6ddc623bedcf0684dafc107809f9434b8426b728634f7c8c455615 1
6. UNIQUE IP OF THE VPS = 201.47.23.109:22020
```

### GETTING A VPS (STEP 1)
Set up your VPS, we recommend [VULTR](https://www.vultr.com/?ref=8638319), and select ``DEPLOY INSTANCE`` then select the following
- Cloud compute
- Location -any
- Server type: Ubuntu 18.04
- Server size: 1GB $5/month
- Add your desired hostname and label
- Click DEPLOY
Note: The server will take a few minutes to deploy and will then shows as "running" in your "instances" section.

### QT WALLET CONFIGURATION (STEP 2)
1. Open your ``QT WALLET`` .
2. Open your ``debug console`` then type the following comamnd
	```
	fivegnode genkey
	```
	- Copy the generated key from your ``debug console`` and open your ``Cheat Sheet`` then paste the generated key on ``4. FIVEGNODE GENKEY`` (84qRmqujiRqJ1vepSacScUz1EuBTYoaPM3cD5n1211THemaRWms)

	- Copy your stealth address under the ``Receive`` tab (for example: SZRnGyyPv1FVGgMGn7JXbuHCGbsgiBRprq ) and paste it on your ``Cheat Sheet`` on ``3. FIVEG ADDRESS``

	- Copy the stealth address again from your ``Cheat Sheet`` under ``3. FIVEG ADDRESS`` and head over to your ``Send`` tab then paste the ``FIVEG ADDRESS`` on the ``Enter a Fiveg address`` area on the ``Send`` tab and input the ``2. FIVEGNODE COLLATERAL`` which is ``50000`` FIVEG on the ``Amount`` area then click ``Send`` and wait for ``6`` confirmations.

	- Then click go to your Transactions tab and right click the transaction when you send the ``FIVEGNODE COLLATERAL`` then click ``Copy transaction ID`` when a windows pops-up and head over to your ``debug console`` then type ``fivegnode outputs`` then press ``enter`` copy the ``txhash`` and paste it on your ``Cheat Sheet`` under ``5. FIVEGNODE OUTPUTS`` ( 4873d0c50c6ddc623bedcf0684dafc107809f9434b8426b728634f7c8c455615 ) and also don't forget to copy the ``outputidx`` ( 1 ) and paste it next to the ``txhash``.

	- Then head over to your ``Cheat Sheet`` 
		- input your ``VPS`` ip address ( 201.47.23.109 ) under ``6. UNIQUE IP OF THE VPS``
		- input your ``Fivegnode Name``  ( SN1 )under ``1. FIVEGNODE NAME``

3. Head over to your ``fiveg`` directory
	- Windows: %APPDATA%/fiveg
	- Linux: ~/.fiveg
	- Mac: ~/Library/Application Support/fiveg
4. Open and edit ``fivegnode.conf`` file with your preferred Text Editor
	- Inside ``fivegnode.conf`` file is this lines of text
	```
	# Fivegnode config file
	# Format: alias IP:port fivegnode_privatekey collateral_output_txid collateral_output_index
	# Example: zn1 127.0.0.1:22020 7Cqyr4U7GU7qVo5TE1nrfA8XPVqh7GXBuEBPYzaWxEhiRRDLZ5c 2bcd3c84c84f87eaa86e4e56834c92927a07f9e18718810b92e0d0324456a67c 1
	```
6. UNIQUE IP OF THE VPS = 201.47.23.109:22020
	- On line 4 add your ``1. FIVEGNODE NAME`` which is ``SN1``, next is add your ``6. UNIQUE IP OF THE VPS`` which is ``56.56.65.20`` and add the respective default port of ``encrypt`` which is ``22020``,next is add your  ``4. FIVEGNODE GENKEY`` which is ``84qRmqujiRqJ1vepSacScUz1EuBTYoaPM3cD5n1211THemaRWms``, and lastly add your ``5. FIVEGNODE OUTPUTS``which is ``4873d0c50c6ddc623bedcf0684dafc107809f9434b8426b728634f7c8c455615`` then add your ``outputidx`` which is ``1`` next to your ``5. FIVEGNODE OUTPUTS``.

	- It will look like this on the ``fivegnode.conf`` file

	```
	# Fivegnode config file
	# Format: alias IP:port fivegnode_privatekey collateral_output_txid collateral_output_index
	# Example: zn1 127.0.0.1:22020 7Cqyr4U7GU7qVo5TE1nrfA8XPVqh7GXBuEBPYzaWxEhiRRDLZ5c 2bcd3c84c84f87eaa86e4e56834c92927a07f9e18718810b92e0d0324456a67c 1
	SN1 201.47.23.109:22020 84qRmqujiRqJ1vepSacScUz1EuBTYoaPM3cD5n1211THemaRWms 4873d0c50c6ddc623bedcf0684dafc107809f9434b8426b728634f7c8c455615 1
	```

### ACCESSING YOUR VPS (STEP 3)
1. Download a SSH Application Client (you can choose one below)
	- PuTTY (https://www.chiark.greenend.org.uk/~sgtatham/putty/latest.html)
	- Bitvise (https://www.bitvise.com/ssh-client-download) [RECOMMENDED]
2. Open Bitvise
	1. Enter your VPS ``ip address`` under ``Host`` on the Server area
	2. Enter the port number which is ``22`` under ``Port``
	3. Enter your VPS ``username`` under ``Username`` on the Authentication area
	4. Check the ``Store encrypted password in profile`` checkbox and enter your VPS ``password`` under ``Password``
	5. Then click ``Log in``

### DOWNLOADING THE SCRIPT ON YOUR VPS (STEP 4)
On your SSH Terminal type this lines below one at a time
```
git clone https://github.com/FivegXProject/Fiveg
chmod -R 755 Fiveg
cd Fiveg/contrib/masternode-setup-scripts/shell-scripts
./Install.sh
```
Note: The script allows you to automatically install ``Fiveg`` from the ``Fiveg`` repository.

#### VPS WALLET CONFIGURATION (STEP 5)

```
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
externalip=XXX.XXX.XXX.XXX:22020 ## Replace with your node external IP
```
1. Change these following lines on the bash file named ``Config.sh`` by following steps below via ``vim``

```
vi Config.sh
```
then 

- change the ``username`` value to your own
- change the ``password`` value to your own
- change the ``XXX.XXX.XXX.XXX:22020`` value to your own which is located on your ``Cheat Sheet`` (e.g. 201.47.23.109:22020)
- change the ``yourmasternodeprivkeyhere`` value to your own which is also located on your ``Cheat Sheet`` (e.g. 84qRmqujiRqJ1vepSacScUz1EuBTYoaPM3cD5n1211THemaRWms this is your ``FIVEGNODE GENKEY``)
	(To save and exit the editor press ``Ctrl + C`` then type ``:wq!`` then press Enter)

2. Then open ``Config.sh`` file by typing ``./Config.sh``. 

Note: It will automatically change your ``fiveg.conf`` file located on the ``fiveg`` directory inputting all the text above.

#### STARTING AND CHECKING YOUR FIVEGNODE (STEP 6)

1. OPEN YOUR QT WALLET ON YOUR LOCAL MACHINE

2. HEAD OVER TO ``Fivegnodes`` tab on your wallet

3. YOU CAN CLICK ``Start all`` to start your fivegnode

Note: Fivegnodes that are enabled will appear on your ``Fivegnodes`` tab

### HOW TO UPDATE YOUR FIVEG DAEMON WITH A SCRIPT
Run first the ``Sourceupdate.sh`` shell file. On your SSH Terminal type this line below
```
cd
cd Fiveg/contrib/masternode-setup-scripts/shell-scripts
./Sourceupdate.sh
```

When finish updating the source using ``Sourceupdate.sh`` then you can run the ``Update.sh`` shell file. On your SSH Terminal type this line below
```
cd
cd Fiveg/contrib/masternode-setup-scripts/shell-scripts
./Update.sh
```
Note: It will automatically updates your daemon



if you have question regarding to the scripts feel free to head over to ``FivegX Discord Channel`` (https://discord.gg/eCBUKf)


# GREAT JOB! YOU CONFIGURED YOUR FIVEGNODE.
