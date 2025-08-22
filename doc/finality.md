# BFT Finality

5G-CASH includes a basic BFT-style finality gadget. Validators vote on block
hashes and once a height gathers enough votes the corresponding block is marked
as finalized. Nodes refuse to reorganize the chain below the finalized height
while the feature is active.

Finality is enabled by default and can be disabled via the `SPORK_17_BFT_FINALITY_ENABLED`
spork. Use the `getfinalityinfo` RPC to inspect the latest finalized block.
