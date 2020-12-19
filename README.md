# EIP-1559 Fast Transaction Pool
WIP

# Complexity
- Add transaction: `O(log n)`
- Sort transactions when the base fee changes: `O(k log n)`
- Select the most profitable transaction: `O(log n)`
- Produce block: `O(m log n)`
- `n`: The number of transactions in a txpool
- `k`: The number of transactions where the miner bribe varies with basefee
- `m`: The number of transactions in a block

## Run

```
g++ eip1559sort.cpp -std=c++17
./a.out
```
