# SUGGESTED CHANGES TO FORCE-DRIVEN ML-RCS ALGORITHM

- Use the average instead of the maximum of q when computing C.
- Compute F only for nodes available to schedule in the current cycle, not everything in advance, to obtain more accurate estimates.
- Correct the distribution probability (with operation latencies > 1 it is not uniform; probability is higher in the middle).
- Instead of starting from the target latency given by ASAP, initialize it from an upper bound and then decrement it.
