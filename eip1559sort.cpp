#include <cassert>
#include <chrono>
#include <iostream>
#include <map>
#include <queue>
#include <random>
#include <vector>
using namespace std;

struct Tx {
    int fee_cap;
    int max_miner_bribe;
    int hash;

    Tx() {}
    Tx(int fee_cap, int max_miner_bribe, int hash)
        : fee_cap(fee_cap), max_miner_bribe(max_miner_bribe), hash(hash){};

    int miner_bribe(int base_fee) {
        return min(fee_cap - base_fee, max_miner_bribe);
    }

    void _print() {
        cerr << "fee_cap: " << fee_cap
             << ", max_miner_bribe: " << max_miner_bribe << ", hash: " << hash
             << endl;
    }
};

using Key = tuple<int, int, int>;

struct TxPool {
    int base_fee;
    TxPool(){};

    virtual void add_tx(Tx tx) = 0;
    virtual Tx pop_most_profitable_tx() = 0;
    virtual void reconstruct(int base_fee) = 0;
    virtual size_t size() = 0;
};

struct NaiveTxPool : TxPool {
    map<Key, Tx> sbst;

    // O(log n)
    void add_tx(Tx tx) {
        sbst.emplace(Key(tx.miner_bribe(base_fee), -tx.fee_cap, tx.hash), tx);
    }

    // O(log n)
    Tx pop_most_profitable_tx() {
        assert(sbst.size() > 0);
        Tx tx = sbst.rbegin()->second;
        sbst.erase(prev(sbst.end()));
        return tx;
    }

    // O(n log n)
    void reconstruct(int new_base_fee) {
        int prev_base_fee = base_fee;
        base_fee = new_base_fee;

        map<Key, Tx> new_sbst;
        for(auto elem : sbst) {
            Tx tx = elem.second;
            new_sbst.emplace(
                Key(tx.miner_bribe(base_fee), -tx.fee_cap, tx.hash), tx);
        }
        sbst = new_sbst;
    }

    size_t size() { return sbst.size(); }
};

struct FastTxPool : TxPool {
    map<Key, Tx> sbst_static;
    map<Key, Tx> sbst_dynamic;
    map<Key, Tx> sbst_decision;

    // O(log n)
    void add_tx(Tx tx) {
        if(tx.fee_cap - base_fee >= tx.max_miner_bribe) {
            sbst_static.emplace(Key(tx.max_miner_bribe, -tx.fee_cap, tx.hash),
                                tx);
        } else {
            sbst_dynamic.emplace(Key(tx.fee_cap, -tx.fee_cap, tx.hash), tx);
        }
        sbst_decision.emplace(
            Key(tx.fee_cap - tx.max_miner_bribe, -tx.fee_cap, tx.hash), tx);
    }

    // O(log n)
    Tx pop_most_profitable_tx() {
        Tx tx;
        assert(sbst_decision.size() > 0);
        if(sbst_static.size() == 0) {
            tx = sbst_dynamic.rbegin()->second;
            sbst_dynamic.erase(prev(sbst_dynamic.end()));
        } else if(sbst_dynamic.size() == 0) {
            tx = sbst_static.rbegin()->second;
            sbst_static.erase(prev(sbst_static.end()));
        } else {
            Tx tx_static = sbst_static.rbegin()->second;
            Tx tx_dynamic = sbst_dynamic.rbegin()->second;

            if(tx_static.miner_bribe(base_fee) >
               tx_dynamic.miner_bribe(base_fee)) {
                tx = tx_static;
                sbst_static.erase(prev(sbst_static.end()));
            } else {
                tx = tx_dynamic;
                sbst_dynamic.erase(prev(sbst_dynamic.end()));
            }
        }

        sbst_decision.erase(sbst_decision.find(
            Key(tx.fee_cap - tx.max_miner_bribe, -tx.fee_cap, tx.hash)));
        return tx;
    }

    // O(k log n)
    // k is a small constant when the change of base fee is small.
    void reconstruct(int new_base_fee) {
        int prev_base_fee = base_fee;
        base_fee = new_base_fee;

        if(prev_base_fee < base_fee) {
            // from S to D
            // [prev_base_fee, base_fee)
            auto left_tx =
                sbst_decision.lower_bound(Key(prev_base_fee, -INT32_MAX, 0));
            auto right_tx =
                sbst_decision.lower_bound(Key(base_fee, -INT32_MAX, 0));
            for(auto pointer = left_tx; pointer != right_tx; pointer++) {
                Tx tx = pointer->second;
                if(tx.fee_cap - tx.max_miner_bribe >= prev_base_fee) {
                    sbst_static.erase(sbst_static.find(
                        Key(tx.max_miner_bribe, -tx.fee_cap, tx.hash)));
                    add_tx(tx);
                }
            }
        } else if(base_fee < prev_base_fee) {
            // from D to S
            // [base_fee, prev_base_fee)
            auto left_tx =
                sbst_decision.lower_bound(Key(base_fee, -INT32_MAX, 0));
            auto right_tx =
                sbst_decision.lower_bound(Key(prev_base_fee, -INT32_MAX, 0));
            for(auto pointer = left_tx; pointer != right_tx; pointer++) {
                Tx tx = pointer->second;
                if(tx.fee_cap - tx.max_miner_bribe < prev_base_fee) {
                    sbst_dynamic.erase(sbst_dynamic.find(
                        Key(tx.fee_cap, -tx.fee_cap, tx.hash)));
                    add_tx(tx);
                }
            }
        }
        // _check_invalid();
    }

