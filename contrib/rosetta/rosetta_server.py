#!/usr/bin/env python3
# Copyright (c) 2024 The 5G-CASH developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Rosetta API server for 5G-CASH.

This lightweight implementation proxies Rosetta Data API requests to a
running 5G node using RPC calls. It is intentionally simple but covers
the common Rosetta endpoints required by most tooling.
"""

import json
import os
import time

import requests
from flask import Flask, Response, jsonify, request
from flask_limiter import Limiter
from flask_limiter.util import get_remote_address

RPC_URL = os.environ.get("ROSETTA_RPC_URL", "http://127.0.0.1:9340")
RPC_USER = os.environ.get("ROSETTA_RPC_USER", "")
RPC_PASSWORD = os.environ.get("ROSETTA_RPC_PASSWORD", "")
NETWORK_NAME = os.environ.get("ROSETTA_NETWORK", "mainnet")
RPC_TIMEOUT = int(os.environ.get("ROSETTA_RPC_TIMEOUT", "10"))
ALLOWED_CALL_METHODS = set(
    filter(None, os.environ.get("ROSETTA_CALL_ALLOWED", "").split(","))
)
API_KEYS = set(filter(None, os.environ.get("ROSETTA_API_KEYS", "").split(",")))
IP_WHITELIST = set(
    filter(None, os.environ.get("ROSETTA_IP_WHITELIST", "").split(","))
)
RATE_LIMIT = os.environ.get("ROSETTA_RATE_LIMIT", "100 per minute")

app = Flask(__name__)
limiter = Limiter(get_remote_address, app=app, default_limits=[RATE_LIMIT])


ERRORS = {
    "unauthorized": {"code": 1, "message": "unauthorized", "retriable": False},
    "forbidden": {"code": 2, "message": "forbidden", "retriable": False},
    "not_allowed": {
        "code": 3,
        "message": "method not allowed",
        "retriable": False,
    },
    "internal": {"code": 4, "message": "internal error", "retriable": True},
}


def rosetta_error(name, status, details=None):
    err = ERRORS[name].copy()
    err["details"] = details or {}
    return jsonify(err), status


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


@app.before_request
def auth_and_ip():
    if IP_WHITELIST and request.remote_addr not in IP_WHITELIST:
        return rosetta_error("forbidden", 403)
    if API_KEYS:
        key = request.headers.get("X-Api-Key") or request.args.get("api_key")
        if key not in API_KEYS:
            return rosetta_error("unauthorized", 401)


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


@app.route("/network/peers", methods=["POST"])
def network_peers():
    """Return the set of peers the node is connected to."""
    try:
        peer_info = rpc("getpeerinfo")
    except Exception as exc:  # pragma: no cover - network errors
        return rosetta_error("internal", 500, {"message": str(exc)})
    peers = [{"peer_id": p.get("addr", "")} for p in peer_info]
    return jsonify({"peers": peers})


@app.route("/call", methods=["POST"])
def call_endpoint():
    """Proxy whitelisted RPC methods through Rosetta's optional Call API."""
    payload = request.json or {}
    method = payload.get("method")
    if method not in ALLOWED_CALL_METHODS:
        return rosetta_error("not_allowed", 403, {"method": method})
    params = payload.get("params", [])
    result = rpc(method, params)
    return jsonify({"result": result})


@app.route("/block", methods=["POST"])
def block():
    """Return a block by index or hash."""
    identifier = request.json.get("block_identifier", {})
    if "hash" in identifier:
        block_hash = identifier["hash"]
    else:
        block_hash = rpc("getblockhash", [identifier.get("index", 0)])
    data = rpc("getblock", [block_hash, 2])
    transactions = []
    for tx in data.get("tx", []):
        ops = []
        idx = 0
        for vin in tx.get("vin", []):
            if "coinbase" in vin:
                continue
            try:
                prev = rpc("getrawtransaction", [vin["txid"], True])
                vout = prev["vout"][vin["vout"]]
                amount = vout["value"]
                addrs = vout.get("scriptPubKey", {}).get("addresses", [])
                addr = addrs[0] if addrs else ""
            except Exception:
                amount = 0
                addr = ""
            ops.append(
                {
                    "operation_identifier": {"index": idx},
                    "type": "TRANSFER",
                    "account": {"address": addr},
                    "amount": {
                        "value": f"-{amount}",
                        "currency": {"symbol": "5G", "decimals": 8},
                    },
                    "coin_change": {
                        "coin_identifier": {
                            "identifier": f"{vin['txid']}:{vin['vout']}"
                        },
                        "coin_action": "coin_spent",
                    },
                }
            )
            idx += 1
        for vout in tx.get("vout", []):
            addrs = vout.get("scriptPubKey", {}).get("addresses", [])
            addr = addrs[0] if addrs else ""
            ops.append(
                {
                    "operation_identifier": {"index": idx},
                    "type": "TRANSFER",
                    "account": {"address": addr},
                    "amount": {
                        "value": str(vout["value"]),
                        "currency": {"symbol": "5G", "decimals": 8},
                    },
                    "coin_change": {
                        "coin_identifier": {
                            "identifier": f"{tx['txid']}:{vout['n']}"
                        },
                        "coin_action": "coin_created",
                    },
                }
            )
            idx += 1
        transactions.append(
            {
                "transaction_identifier": {"hash": tx["txid"]},
                "operations": ops,
            }
        )
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
            "transactions": transactions,
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


