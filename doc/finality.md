# BFT Finality

5G-CASH includes a basic BFT-style finality gadget. Validators vote on block
hashes and once a height gathers enough votes the corresponding block is marked
as finalized. Nodes refuse to reorganize the chain below the finalized height.

Finality is always enabled on all networks. Use the `getfinalityinfo` RPC to inspect
the latest finalized block.
