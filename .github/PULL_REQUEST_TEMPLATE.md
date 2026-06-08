## Security Hardening Audit Review

### Description
This PR hardens privacy proof validation, Dandelion++ routing defaults, proof-of-stake modifier entropy, and deep reorganization handling.

### Security Checklist
- [ ] **Sigma Protocol:** Proof and anonymity set bounds are validated before cryptographic verification.
- [ ] **Dandelion++:** Stem routing has a larger destination set to reduce route predictability.
- [ ] **Consensus / Staking:** Stake modifier entropy includes an older ancestor hash.
- [ ] **Long-Range Mitigation:** Deep reorganization candidates are rejected on public networks.
- [ ] **Supply Chain:** CodeQL C/C++ scanning runs for pushes and pull requests.

### Testing Status
- [ ] Native unit tests compiled and executed with 0 errors.
- [ ] CodeQL static analysis passes without critical findings.
