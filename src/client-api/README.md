# Overview
API for interaction with the new `fiveg-client` application. This project closely resembles the `rpc` layout, however it instead uses ZeroMQ as a transport mechanism, the code for which is contained within `src/zmqserver`. 

# Request
A request to be passed contains three elements: `type`, `collection`, and `data`.

## type
operation to be performed on `collection`.
### values
`get`: get a previously created object of type `collection`\
    requirements: see `Data Formats`. \
    *returns*: the full object previously created.

`create`: create an object of type `collection` to be stored. \
    requirements: see `Data Formats`. \
    *returns*: the full object that has been created.

`update`: update an object of type `collection`. \
    requirements: - see `Data Formats` (requires passing `id`. all other fields optional). \
    *returns*: the full object that has been modified.

`delete`: delete an object of type `collection`. requires passing `id` in `data`. \
    requirements: - see `Data Formats`. \
    *returns*: the status of the call.

`initial`: gets all objects of type `collection`. \
    requirements: - see `Data Formats`. (requires passing `id`. all other fields optional) \
    *returns*: the stored `data` object for type `collection`.

Some methods do not need to have a `type` parameter passed. These will be indicated by the preceding
`None` type in the `Data Formats` section below.

## collection
A function with one or more operations.

| Collection     | Description      | Port   | Passphrase | Warmup Ok
| :------------- | :--------------- | :----- | :--------- | :--------- |
| [apiStatus](#apistatus)           | Initial status of core. | 👁  | – |   ✅   |
| [backup](#backup)                 | Creates a zip file from wallet.dat and the `persistent/` folder, and stores in the filepath specified, as `index_backup-{TIMESTAMP}.zip`.  | 🔐 | – |  – |
| [balance](#balance)               | Coin balance of a number of different categories. | 🔐 | – | – |
| [block](#block)                   | All transaction information from, and including, the blockHash parameter passed. | 🔐 | – | – |
| [blockhash](#blockhash)           | Same as `block` but queried by block height. | 🔐 | – | – |
| [blockchain](#blockchain)         | Information related to chain sync status and tip. | 🔐 | – | – |
| [listMints](#listmints)           | Returns a list of unspent Sigma mints.  | 🔐 | 🔐 | – |
| [lockWallet](#lockwallet)         | Lock core wallet, should it be encrypted.  | 🔐 | – | – |
| [mint](#mint)                     | Mint 1 or more Sigma mints. | 🔐 | ✅ | – |
| [paymentRequest](#paymentrequest) | Bundles of information related to a Zcoin payment. | 🔐 | – | – |
| [privateTxFee](#privatetxfee)     | Gets the transaction fee and inputs required for the private spend data passed. | 🔐 | - | – |
| [rebroadcast](#rebroadcast)       | Rebroadcast a transaction from mempool. | 🔐 | - | - |
| [rpc](#rpc)                       | Call an RPC command, or return a list of them. | 🔐 | - | - |
| [sendPrivate](#sendprivate)       | Spend 1 or more Sigma mints. Allows specifying third party addresses to spend to. | 🔐    | ✅ | – |
| [sendZcoin](#sendzcoin)           | Send Zcoin to the specified address(es). | 🔐 | ✅ | – |
| [setPassphrase](#setpassphrase)   |  Set, or update, the passphrase for the encryption of the wallet. | 🔐 | – | – |
| [setting](#setting)               | Interact with settings. | 🔐 | - | – |
| [stateWallet](#statewallet)       | Returns all information related to addresses in the wallet.  | 🔐 | – | – |
| [stop](#stop)                     | Stop the Zcoin daemon. | 🔐 | - | – |
| [txFee](#txfee)                   | Gets the transaction fee required for the size of the tx passed + fee per kb. | 🔐 | – | – |
| [unlockWallet](#unlockwallet)     | Unlock core wallet, should it be encrypted. | 🔐 | – | – |
| [updateLabels](#updatelabels)     | Update transaction labels stored in the persistent tx metadata file. | 🔐 | – | – |
| [fivegnodeControl](#fivegnodecontrol)     | Start/stop Fivegnode(s) by alias. | 🔐 | ✅ | – |
| [fivegnodeKey](#fivegnodekey)             | Generate a new fivegnode key. | 🔐 | - | – |
| [fivegnodeList](#fivegnodelist)           | list information related to all Fivegnodes. | 🔐 | – | – |

## data
to be passed with `type` to be performed on `collection`.

## Reply
Replies contain two elements: `meta` and `data`.

### meta
status of the request performed.

#### values
`200`: successful request.
`400`: unsuccessful request.

### data
payload of the reply.

## Data Formats

#### Guide
VAR: variable return value.
OPTIONAL: not a necessary parameter to pass.

### `apiStatus`
`initial`:
```
    data: {
    }
``` 
*Returns:*
```
    data: { 
        version: INT,
        protocolVersion: INT,
        walletVersion: INT, (VAR: Wallet initialized)
        walletLock: BOOL,  (VAR: Wallet initialized)
        unlockedUntil: INT, (VAR : wallet is unlocked)
        dataDir: STRING,
        network: STRING("main"|"testnet"|"regtest"),
        blocks: INT,
        connections: INT,
        devAuth: BOOL,
        synced: BOOL,
        reindexing: BOOL,
        safeMode: BOOL,
        pid: INT,
        modules: {
            API: BOOL,
            Fivegnode: BOOL
        },
        Fivegnode: {
            localCount: INT,
            totalCount: INT,
            enabledCount: INT
        }
    },
    meta:{
       status: 200
    }
```

### `backup`
`create`:
```
    data: {
        directory: STRING ("absolute/path/to/backup/location")
    }
``` 
*Returns:*
```
    data: { 
        true
    },
    meta:{
       status: 200
    }
```

### `balance`
`get`:
```
    data: {
    }
```
*Returns:*
```
{ 
    data: {
        total: {
            all: INT,
            pending: INT,
            available: INT
        },
        public: {
            confirmed: INT,
            unconfirmed: INT,
            locked: INT,
        },
        private: {
            confirmed: INT,
            unconfirmed: INT,
        },
        unspentMints: {
            "0.05": {
                confirmed: INT,
                unconfirmed: INT,
            },
            "0.1": {
                confirmed: INT,
                unconfirmed: INT,
            },
            "0.5": {
                confirmed: INT,
                unconfirmed: INT,
            },
            "1": {
                confirmed: INT,
                unconfirmed: INT,
            },
            "10": {
                confirmed: INT,
                unconfirmed: INT,
            },
            "25": {
                confirmed: INT,
                unconfirmed: INT,
            },
            "100": {
                confirmed: INT,
                unconfirmed: INT,
            }
        },
    }, 
    meta:{
        status: 200
    }
}
```

### `block`
`get`:
```
    data: {
        blockHash: STRING
    }
``` 
*Returns:*
```
    data: {
        type: STRING("PoS"|"PoW"),
        [STRING | "MINT"]: (address)
            {
                txids: 
                    {
                        STRING: (txid)
                            { 
                            ["mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"]: (category) 
                                 {
                                    address: STRING,
                                    category: STRING("mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"),
                                    amount: INT,
                                    fee: INT(sats),
                                    label: STRING (VAR : address is part of fivegd "account")
                                    firstSeenAt: INT(secs), 
                                    blockHash: STRING,
                                    blockTime: INT(secs),                            
                                    blockHeight: INT,
                                    txid: STRING
                                },
                            ["mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"]: (category) 
                                 {
                                    address: STRING,
                                    category: STRING("mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"),
                                    amount: INT,
                                    fee: INT(sats),
                                    label: STRING (VAR : address is part of fivegd "account")
                                    firstSeenAt: INT(secs), 
                                    blockHash: STRING,
                                    blockTime: INT(secs),                            
                                    blockHeight: INT,
                                    txid: STRING
                                },
                                ...
                            },
                        STRING: (txid)
                            { 
                            ["mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"]: (category) 
                                 {
                                    address: STRING,
                                    category: STRING("mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"),
                                    amount: INT,
                                    fee: INT(sats),
                                    label: STRING (VAR : address is part of fivegd "account")
                                    firstSeenAt: INT(secs), 
                                    blockHash: STRING,
                                    blockTime: INT(secs),                            
                                    blockHeight: INT,
                                    txid: STRING                                    
                                },
                            ["mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"]: (category) 
                                 {
                                    address: STRING,
                                    category: STRING("mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"),
                                    amount: INT,
                                    fee: INT(sats),
                                    label: STRING (VAR : address is part of fivegd "account")
                                    firstSeenAt: INT(secs), 
                                    blockHash: STRING,
                                    blockTime: INT(secs),                            
                                    blockHeight: INT,
                                    txid: STRING 
                                },
                                ...
                            },
                        ...
                    },
                total: 
                    {
                        sent: INT, (VAR : category=="send"|"mint"|"spend")
                        balance: INT, (VAR: category=="mined"|"fivegnode"|"receive"|)
                    } 
            },
        [STRING | "MINT"]: (address)
            { 
                txids: 
                    {
                        STRING: (txid)
                            { 
                            ["mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"]: (category) 
                                 {
                                    address: STRING,
                                    category: STRING("mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"),
                                    amount: INT,
                                    fee: INT(sats),
                                    label: STRING (VAR : address is part of fivegd "account")
                                    firstSeenAt: INT(secs), 
                                    blockHash: STRING,
                                    blockTime: INT(secs),                            
                                    blockHeight: INT,
                                    txid: STRING 
                                },
                            ["mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"]: (category) 
                                 {
                                    address: STRING,
                                    category: STRING("mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"),
                                    amount: INT,
                                    fee: INT(sats),
                                    label: STRING (VAR : address is part of fivegd "account")
                                    firstSeenAt: INT(secs), 
                                    blockHash: STRING,
                                    blockTime: INT(secs),                            
                                    blockHeight: INT,
                                    txid: STRING 
                                },
                                ...
                            },
                        STRING: (txid)
                            { 
                            ["mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"]: (category) 
                                 {
                                    address: STRING,
                                    category: STRING("mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"),
                                    amount: INT,
                                    fee: INT(sats),
                                    label: STRING (VAR : address is part of fivegd "account")
                                    firstSeenAt: INT(secs), 
                                    blockHash: STRING,
                                    blockTime: INT(secs),                            
                                    blockHeight: INT,
                                    txid: STRING 
                                },
                            ["mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"]: (category) 
                                 {
                                    address: STRING,
                                    category: STRING("mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"),
                                    amount: INT,
                                    fee: INT(sats),
                                    label: STRING (VAR : address is part of fivegd "account")
                                    firstSeenAt: INT(secs), 
                                    blockHash: STRING,
                                    blockTime: INT(secs),                            
                                    blockHeight: INT,
                                    txid: STRING 
                                },
                                ...
                            },
                        ...
                    },
                total: 
                    {
                        sent: INT, (VAR : category=="send"|"mint"|"spend")
                        balance: INT, (VAR: category=="mined"|"fivegnode"|"receive"|)
                    }  
            },
        ...
        },
    meta: {
        status: 200
    }
```

### `blockhash`
`get`:
```
    data: {
        index: INT
    }
```
*Returns:*
```
    data: {
        type: STRING("PoS"|"PoW"),
        addresses: { ... }
    },
    meta: {
        status: 200
    }
```

### `blockchain`
`get`:
```
    data: {
    }
```
*Returns:*
```
{ 
    data: {
        testnet: BOOL,
        connections: INT,
        type: STRING,
        status: {
            isBlockchainSynced: BOOL,
            isFivegnodeListSynced: BOOL,
            isWinnersListSynced: BOOL,
            isSynced: BOOL,
            isFailed: BOOL
        },
        currentBlock: {
            height: INT,
            timestamp: INT,
        },
        avgBlockTime(secs): INT,
        timeUntilSynced(secs): INT (VAR: !isBlockchainSynced)
    } 
    meta:{
        status: 200
    }
}
```

### `listMints`:
`None`:
```
    data: {
    }
``` 
*Returns:*
```
{ 
    data: {
        STRING (serialNumberHash) {
            id: INT,
            IsUsed: BOOL,
            denomination: INT,
            value:  STRING,
            serialNumber: STRING,
            nHeight: INT, 
            randomness: STRING
        },
        STRING (serialNumberHash) {
            id: INT,
            IsUsed: BOOL,
            denomination: INT,
            value:  STRING,
            serialNumber: STRING,
            nHeight: INT, 
            randomness: STRING
        },
        ...
    }, 
    meta:{
       status: 200
    }
}
```

### `lockWallet`:
`None`:
```
    data: {
    }
``` 
*Returns:*
```
{ 
    data: {
        true
    }, 
    meta:{
       status: 200
    }
}
```

### `mint`
`create`:
```
    data: {
        value: INT (VAR: denominations.IsNull())
        denominations: { (VAR: value.IsNull())
            STRING (denomination) : INT (amount),
            STRING (denomination) : INT (amount),
            STRING (denomination) : INT (amount),
            ...
        }
    },
    auth: {
        passphrase: STRING
    }
``` 
*Returns:*
```
{ 
    txids: {
       STRING (txid)
   },
    meta:{
       status: 200
    }
}
```


### `mintTxFee`
`none`:
```
    data: {
        value: INT (sats) (VAR: denominations.IsNull())
        denominations: { (VAR: value.IsNull())
            STRING (denomination) : INT (amount),
            STRING (denomination) : INT (amount),
            STRING (denomination) : INT (amount),
            ...
        }
    },
    auth: {
        passphrase: STRING
    }
```
*Returns:*
```
{
    "fee": INT(sats)
    meta:{
       status: 200
    }
}
```

### `paymentRequest`
`create`:
```
    data: {
        amount: INT (OPTIONAL),
        label: STRING,
        message: STRING
    }
```
*Returns:*
```
    data: {
        address: STRING, 
        createdAt: INT(secs)
        amount: INT,
        label: STRING,
        message: STRING
        state: STRING ("active")
    },
    meta:{
        status: 200
    }
```

`update`:
```
    data: {
        id: STRING,
        amount: INT, (OPTIONAL)
        label: STRING, (OPTIONAL)
        message: STRING, (OPTIONAL)
        state: STRING, (OPTIONAL) ("active"|"hidden"|"deleted"|"archived")
    }
```
*Returns:*
```
    data: {
        address: STRING,
        amount: INT, (OPTIONAL)
        label: STRING, (OPTIONAL)
        message: STRING (OPTIONAL)
        state: STRING, (OPTIONAL) ("active"|"hidden"|"deleted"|"archived")
    },
    meta:{
        status: 200
    }
```

`delete`:
```
    data: {
        id: STRING
    }
```
*Returns:*
```
    data: {
        true
    },
    meta:{
        status: 200
    }
```

`initial`:
```
    data: {
    }
```
*Returns:*
```
   data: {
        STRING (address): {
            "amount": INT,
            "createdAt": INT,
            "label": STRING,
            "message": STRING,
            state: STRING, ("active"|"hidden"|"deleted"|"archived")
        },
        STRING (address): {
            "amount": INT,
            "created_at": INT,
            "label": STRING,
            "message": STRING,
            state: STRING, ("active"|"hidden"|"deleted"|"archived")
        },
    ...
    },
    meta:{
        status: 200
    }
```

### `privateTxFee`
`none`:
```
    data: {
        outputs: [
            {
                address: STRING,
                amount: INT
            }
        ],
        label: STRING,
        subtractFeeFromAmount: BOOL
    }
```

*Returns:*
```
{
    data: {
        inputs: INT,
        fee: INT(sats)
    },
    meta:{
        status: 200
    }
}
```

### `rebroadcast`
`create`:
```
    data: {
        "txHash" : STRING
    }
```
*Returns:*
```
   data: {
        "result": BOOL
        "error": STRING (VAR: failure in call)
    },
    meta:{
        status: 200
    }
```

### `rpc`
`initial`:
```
    data: {
    }
```
*Returns:*
```
    data: {
        categories: [
            "category" : {
                [
                    "command",
                    "command",
                    ...
                ]
            },
            "category" : {
                [
                    "command",
                    "command",
                    ...
                ]
            },
            ...
        ]
    }
```

`create`:
```
    data: {
        "method": STRING
        "args": STRING
    }
```
*Returns:*
```
   data: {
        "result": STRING,
        "error": STRING (VAR: failure in call)
    },
    meta:{
        status: 200
    }
```

### `sendPrivate`
`create`:
```
    data: {
        outputs: [
            {
                address: STRING,
                amount: INT
            }
        ],
        label: STRING,
        subtractFeeFromAmount: BOOL
    }
    auth: {
        passphrase: STRING
    }
``` 

*Returns:*
```
{ 
    data: {
        txids: {
           STRING (txid)
       }
    }, 
    meta:{
        status: 200
    }
}
```

### `sendZcoin`
`create`:
```
    data: {
        addresses: {
          STRING (address): {
            amount: INT,
            label: STRING
          },
          STRING (address): {
            amount: INT,
            label: STRING
          },
          ...
        },
        feePerKb: INT (sats),
        subtractFeeFromAmount: BOOL
    },
    auth: {
        passphrase: STRING
    }
``` 
*Returns:*
```
{ 
    data: {
        txids: {
           STRING (txid)
       }
    }, 
    meta:{
       status: 200
    }
}
```

### `setPassphrase`
`create`:
```
    data: {
    },
    auth: {
        passphrase: STRING
    }
```

*Returns:*
```
{ 
    data: {
        true
    }, 
    meta:{
        status: 200
    }
}
```

`update`:
```
    data: {
    },
    auth: {
        passphrase: STRING,
        newPassphrase: STRING
    }
```
*Returns:*
```
{ 
    data: {
        true
    }, 
    meta:{
        status: 200
    }
}
```

### `setting`
`initial`:
```
    data: {
      }
```
*Returns:*
```
{
    data: {
        STRING (setting): {
            data: STRING,
            changed: BOOL,
            disabled: BOOL
        },
        STRING (setting): {
            data: STRING,
            changed: BOOL,
            disabled: BOOL
        },
        ...
        restartNow: BOOL
    },
    meta:{
       status: 200
    }
}
```

`create`:
```
    data: {
        STRING (setting): STRING (data),
        STRING (setting): STRING (data),
        ...
        }
    }
```
*Returns:*
```
{
    data: {
        true
    },
    meta:{
       status: 200
    }
}
```

`update`:
```
    data: {
        STRING (setting): STRING (data),
        STRING (setting): STRING (data),
        ...
    }
```
*Returns:*
```
{
    data: {
        true
    },
    meta:{
       status: 200
    }
}
```

`get`:
```
{
    data: {
        settings: [STRING,STRING,...]
    }
}
```
*Returns:*
```
{
    data: {
        STRING (setting): {
            data: STRING,
            changed: BOOL,
            restartRequired: BOOL
        },
        STRING (setting): {
            data: STRING,
            changed: BOOL,
            restartRequired: BOOL
        },
        ...
    },
    meta:{
       status: 200
    }
}
```

### `stateWallet`
`initial`:
```
    data: {
    }
``` 
*Returns:*
```
    data: {
        [STRING | "MINT"]: (address)
            { 
                txids: 
                    {
                        STRING: (txid)
                            { 
                            ["mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"]: (category) 
                                 {
                                    address: STRING,
                                    category: STRING("mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"),
                                    amount: INT,
                                    fee: INT(sats),
                                    label: STRING (VAR : address is part of fivegd "account")
                                    firstSeenAt: INT(secs), 
                                    blockHash: STRING,
                                    blockTime: INT(secs),                            
                                    blockHeight: INT,
                                    txid: STRING
                                },
                            ["mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"]: (category) 
                                 {
                                    address: STRING,
                                    category: STRING("mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"),
                                    amount: INT,
                                    fee: INT(sats),
                                    label: STRING (VAR : address is part of fivegd "account")
                                    firstSeenAt: INT(secs), 
                                    blockHash: STRING,
                                    blockTime: INT(secs),                            
                                    blockHeight: INT,
                                    txid: STRING
                                },
                                ...
                            },
                        STRING: (txid)
                            { 
                            ["mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"]: (category) 
                                 {
                                    address: STRING,
                                    category: STRING("mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"),
                                    amount: INT,
                                    fee: INT(sats),
                                    label: STRING (VAR : address is part of fivegd "account")
                                    firstSeenAt: INT(secs), 
                                    blockHash: STRING,
                                    blockTime: INT(secs),                            
                                    blockHeight: INT,
                                    txid: STRING                                    
                                },
                            ["mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"]: (category) 
                                 {
                                    address: STRING,
                                    category: STRING("mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"),
                                    amount: INT,
                                    fee: INT(sats),
                                    label: STRING (VAR : address is part of fivegd "account")
                                    firstSeenAt: INT(secs), 
                                    blockHash: STRING,
                                    blockTime: INT(secs),                            
                                    blockHeight: INT,
                                    txid: STRING 
                                },
                                ...
                            },
                        ...
                    },
                total: 
                    {
                        sent: INT, (VAR : category=="send"|"mint"|"spend")
                        balance: INT, (VAR: category=="mined"|"fivegnode"|"receive"|)
                    } 
            },
        [STRING | "MINT"]: (address)
            { 
                txids: 
                    {
                        STRING: (txid)
                            { 
                            ["mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"]: (category) 
                                 {
                                    address: STRING,
                                    category: STRING("mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"),
                                    amount: INT,
                                    fee: INT(sats),
                                    label: STRING (VAR : address is part of fivegd "account")
                                    firstSeenAt: INT(secs), 
                                    blockHash: STRING,
                                    blockTime: INT(secs),                            
                                    blockHeight: INT,
                                    txid: STRING 
                                },
                            ["mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"]: (category) 
                                 {
                                    address: STRING,
                                    category: STRING("mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"),
                                    amount: INT,
                                    fee: INT(sats),
                                    label: STRING (VAR : address is part of fivegd "account")
                                    firstSeenAt: INT(secs), 
                                    blockHash: STRING,
                                    blockTime: INT(secs),                            
                                    blockHeight: INT,
                                    txid: STRING 
                                },
                                ...
                            },
                        STRING: (txid)
                            { 
                            ["mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"]: (category) 
                                 {
                                    address: STRING,
                                    category: STRING("mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"),
                                    amount: INT,
                                    fee: INT(sats),
                                    label: STRING (VAR : address is part of fivegd "account")
                                    firstSeenAt: INT(secs), 
                                    blockHash: STRING,
                                    blockTime: INT(secs),                            
                                    blockHeight: INT,
                                    txid: STRING 
                                },
                            ["mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"]: (category) 
                                 {
                                    address: STRING,
                                    category: STRING("mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"),
                                    amount: INT,
                                    fee: INT(sats),
                                    label: STRING (VAR : address is part of fivegd "account")
                                    firstSeenAt: INT(secs), 
                                    blockHash: STRING,
                                    blockTime: INT(secs),                            
                                    blockHeight: INT,
                                    txid: STRING 
                                },
                                ...
                            },
                        ...
                    },
                total: 
                    {
                        sent: INT, (VAR : category=="send"|"mint"|"spend")
                        balance: INT, (VAR: category=="mined"|"fivegnode"|"receive"|)
                    }  
            },
        ...
        },
    meta: {
        status: 200
    }
```

### `stop`
`initial`:
```
    data: {
      }
``` 
*Returns:*
```
{ 
    data: {
        true
    }, 
    meta:{
       status: 200
    }
}
```

### `txFee`
```
    data: {
          addresses: {
              STRING (address): INT (amount),
              STRING (address): INT (amount),
              ...
          },
          feePerKb: INT (sats),
          subtractFeeFromAmount: BOOL
      }
``` 
*Returns:*
```
{ 
    data: {
        fee: INT(sats),
    }, 
    meta:{
       status: 200
    }
}
```

### `unlockWallet`:
`None`:
```
    auth: {
        passphrase: STRING
    }
``` 
*Returns:*
```
{ 
    data: {
        true
    }, 
    meta:{
       status: 200
    }
}
```

### `updateLabels`
```
    data: {
        "txid": STRING,
        "label": STRING
    }
``` 
*Returns:*
```
{ 
    data: {
        "txid": STRING,
        "label": STRING,
        "address": STRING
    }
    meta:{
       status: 200
    }
}
```

### `fivegnodeControl`
`update`:
```
    data: {
        method: STRING, ["start-all" || "start-missing" || "start-alias"]
        alias: STRING (VAR: method=="start-alias")
      }
``` 
*Returns:*
```
{ 
    data: {
        detail: {
            status: {
                alias: STRING,
                success: BOOL,
                info: STRING (VAR: success==false)
            },
            status: {
                alias: STRING,
                success: BOOL,
                info: STRING (VAR: success==false)
            },
            ...
        },
        overall: {
          successful: INT,
          failed: INT,
          total: INT 
        }
    }, 
    meta:{
       status: 200
    }
}
```

### `fivegnodeKey`
`create`:
```
    data: {
      }
```
*Returns:*
```
{
    data: {
        key: STRING
    },
    meta:{
       status: 200
    }
}
```

### `fivegnodeList`
`initial`:
```
    data: {
      }
``` 
*Returns:*
```
{

    data: (VAR: Fivegnodes not synced) {
        nodes: {
            STRING: (txid) {
                label: STRING,
                isMine: BOOL,
                outpoint: {
                    txid: STRING,
                    fiveg: INT
                },
                authority: {
                    ip: STRING,
                    port: STRING
                },
                position: INT
            },
            STRING: (txid) {
                label: STRING,
                isMine: BOOL,
                outpoint: {
                    txid: STRING,
                    fiveg: INT
                },
                authority: {
                    ip: STRING,
                    port: STRING
                },
                position: INT
            },
            ...
            }
        },
        total: INT
    },

    data: (VAR: Fivegnodes synced) {
        STRING: { (payeeAddress)
            rank: INT,
            outpoint: {
                txid: STRING,
                fiveg: STRING
            },
            status: STRING,
            protocolVersion: INT,
            payeeAddress: STRING,
            lastSeen: INT,
            activeSince: INT,
            lastPaidTime: INT,
            lastPaidBlock: INT,
            authority: {
                ip: STRING,
                port: STRING
            }
            isMine: BOOL,
            label: STRING, (VAR: isMine==true)
            position: INT, (VAR: isMine==true)
            qualify: {
                result: BOOL,
                description: STRING ["Is scheduled"             ||
                                     "Invalid nProtocolVersion" ||
                                     "Too new"                  ||
                                     "collateralAge < znCount"] (VAR: result==false)
                data: { (VAR: result==false)
                    nProtocolVersion: INT, (VAR: description=="Invalid nProtocolVersion")
                    sigTime:          INT, (VAR: description=="Too new"),
                    qualifiedAfter:   INT, (VAR: description=="Too new"),
                    collateralAge:    INT, (VAR: description=="collateralAge < znCount"),
                    znCount:          INT, (VAR: description=="collateralAge < znCount")
                }
            }
        },
        STRING: { (payeeAddress)
            rank: INT,
            outpoint: {
                txid: STRING,
                fiveg: STRING
            },
            status: STRING,
            protocolVersion: INT,
            payeeAddress: STRING,
            lastSeen: INT,
            activeSince: INT,
            lastPaidTime: INT,
            lastPaidBlock: INT,
            authority: {
                ip: STRING,
                port: STRING
            }
            isMine: BOOL,
            label: STRING, (VAR: isMine==true)
            position: INT, (VAR: isMine==true)
            qualify: {
                result: BOOL,
                description: STRING ["Is scheduled"             ||
                                     "Invalid nProtocolVersion" ||
                                     "Too new"                  ||
                                     "collateralAge < znCount"] (VAR: result==false)
                data: { (VAR: result==false)
                    nProtocolVersion: INT, (VAR: description=="Invalid nProtocolVersion")
                    sigTime:          INT, (VAR: description=="Too new"),
                    qualifiedAfter:   INT, (VAR: description=="Too new"),
                    collateralAge:    INT, (VAR: description=="collateralAge < znCount"),
                    znCount:          INT, (VAR: description=="collateralAge < znCount")
                }
            }
        },
        ...
    }, 
    meta:{
       status: 200
    }
}
```

# Publish
The publisher module is comprised of various _topics_ that are triggered under specific conditions, called _events_. Both topics and events have a 1 to N relationship with each other; ie. 1 event may trigger 1 to N topics, and 1 topic may be triggered by 1 to N events.


|               | _Event_       | NotifyAPIStatus  | SyncTransaction | NumConnectionsChanged | UpdatedBlockTip | UpdatedMintStatus  | UpdatedSettings | UpdatedFivegnode | UpdateSyncStatus |
| ------------- | ------------- | ---------------  | --------------- | --------------------- | --------------- | -----------------  | --------------- | ------------ | ---------------- |
| **_Topic_**   | Description   | API status notification | new transactions | fivegd peer list updated | blockchain head updated | mint transaction added/up dated | settings changed/updated | Fivegnode update | Blockchain sync update
**address** (triggers [block](#block))                 | block tx data.                            | -  | -  | -  | ✅ | -  | -  | -  | -  |
**apiStatus** (triggers [apiStatus](#apistatus))       | Status of API                             | ✅ | -  | -  | -  | -  | -  | -  | -  |
**balance** (triggers [balance](#balance))             | Balance info                              | -  | -  | -  | ✅ | -  | -  | -  | -  |
**block** (triggers [blockchain](#blockchain))         | general block data (sync status + header) | -  | -  | ✅ | ✅ | -  | -  | -  | ✅ |
**mintStatus** (triggers [mintStatus](#mintstatus))    | status of new mint                        | -  | -  | -  | -  | ✅ | -  | -  | -  |
**settings** (triggers [readSettings](#readsettings))  | settings changed                          | -  | -  | -  | -  | -  | ✅ | -  | -  |
**transaction** (triggers [transaction](#transaction)) | new transaction data                      | -  | ✅ | -  | -  | -  | -  | -  | -  |
**fivegnode** (triggers [fivegnodeUpdate](#fivegnodeupdate))       | update to fivegnode                           | -  | -  | -  | -  | -  | -  | ✅ | -  |

## Methods

Methods specific to the publisher.

### `mintStatus` 
*Returns:*
```
{
    "data": {
        STRING: (txid + index) {
            txid: STRING,
            fiveg: STRING,
            available: BOOL
        },
        STRING: (txid + index) {
            txid: STRING,
            fiveg: STRING,
            available: BOOL
        },
        ...
    },
    meta: {
        status: 200
    },
    error: null
}
```

### `readSettings` 
*Returns:*
```
{
    data: {
        STRING: (setting) {
            data: STRING,
            changed: BOOL,
            disabled: BOOL
        },
        STRING: (setting) {
            data: STRING,
            changed: BOOL,
            disabled: BOOL
        },
        ...
        restartNow: BOOL
    }

    "meta": {
        "status": 200
    },
    "error": null
}
```



### `transaction` 
*Returns:*
```
{ 
    data: {
        [STRING | "MINT"]: (address)
            { 
                txids: 
                    {
                        STRING: (txid)
                            { 
                            ["mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"]: (category) 
                                 {
                                    address: STRING,
                                    category: STRING("mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"),
                                    amount: INT,
                                    fee: INT(sats),
                                    label: STRING (VAR : address is part of fivegd "account")
                                    firstSeenAt: INT(secs), 
                                    blockHash: STRING,
                                    blockTime: INT(secs),                            
                                    blockHeight: INT,
                                    txid: STRING
                                },
                            ["mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"]: (category) 
                                 {
                                    address: STRING,
                                    category: STRING("mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"),
                                    amount: INT,
                                    fee: INT(sats),
                                    label: STRING (VAR : address is part of fivegd "account")
                                    firstSeenAt: INT(secs), 
                                    blockHash: STRING,
                                    blockTime: INT(secs),                            
                                    blockHeight: INT,
                                    txid: STRING
                                },
                                ...
                            },
                        STRING: (txid)
                            { 
                            ["mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"]: (category) 
                                 {
                                    address: STRING,
                                    category: STRING("mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"),
                                    amount: INT,
                                    fee: INT(sats),
                                    label: STRING (VAR : address is part of fivegd "account")
                                    firstSeenAt: INT(secs), 
                                    blockHash: STRING,
                                    blockTime: INT(secs),                            
                                    blockHeight: INT,
                                    txid: STRING                                    
                                },
                            ["mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"]: (category) 
                                 {
                                    address: STRING,
                                    category: STRING("mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"),
                                    amount: INT,
                                    fee: INT(sats),
                                    label: STRING (VAR : address is part of fivegd "account")
                                    firstSeenAt: INT(secs), 
                                    blockHash: STRING,
                                    blockTime: INT(secs),                            
                                    blockHeight: INT,
                                    txid: STRING 
                                },
                                ...
                            },
                        ...
                    },
                total: 
                    {
                        sent: INT, (VAR : category=="send"|"mint"|"spend")
                        balance: INT, (VAR: category=="mined"|"fivegnode"|"receive"|)
                    } 
            },
        [STRING | "MINT"]: (address)
            { 
                txids: 
                    {
                        STRING: (txid)
                            { 
                            ["mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"]: (category) 
                                 {
                                    address: STRING,
                                    category: STRING("mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"),
                                    amount: INT,
                                    fee: INT(sats),
                                    label: STRING (VAR : address is part of fivegd "account")
                                    firstSeenAt: INT(secs), 
                                    blockHash: STRING,
                                    blockTime: INT(secs),                            
                                    blockHeight: INT,
                                    txid: STRING 
                                },
                            ["mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"]: (category) 
                                 {
                                    address: STRING,
                                    category: STRING("mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"),
                                    amount: INT,
                                    fee: INT(sats),
                                    label: STRING (VAR : address is part of fivegd "account")
                                    firstSeenAt: INT(secs), 
                                    blockHash: STRING,
                                    blockTime: INT(secs),                            
                                    blockHeight: INT,
                                    txid: STRING 
                                },
                                ...
                            },
                        STRING: (txid)
                            { 
                            ["mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"]: (category) 
                                 {
                                    address: STRING,
                                    category: STRING("mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"),
                                    amount: INT,
                                    fee: INT(sats),
                                    label: STRING (VAR : address is part of fivegd "account")
                                    firstSeenAt: INT(secs), 
                                    blockHash: STRING,
                                    blockTime: INT(secs),                            
                                    blockHeight: INT,
                                    txid: STRING 
                                },
                            ["mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"]: (category) 
                                 {
                                    address: STRING,
                                    category: STRING("mined"|"send"|"receive"|"fivegnode"|"spend"|"mint"),
                                    amount: INT,
                                    fee: INT(sats),
                                    label: STRING (VAR : address is part of fivegd "account")
                                    firstSeenAt: INT(secs), 
                                    blockHash: STRING,
                                    blockTime: INT(secs),                            
                                    blockHeight: INT,
                                    txid: STRING 
                                },
                                ...
                            },
                        ...
                    },
                total: 
                    {
                        sent: INT, (VAR : category=="send"|"mint"|"spend")
                        balance: INT, (VAR: category=="mined"|"fivegnode"|"receive"|)
                    }  
            },
        ...
        },
    meta: {
        status: 200
    }
}
```

### `fivegnodeUpdate` 
*Returns:*
```
{
    "data": {
        STRING: (txid + index) {
            rank: INT,
            outpoint: {
                txid: STRING,
                fiveg: STRING
            },
            status: STRING,
            protocolVersion: INT,
            payeeAddress: STRING,
            lastSeen: INT,
            activeSince: INT,
            lastPaidTime: INT,
            lastPaidBlock: INT,
            authority: {
                ip: STRING,
                port: STRING
            }
            isMine: BOOL,
            label: STRING, (VAR: isMine==true)
            position: INT, (VAR: isMine==true)
            qualify: {
                result: BOOL,
                description: STRING ["Is scheduled"             ||
                                     "Invalid nProtocolVersion" ||
                                     "Too new"                  ||
                                     "collateralAge < znCount"] (VAR: result==false)
                data: { (VAR: result==false)
                    nProtocolVersion: INT, (VAR: description=="Invalid nProtocolVersion")
                    sigTime:          INT, (VAR: description=="Too new"),
                    qualifiedAfter:   INT, (VAR: description=="Too new"),
                    collateralAge:    INT, (VAR: description=="collateralAge < znCount"),
                    znCount:          INT, (VAR: description=="collateralAge < znCount")
                }
            }
        }
    },
    "meta": {
        "status": 200
    },
    "error": null
}
```
