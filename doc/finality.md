# BFT Finality

5G-CASH includes a basic BFT-style finality gadget. Validators vote on block
hashes and once a height gathers enough votes the corresponding block is marked
as finalized. Nodes refuse to reorganize the chain below the finalized height.

Finality is governed by `SPORK_17_BFT_FINALITY_ENABLED`, which defaults to active on
all networks. Operators can disable the spork to temporarily revert to standard
longest-chain rules. Use the `getfinalityinfo` RPC to inspect the latest finalized block.
