Fivegnode Build Instructions and Notes
=============================
 - Version 1.0.0
 - Date: July 26, 2020
 - More detailed guide available here: https://fivegx.org/fivegnode-setup-guide/

Prerequisites
-------------
 - Ubuntu 18.04+
 - Libraries to build from Fiveg source
 - Port **22020** is open

Step 1. Build
----------------------
**1.1.**  Check out from source:

    git clone https://github.com/FivegXProject/Fiveg

**1.2.**  See [README.md](README.md) for instructions on building.

Step 2. (Optional - only if firewall is running). Open port 22020
----------------------
**2.1.**  Run:

    sudo ufw allow 22020
    sudo ufw default allow outgoing
    sudo ufw enable

Step 3. First run on your Local Wallet
----------------------
**3.0.**  Go to the checked out folder

    cd Fiveg

**3.1.**  Start daemon in testnet mode:

    ./src/fivegd -daemon -server -testnet

**3.2.**  Generate fivegnodeprivkey:

    ./src/fiveg-cli fivegnode genkey

(Store this key)

**3.3.**  Get wallet address:

    ./src/fiveg-cli getaccountaddress 0

**3.4.**  Send to received address **exactly 10000 FIVEG** in **1 transaction**. Wait for 15 confirmations.

**3.5.**  Stop daemon:

    ./src/fiveg-cli stop

Step 4. In your VPS where you are hosting your Fivegnode. Update config files
----------------------
**4.1.**  Create file **fiveg.conf** (in folder **~/.fiveg**)

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

**4.2.**  Create file **fivegnode.conf** (in 2 folders **~/.fiveg** and **~/.fiveg/testnet3**) contains the following info:
 - LABEL: A one word name you make up to call your node (ex. SN1)
 - IP:PORT: Your fivegnode VPS's IP, and the port is always 22020.
 - FIVEGNODEPRIVKEY: This is the result of your "fivegnode genkey" from earlier.
 - TRANSACTION HASH: The collateral tx. hash from the 10000 FIVEG deposit.
 - INDEX: The Index is always 0 or 1.

To get TRANSACTION HASH, run:

    ./src/fiveg-cli fivegnode outputs

The output will look like:

    { "d6fd38868bb8f9958e34d5155437d009b72dfd33fc28874c87fd42e51c0f74fdb" : "0", }

Sample of fivegnode.conf:

    SN1 51.52.53.54:22020 XrxSr3fXpX3dZcU7CoiFuFWqeHYw83r28btCFfIHqf6zkMp1PZ4 d6fd38868bb8f9958e34d5155437d009b72dfd33fc28874c87fd42e51c0f74fdb 0

Step 5. Run a fivegnode
----------------------
**5.1.**  Start fivegnode:

    ./src/fiveg-cli fivegnode start-alias <LABEL>

For example:

    ./src/fiveg-cli fivegnode start-alias SN1

**5.2.**  To check node status:

    ./src/fiveg-cli fivegnode debug

If not successfully started, just repeat start command