@app.route("/account/history", methods=["POST"])
def account_history():
    """Return recent transactions for an address."""
    address = request.json["account_identifier"]["address"]
    depth = int(os.environ.get("ROSETTA_HISTORY_DEPTH", "100"))
    info = rpc("getblockchaininfo")
    height = info["blocks"]
    matches = []
    # Scan mempool
    try:
        txids = rpc("getrawmempool")
        for txid in txids:
            data = rpc("getrawtransaction", [txid, True])
            for vout in data.get("vout", []):
                addrs = vout.get("scriptPubKey", {}).get("addresses", [])
                if address in addrs:
                    matches.append(
                        {
                            "transaction_identifier": {"hash": txid},
                            "metadata": data,
                        }
                    )
                    break
    except Exception:
        pass
    # Scan recent blocks
    start = max(0, height - depth)
    for i in range(height, start - 1, -1):
        block_hash = rpc("getblockhash", [i])
        block = rpc("getblock", [block_hash])
        for txid in block.get("tx", []):
            data = rpc("getrawtransaction", [txid, True, block_hash])
            for vout in data.get("vout", []):
                addrs = vout.get("scriptPubKey", {}).get("addresses", [])
                if address in addrs:
                    matches.append(
                        {
                            "transaction_identifier": {"hash": txid},
                            "metadata": data,
                        }
                    )
                    break
    return jsonify({"transactions": matches})


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


@app.route("/mempool/fees", methods=["POST"])
def mempool_fees():
    """Estimate current fee rates."""
    target = request.json.get("target_block_count", 6)
    result = rpc("estimatesmartfee", [target])
    return jsonify({"fee": result.get("feerate")})


@app.route("/mempool/submit", methods=["POST"])
def mempool_submit():
    """Submit a raw transaction to the network."""
    tx_hex = request.json.get("transaction")
    txid = rpc("sendrawtransaction", [tx_hex])
    return jsonify({"transaction_identifier": {"hash": txid}})


@app.route("/search/transactions", methods=["POST"])
def search_transactions():
    """Search transactions by identifier or address in recent data."""
    req = request.json or {}
    tx_id = req.get("transaction_identifier", {}).get("hash")
    address = req.get("address")
    matches = []
    if tx_id:
        try:
            tx = rpc("getrawtransaction", [tx_id, True])
            matches.append(
                {"transaction_identifier": {"hash": tx_id}, "metadata": tx}
            )
        except Exception:
            pass
    if address:
        try:
            txids = rpc("getrawmempool")
            for txhash in txids:
                data = rpc("getrawtransaction", [txhash, True])
                for vout in data.get("vout", []):
                    addrs = vout.get("scriptPubKey", {}).get("addresses", [])
                    if address in addrs:
                        matches.append(
                            {
                                "transaction_identifier": {"hash": txhash},
                                "metadata": data,
                            }
                        )
                        break
        except Exception:
            pass
    return jsonify({"transactions": matches})


@app.route("/events/blocks", methods=["GET"])
def events_blocks():
    """Stream new block hashes using server-sent events."""
    def generate():
        last = rpc("getbestblockhash")
        payload = json.dumps(
            {"block_identifier": {"hash": last}, "sequence": 0}
        )
        yield f"data: {payload}\n\n"
        seq = 1
        while True:
            try:
                current = rpc("getbestblockhash")
            except Exception:  # pragma: no cover - network errors
                time.sleep(5)
                continue
            if current != last:
                try:
                    prev = rpc("getblock", [current])["previousblockhash"]
                except Exception:
                    prev = None
                if prev and prev != last:
                    removed = json.dumps(
                        {
                            "block_identifier": {"hash": last},
                            "type": "block_removed",
                            "sequence": seq,
                        }
                    )
                    yield f"data: {removed}\n\n"
                    seq += 1
                added = json.dumps(
                    {
                        "block_identifier": {"hash": current},
                        "type": "block_added",
                        "sequence": seq,
                    }
                )
                yield f"data: {added}\n\n"
                seq += 1
                last = current
            time.sleep(5)

    return Response(generate(), mimetype="text/event-stream")


