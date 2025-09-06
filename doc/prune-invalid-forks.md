# Pruning invalid forks

Nodes can permanently discard unwanted branches using the existing `invalidateblock` RPC.

```bash
fiveg-cli invalidateblock <blockhash>
```

This command marks the specified block and all of its descendants as invalid and
removes them from the block index.  After invalidation:

- the pruned blocks are deleted from both disk and memory
- fork warnings stop, because no invalid chain is tracked
- `getchaintips` reports only the active chain tip

Use this command when a rogue fork appears or when you wish to pin your node to a
known-good chain tip.
