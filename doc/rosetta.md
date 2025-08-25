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
   export ROSETTA_CALL_ALLOWED="getblockchaininfo"  # optional
   export ROSETTA_API_KEYS="changeme"  # optional
   export ROSETTA_IP_WHITELIST="127.0.0.1"  # optional
   export ROSETTA_RATE_LIMIT="100 per minute"  # optional
   export ROSETTA_SSL_CERT="/path/cert.pem"   # optional
   export ROSETTA_SSL_KEY="/path/key.pem"     # optional
   export ROSETTA_HISTORY_DEPTH="100"         # optional
   ```

`ROSETTA_CALL_ALLOWED` is a comma-separated list of RPC methods that
the optional `/call` endpoint will proxy. `ROSETTA_API_KEYS`
specifies API keys required to access the server. If set, clients must
pass the key via the `X-Api-Key` header or `api_key` query parameter.
`ROSETTA_IP_WHITELIST` restricts access to specific IP addresses, and
`ROSETTA_RATE_LIMIT` configures request throttling. `ROSETTA_SSL_CERT`
and `ROSETTA_SSL_KEY` enable HTTPS, and `ROSETTA_HISTORY_DEPTH`
controls the `/account/history` scan depth.
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
* ``/account/history``
* ``/mempool/fees``
* ``/mempool/submit``
* ``/network/peers``
* ``/call``
* ``/search/transactions``
* ``/events/blocks``

It also exposes the Rosetta Construction API:

* ``/construction/derive``
* ``/construction/preprocess``
* ``/construction/metadata``
* ``/construction/payloads``
* ``/construction/parse``
* ``/construction/combine``
* ``/construction/hash``
* ``/construction/submit``

It listens on ``localhost:8080`` by default.

Example construction flow:

```bash
# Derive an address from a public key
curl -X POST localhost:8080/construction/derive -d '{"network_identifier":{},"public_key":{"hex":"<pubkey>"}}'

# Create payloads for signing
curl -X POST localhost:8080/construction/payloads -d '{"network_identifier":{},"operations":[],"options":{"inputs":[],"outputs":{}}}'

# Submit a signed transaction
curl -X POST localhost:8080/construction/submit -d '{"network_identifier":{},"signed_transaction":"<hex>"}'
```
