#!/usr/bin/env python3
# Copyright (c) 2024 The 5G-CASH developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Rosetta API server for 5G-CASH.

This lightweight implementation proxies Rosetta Data API requests to a
running 5G node using RPC calls. It is intentionally simple but covers
the common Rosetta endpoints required by most tooling.
"""

import os

import requests
from flask import Flask, jsonify, request

RPC_URL = os.environ.get("ROSETTA_RPC_URL", "http://127.0.0.1:9340")
RPC_USER = os.environ.get("ROSETTA_RPC_USER", "")
RPC_PASSWORD = os.environ.get("ROSETTA_RPC_PASSWORD", "")
NETWORK_NAME = os.environ.get("ROSETTA_NETWORK", "mainnet")
RPC_TIMEOUT = int(os.environ.get("ROSETTA_RPC_TIMEOUT", "10"))

app = Flask(__name__)


def rpc(method, params=None):
    """Make an RPC call to the 5G daemon."""
    payload = {
        "jsonrpc": "1.0",
        "id": "rosetta",
        "method": method,
        "params": params or [],
    }
    response = requests.post(
        RPC_URL,
        json=payload,
        auth=(RPC_USER, RPC_PASSWORD),
        timeout=RPC_TIMEOUT,
    )
    response.raise_for_status()
    data = response.json()
    if data.get("error"):
        raise RuntimeError(data["error"])
    return data["result"]


NETWORK_IDENTIFIER = {"blockchain": "5G", "network": NETWORK_NAME}


@app.route("/", methods=["GET"])
def index():
    """Simple health check endpoint."""
    return jsonify({"status": "ok"})


@app.route("/network/list", methods=["POST"])
def network_list():
    """Return the list of supported networks."""
    return jsonify({"network_identifiers": [NETWORK_IDENTIFIER]})


@app.route("/network/status", methods=["POST"])
def network_status():
    """Return basic information about the blockchain."""
    info = rpc("getblockchaininfo")
    height = info["blocks"]
    block_hash = rpc("getblockhash", [height])
    block = rpc("getblock", [block_hash])
    genesis_hash = rpc("getblockhash", [0])
    return jsonify(
        {
            "current_block_identifier": {"index": height, "hash": block_hash},
            "current_block_timestamp": block["time"] * 1000,
            "genesis_block_identifier": {"index": 0, "hash": genesis_hash},
            "peers": [],
        }
    )


@app.route("/network/options", methods=["POST"])
def network_options():
    """Return implementation and version details."""
    info = rpc("getnetworkinfo")
    return jsonify(
        {
            "version": {
                "rosetta_version": "1.4.10",
                "node_version": info.get("subversion", ""),
                "middleware_version": "0.1",
            },
            "allow": {
                "operation_statuses": [],
                "operation_types": [],
                "errors": [],
                "historical_balance_lookup": True,
            },
        }
    )


@app.route("/block", methods=["POST"])
def block():
    """Return a block by index or hash."""
    identifier = request.json.get("block_identifier", {})
    if "hash" in identifier:
        block_hash = identifier["hash"]
    else:
        block_hash = rpc("getblockhash", [identifier.get("index", 0)])
    data = rpc("getblock", [block_hash])
    result = {
        "block": {
            "block_identifier": {
                "index": data["height"],
                "hash": data["hash"],
            },
            "parent_block_identifier": {
                "index": data["height"] - 1,
                "hash": data.get("previousblockhash", ""),
            },
            "timestamp": data["time"] * 1000,
            "transactions": [],
        }
    }
    return jsonify(result)


@app.route("/block/transaction", methods=["POST"])
def block_transaction():
    """Return a transaction contained in a block."""
    block_id = request.json["block_identifier"].get("hash")
    tx_id = request.json["transaction_identifier"]["hash"]
    data = rpc("getrawtransaction", [tx_id, True, block_id])
    return jsonify(
        {
            "transaction": {
                "transaction_identifier": {"hash": tx_id},
                "metadata": data,
            }
        }
    )


@app.route("/account/balance", methods=["POST"])
def account_balance():
    """Return an account balance for a given address."""
    address = request.json["account_identifier"]["address"]
    try:
        balance = rpc(
            "getaddressbalance", [{"addresses": [address]}]
        )["balance"]
    except Exception:
        # Fallback to wallet balance if address index is not available
        balance = rpc("getbalance", [address])
    return jsonify(
        {
            "balances": [
                {
                    "value": str(balance),
                    "currency": {"symbol": "5G", "decimals": 8},
                }
            ]
        }
    )


@app.route("/account/coins", methods=["POST"])
def account_coins():
    """Return UTXOs for a given address."""
    address = request.json["account_identifier"]["address"]
    utxos = rpc("listunspent", [0, 9999999, [address]])
    coins = []
    for utxo in utxos:
        identifier = f"{utxo['txid']}:{utxo['vout']}"
        coins.append(
            {
                "coin_identifier": {"identifier": identifier},
                "amount": {
                    "value": str(utxo["amount"]),
                    "currency": {"symbol": "5G", "decimals": 8},
                },
            }
        )
    return jsonify({"coins": coins})


@app.route("/mempool", methods=["POST"])
def mempool():
    """Return all transaction identifiers in the mempool."""
    txids = rpc("getrawmempool")
    ids = [{"hash": txid} for txid in txids]
    return jsonify({"transaction_identifiers": ids})


@app.route("/mempool/transaction", methods=["POST"])
def mempool_transaction():
    """Return a specific transaction from the mempool."""
    tx_id = request.json["transaction_identifier"]["hash"]
    data = rpc("getrawtransaction", [tx_id, True])
    return jsonify(
        {
            "transaction": {
                "transaction_identifier": {"hash": tx_id},
                "metadata": data,
            }
        }
    )


def main():
    port = int(os.environ.get("ROSETTA_PORT", "8080"))
    app.run(host="0.0.0.0", port=port)


if __name__ == "__main__":
    main()
