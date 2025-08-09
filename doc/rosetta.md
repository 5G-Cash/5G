Rosetta API
===========

This project includes a lightweight [Rosetta](https://www.rosetta-api.org/)
API server that exposes blockchain data from a running 5G node. It
implements the Rosetta Data API and is intended for developers
experimenting with Rosetta tooling.

Running the server
------------------

1. Ensure a 5G daemon is running with RPC enabled.
2. Install the dependencies:
   ```bash
   pip install -r contrib/rosetta/requirements.txt
   ```
3. Export RPC credentials and optional settings for the node:
   ```bash
   export ROSETTA_RPC_URL="http://127.0.0.1:9340"
   export ROSETTA_RPC_USER="user"
   export ROSETTA_RPC_PASSWORD="password"
   export ROSETTA_NETWORK="mainnet"    # optional
   export ROSETTA_PORT="8080"          # optional
   export ROSETTA_RPC_TIMEOUT="10"     # optional
   ```
4. Start the server:
   ```bash
   python contrib/rosetta/rosetta_server.py
   ```

The server implements ``/`` for a basic health check and the following
Rosetta Data API endpoints:

* ``/network/list``
* ``/network/status``
* ``/network/options``
* ``/block``
* ``/block/transaction``
* ``/mempool``
* ``/mempool/transaction``
* ``/account/balance``
* ``/account/coins``

It listens on ``localhost:8080`` by default.
