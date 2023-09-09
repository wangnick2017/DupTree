#ifndef DUPTREE_DUPTREE_PLUS_HPP
#define DUPTREE_DUPTREE_PLUS_HPP

#include "mem_checker.hpp"
#include "duptree.hpp"

template <class DUP>
class DupTreePlus : public MerkleBase {
    Int base_height{};
    Int P, Pl, num_blocks;
    MerkleBase *base_tree[2]{};
    IOMultiple *iom;

    std::string gen_node(Int id, std::string *values) {
        if (id >= num_blocks) {
            iom->change_id(id);
            if (values == nullptr) {
                std::string vals[Pl];
                for (Int i = 0; i < Pl; ++i) {
                    vals[i] = calculateSHA256Hash(random_string());
                }
                base_tree[1]->init(iom, true, vals);
            } else {
                base_tree[1]->init(iom, true, values + ((id - num_blocks) * Pl));
            }
            return base_tree[1]->get_digest();
        }
        std::string vals[P];
        for (Int i = 0; i < P; ++i) {
            vals[i] = gen_node((id - 1) * P + 2 + i, values);
        }
        iom->change_id(id);
        base_tree[0]->init(iom, true, vals);
        return base_tree[0]->get_digest();
    }

public:
    DupTreePlus(Int height, Int base_height, Int base_height_boundary) : MerkleBase(height) {
        this->base_height = base_height;
        P = 1LL << base_height;
        base_tree[0] = new DUP(base_height, base_height_boundary);
        if (height % base_height == 0) {
            Pl = P;
            num_blocks = (num_leaf - P) / (P * (P - 1)) + 1;
            base_tree[1] = base_tree[0];
        } else {
            Pl = 1LL << (height % base_height);
            num_blocks = ((1LL << ((height / base_height + 1) * base_height)) - P) / (P * (P - 1)) + 1;
            base_tree[1] = new DUP(height % base_height, std::min(height % base_height, base_height_boundary));
        }
        iom = nullptr;
    }

    bool init(IO *io, bool create_db, std::string *values) override {
        this->io = io;
        iom = new IOMultiple(io);
        if (!this->io->open()) {
            return false;
        }
        if (create_db) {
            digest = gen_node(1, values);
            this->io->flush();
        } else {
            base_tree[0]->init(iom, false, nullptr);
            digest = base_tree[0]->get_digest();
        }
        return true;
    }

    void update(const std::string &spos, const std::string &value) override {
        Int pos = std::stoi(spos);
        Int p = pos / Pl;
        std::string v(value);
        for (Int id = p + num_blocks; ; pos = pos / (id >= num_blocks ? Pl : P), id = (id - 2) / P + 1) {
            iom->change_id(id);
            base_tree[id >= num_blocks]->update(itos(pos % (id >= num_blocks ? Pl : P)), v);
            v = base_tree[id >= num_blocks]->get_digest();
            if (id == 1) {
                break;
            }
        }
        digest = v;
    }

    std::string gen_proof(const std::string &spos) override {
        Int pos = std::stoi(spos);
        std::string output;
        Int p = pos / Pl;
        for (Int id = p + num_blocks; ; pos = pos / (id >= num_blocks ? Pl : P), id = (id - 2) / P + 1) {
            iom->change_id(id);
            output.append(base_tree[id >= num_blocks]->gen_proof(itos(pos % (id >= num_blocks ? Pl : P))));
            if (id == 1) {
                break;
            }
        }
        return output;
    }

    std::string get_name() override {
        return "duptree_plus[" + base_tree[0]->get_name() + "]";
    }

    ~DupTreePlus() override {
        if (base_tree[1] != base_tree[0]) {
            delete base_tree[1];
        }
        delete base_tree[0];
        delete iom;
    }
};

#endif //DUPTREE_DUPTREE_PLUS_HPP
