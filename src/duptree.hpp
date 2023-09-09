#ifndef DUPTREE_DUPTREE_HPP
#define DUPTREE_DUPTREE_HPP

#include <utility>
#include <vector>
#include "tools.hpp"
#include "io.hpp"
#include "mem_checker.hpp"
#include "node.hpp"

class DupTree : public MerkleBase {
protected:
    Int boundary, height_boundary;

    virtual void modify_id_level(Int id, Int level, const std::string &key) = 0;
    virtual void read_self(Int id, std::vector<std::string> &self_proofs) = 0;
    virtual void get_high(Int id, std::string &output) = 0;

    Node gen_node(Int id, Int level, std::string *values) {
        if (id >= num_leaf) {
            Node leaf(values == nullptr ? calculateSHA256Hash(random_string()) : values[id - num_leaf]);
            io->write(itos(id), leaf.to_string());
            return leaf;
        }
        Node left_child = gen_node(id * 2, level + 1, values);
        Node right_child = gen_node(id * 2 + 1, level + 1, values);
        Node node(left_child, right_child);
        if (id >= boundary) {
            io->write(itos(id), node.to_string());
        } else if (id != 1) {
            Int sibling = id / 2 * 4 + 1 - id;
            Int l = up_to(sibling, boundary);
            Int r = up_to_max(sibling, boundary);
            for (Int i = l; i <= r; ++i) {
                modify_id_level(i, level, node.get_hash_val());
            }
        }
        return node;
    }

public:
    explicit DupTree(Int height, Int height_boundary) : MerkleBase(height) {
        this->boundary = 1LL << height_boundary;
        this->height_boundary = height_boundary;
    }

    bool init(IO *io, bool create_db, std::string *values) override {
        this->io = io;
        if (!this->io->open()) {
            return false;
        }
        if (create_db) {
            digest = gen_node(1, 1, values).get_hash_val();
            io->write("1", digest);
            this->io->flush();
        } else {
            io->read("1", digest);
        }
        return true;
    }

    void update(const std::string &spos, const std::string &value) override {
        std::string key = value;
        Int pos = std::stoi(spos);
        io->write(itos(pos + num_leaf), key);
        std::string s;
        Int id = pos + num_leaf;
        for (; id >= boundary; id /= 2) {
            io->read(itos(id / 2 * 4 + 1 - id), s);
            key = calculateSHA256Hash((id & 1) == 0 ? key.append(s) : s.append(key));
            if (id / 2 >= boundary)
                io->write(itos(id / 2), key);
        }

        std::vector<std::string> self_proofs;
        read_self(id, self_proofs);

        for (Int i = height_boundary; i > 1; i--, id /= 2) {
            Int sibling = id / 2 * 4 + 1 - id;
            Int l = up_to(sibling, boundary);
            Int r = up_to_max(sibling, boundary);
            for (Int j = l; j <= r; ++j) {
                modify_id_level(j, i, key);
            }

            s = self_proofs[height_boundary - i];
            key = calculateSHA256Hash((id & 1) == 0 ? key.append(s) : s.append(key));
        }
        digest = key;
    }

    std::string gen_proof(const std::string &spos) override {
        std::string s, output;
        Int pos = std::stoi(spos);
        Int id = pos + num_leaf;
        for (; id >= boundary; id /= 2) {
            io->read(itos(id / 2 * 4 + 1 - id), s);
            output.append(s);
        }
        get_high(id, output);
        return output;
    }
};

class DupTreeSimple : public DupTree {
    void modify_id_level(Int id, Int level, const std::string &key) override {
        io->write(itos(id) + "-" + itos(level), key);
    }
    void get_high(Int id, std::string &output) override {
        std::string s;
        for (Int i = height_boundary; i > 1; i--) {
            io->read(itos(id) + "-" + itos(i), s);
            output.append(s);
        }
    }
    void read_self(Int id, std::vector<std::string> &self_proofs) override {
        std::string s;
        for (Int i = height_boundary; i > 1; i--) {
            io->read(itos(id) + "-" + itos(i), s);
            self_proofs.push_back(s);
        }
    }
public:
    DupTreeSimple(Int height, Int height_boundary) : DupTree(height, height_boundary) {}
    std::string get_name() override {
        return "duptree_simple";
    }
};

class DupTreeBlock : public DupTree {
    void modify_id_level(Int id, Int level, const std::string &key) override {
        std::string pre;
        if (!io->read(itos(id), pre) || pre.empty()) {
            pre = std::string((height_boundary - 1) * 64, '0').replace((level - 2) * 64, 64, key);
        } else {
            pre = pre.replace((height_boundary - level) * 64, 64, key);
        }
        io->write(itos(id), pre);
    }
    void get_high(Int id, std::string &output) override {
        std::string s;
        io->read(itos(id), s);
        output.append(s);
    }
    void read_self(Int id, std::vector<std::string> &self_proofs) override {
        std::string s;
        io->read(itos(id), s);
        for (Int i = 0; i < s.length(); i += 64) {
            self_proofs.push_back(s.substr(i, 64));
        }
    }
public:
    DupTreeBlock(Int height, Int height_boundary) : DupTree(height, height_boundary) {}
    std::string get_name() override {
        return "duptree_block";
    }
};

#endif //DUPTREE_DUPTREE_HPP