    size_t size() { return sbst_decision.size(); }

    void _print() {
        for(auto elem : sbst_static) {
            cerr << "sbst_static: ";
            elem.second._print();
        }
        for(auto elem : sbst_dynamic) {
            cerr << "sbst_dynamic: ";
            elem.second._print();
        }
        for(auto elem : sbst_decision) {
            cerr << "sbst_decision: ";
            elem.second._print();
        }
    }

    void _check_invalid() {
        for(auto elem : sbst_static) {
            auto tx = elem.second;
            if(tx.fee_cap - tx.max_miner_bribe < base_fee) {
                cerr << "error" << endl;
                cerr << "static => dynamic" << endl;
                tx._print();
                assert(false);
            }
        }
        for(auto elem : sbst_dynamic) {
            auto tx = elem.second;
            if(tx.fee_cap - tx.max_miner_bribe >= base_fee) {
                cerr << "error" << endl;
                cerr << "dynamic => static" << endl;
                tx._print();
                assert(false);
            }
        }
    }
};

void test_txpool(TxPool &txpool, int seed = 0) {
    const int END_BLOCK_HEIGHT = 100;
    const int INITIAL_BASE_FEE = 30;
    const int MIN_BASE_FEE = 10;
    const int FLUCTUATION_AMOUNT_OF_BASE_FEE = 5;
    const int INITIAL_TX_NUM = 10000;
    const int ADD_TX_NUM = 100;
    const int POP_TX_NUM = 100;
    const int MAX_FEE_CAP = 200;
    const int MAX_MAX_MINER_BRIBE = 100;
    const int GAS_USED = 1;

    chrono::system_clock::time_point start, end;
    start = chrono::system_clock::now();
    mt19937_64 mt(seed);

    auto generate_txs = [&mt](int tx_num, int max_fee_cap,
                              int max_max_miner_bribe) {
        vector<Tx> txs;
        for(int i = 0; i < tx_num; i++) {
            int fee_cap = mt() % max_fee_cap;
            int max_miner_bribe = mt() % max_max_miner_bribe;
            int hash = mt() % INT32_MAX;
            txs.emplace_back(fee_cap, max_miner_bribe, hash);
        }
        return txs;
    };

    int base_fee = INITIAL_BASE_FEE;
    txpool.base_fee = base_fee;

    vector<Tx> txs =
        generate_txs(INITIAL_TX_NUM, MAX_FEE_CAP, MAX_MAX_MINER_BRIBE);
    for(auto tx : txs) {
        txpool.add_tx(tx);
    }

    long long fee = 0;
    for(int block_height = 0; block_height < END_BLOCK_HEIGHT; block_height++) {
        vector<Tx> txs =
            generate_txs(ADD_TX_NUM, MAX_FEE_CAP, MAX_MAX_MINER_BRIBE);
        for(auto tx : txs) {
            txpool.add_tx(tx);
        }
        base_fee =
            max(MIN_BASE_FEE,
                base_fee - FLUCTUATION_AMOUNT_OF_BASE_FEE +
                    (int)mt() % (2 * FLUCTUATION_AMOUNT_OF_BASE_FEE + 1));
        txpool.reconstruct(base_fee);
        for(int i = 0; i < POP_TX_NUM; i++) {
            Tx tx = txpool.pop_most_profitable_tx();
            if(tx.fee_cap < base_fee) {
                txpool.add_tx(tx);
                continue;
            }
            fee += tx.miner_bribe(base_fee) * GAS_USED;
        }
    }
    cout << "fee earned: " << fee << endl;

    end = chrono::system_clock::now();
    double elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();
    cout << elapsed << " ms" << endl;
}

int main() {

    // O(block_height * n log n)
    cout << "Naive txpool: " << endl;
    NaiveTxPool txpool1 = NaiveTxPool();
    test_txpool(txpool1);

    // O(block_height * k log n)
    // if FLUCTUATION_AMOUNT_OF_BASE_FEE is small, k << n
    cout << "Proposed txpool: " << endl;
    FastTxPool txpool2 = FastTxPool();
    test_txpool(txpool2);

    return 0;
}