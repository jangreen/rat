### LKMM versions

This folder contains different variants of the Linux Kernel Memory Model (LKMM).
The models have been slightly modified to be used with RAT

- lkmm-v00: official version up to version 6.2 of the kernel.
- lkmm-v01: updated via [this](https://lkml.org/lkml/2022/11/16/1555) patch.
- lkmm-v02: updated via [this](https://lkml.org/lkml/2022/12/1/465) patch.
- lkmm-v03: updated via [this](https://lkml.org/lkml/2023/1/17/1044) patch
- lkmm-v04: suggested [here](https://lkml.org/lkml/2024/6/4/1071).

### References

Release-Acquire chains in LKMM
- https://github.com/paulmckrcu/litmus/issues/11
- https://lkml.org/lkml/2022/11/16/1555

PPO (no) subset of PO:
- https://github.com/torvalds/linux/commit/762e9357e713a2025db05bc36a36a7afc248f9d3

Plain accesses carry dependencies (deps over rfi):
- https://lkml.org/lkml/2022/12/1/465

Monotonicity (release to mb) in LKMM:
- https://lkml.org/lkml/2022/11/16/1555

New modelling of RMWs with MB semantics
- https://github.com/herd/herdtools7/pull/865
- https://lkml.org/lkml/2024/6/4/1071