@app.route("/construction/derive", methods=["POST"])
def construction_derive():
    """Derive an address from a public key."""
    pubkey = request.json["public_key"]["hex"]
    try:
        addr = rpc("addmultisigaddress", [1, [pubkey]])["address"]
    except Exception as exc:
        return rosetta_error("internal", 500, {"message": str(exc)})
    return jsonify({"address": addr})


@app.route("/construction/preprocess", methods=["POST"])
def construction_preprocess():
    """Extract inputs and outputs from operations."""
    ops = request.json.get("operations", [])
    inputs, outputs = [], {}
    for op in ops:
        value = op.get("amount", {}).get("value", "0")
        if value.startswith("-"):
            cid = (
                op.get("coin_change", {})
                .get("coin_identifier", {})
                .get("identifier")
            )
            if cid:
                txid, vout = cid.split(":")
                inputs.append({"txid": txid, "vout": int(vout)})
        else:
            addr = op.get("account", {}).get("address")
            outputs[addr] = float(value)
    return jsonify({"options": {"inputs": inputs, "outputs": outputs}})


@app.route("/construction/metadata", methods=["POST"])
def construction_metadata():
    """Return fee estimate metadata."""
    fee = rpc("estimatesmartfee", [6]).get("feerate")
    return jsonify({"metadata": {"fee": fee}})


@app.route("/construction/payloads", methods=["POST"])
def construction_payloads():
    """Create an unsigned transaction and payloads."""
    opts = request.json.get("options", {})
    inputs = opts.get("inputs", [])
    outputs = opts.get("outputs", {})
    try:
        unsigned = rpc("createrawtransaction", [inputs, outputs])
    except Exception as exc:
        return rosetta_error("internal", 500, {"message": str(exc)})
    payload = {"hex_bytes": unsigned, "signature_type": "ecdsa"}
    return jsonify({"unsigned_transaction": unsigned, "payloads": [payload]})


@app.route("/construction/parse", methods=["POST"])
def construction_parse():
    """Parse a transaction into operations."""
    tx_hex = request.json.get("transaction")
    tx = rpc("decoderawtransaction", [tx_hex])
    ops = []
    idx = 0
    for vin in tx.get("vin", []):
        if "txid" not in vin:
            continue
        prev = rpc("getrawtransaction", [vin["txid"], True])
        vout = prev["vout"][vin["vout"]]
        amount = vout["value"]
        addrs = vout.get("scriptPubKey", {}).get("addresses", [])
        addr = addrs[0] if addrs else ""
        ops.append(
            {
                "operation_identifier": {"index": idx},
                "type": "TRANSFER",
                "account": {"address": addr},
                "amount": {
                    "value": f"-{amount}",
                    "currency": {"symbol": "5G", "decimals": 8},
                },
            }
        )
        idx += 1
    for vout in tx.get("vout", []):
        addrs = vout.get("scriptPubKey", {}).get("addresses", [])
        addr = addrs[0] if addrs else ""
        ops.append(
            {
                "operation_identifier": {"index": idx},
                "type": "TRANSFER",
                "account": {"address": addr},
                "amount": {
                    "value": str(vout["value"]),
                    "currency": {"symbol": "5G", "decimals": 8},
                },
            }
        )
        idx += 1
    return jsonify({"operations": ops})


@app.route("/construction/combine", methods=["POST"])
def construction_combine():
    """Combine signatures with an unsigned transaction."""
    unsigned = request.json.get("unsigned_transaction")
    sigs = [p.get("hex_bytes") for p in request.json.get("signatures", [])]
    try:
        raw = rpc("combinerawtransaction", [[unsigned] + sigs])
    except Exception as exc:
        return rosetta_error("internal", 500, {"message": str(exc)})
    return jsonify({"signed_transaction": raw})


@app.route("/construction/hash", methods=["POST"])
def construction_hash():
    """Compute the transaction identifier for a signed transaction."""
    tx_hex = request.json.get("signed_transaction")
    tx = rpc("decoderawtransaction", [tx_hex])
    return jsonify({"transaction_identifier": {"hash": tx["txid"]}})


@app.route("/construction/submit", methods=["POST"])
def construction_submit():
    """Submit a signed transaction to the network."""
    tx_hex = request.json.get("signed_transaction")
    try:
        txid = rpc("sendrawtransaction", [tx_hex])
    except Exception as exc:
        return rosetta_error("internal", 500, {"message": str(exc)})
    return jsonify({"transaction_identifier": {"hash": txid}})


def main():
    port = int(os.environ.get("ROSETTA_PORT", "8080"))
    cert = os.environ.get("ROSETTA_SSL_CERT")
    key = os.environ.get("ROSETTA_SSL_KEY")
    ssl_context = (cert, key) if cert and key else None
    app.run(host="0.0.0.0", port=port, ssl_context=ssl_context)


if __name__ == "__main__":
    main()
