# ChainLocks Finality

5G-CASH integrates a ChainLocks-style finality layer. Masternode quorums sign the
hash of each block and broadcast a `CLSIG` message to the network. Once a block
receives a valid ChainLock signature, nodes mark it as `chainlocked` and reject
any alternative branches extending below that height. This prevents deep
reorganizations and protects against 51% attacks. Protection is controlled by
`SPORK_16_CHAINLOCKS_ENABLED`, which defaults to active on mainnet, testnet, and regtest but can be disabled if needed.

ChainLock signatures contain threshold BLS attestations from a deterministic
masternode quorum. A node reconstructs the quorum for the target height and
verifies that at least 3 of the 5 members signed the block hash before the lock
is accepted.

Incoming `CLSIG` messages update the best chain lock and nodes persist the
locked block in the block index so the status survives restarts.

Use the `getchainlockinfo` RPC to inspect the most recently locked block and
whether ChainLocks are currently enabled.
