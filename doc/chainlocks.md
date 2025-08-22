# ChainLocks Finality

5G-CASH integrates a ChainLocks-style finality layer. Masternode quorums sign the
hash of each block and broadcast a `CLSIG` message to the network. Once a block
receives a valid ChainLock signature, nodes mark it as `chainlocked` and reject
any alternative branches extending below that height. This prevents deep
reorganizations and protects against 51% attacks. ChainLocks protection is
always active and cannot be disabled at runtime.

Incoming `CLSIG` messages update the best chain lock and persist
that state across restarts via the `fChainLocked` flag in `CBlockIndex`.

Use the `getchainlockinfo` RPC to inspect the most recently locked block.
