#ifndef DUPTREE_MERKLE_HPP
#define DUPTREE_MERKLE_HPP

#include <utility>
#include <vector>
#include "tools.hpp"
#include "io.hpp"
#include "node.hpp"
#include "mem_checker.hpp"

template <class N>
class MerkleTree : public MerkleBase {
private:
    N gen_node(Int id, std::string *values) {
        if (id >= num_leaf) {
            N leaf(values == nullptr ? calculateSHA256Hash(random_string()) : values[id - num_leaf]);
            io->write(itos(id), leaf.to_string());
            return leaf;
        }
        N left_child = gen_node(id * 2, values);
        N right_child = gen_node(id * 2 + 1, values);
        N node(left_child, right_child);
        io->write(itos(id), node.to_string());
        return node;
    }

    virtual void get_sibling(Int id, std::string &s) = 0;
    virtual void modify_parent(Int id, std::string &key) = 0;

public:
    explicit MerkleTree(Int height) : MerkleBase(height) {}

    bool init(IO *io, bool create_db, std::string *values) override {
        this->io = io;
        if (!this->io->open()) {
            return false;
        }
        if (create_db) {
            digest = gen_node(1, values).get_hash_val();
            this->io->flush();
        } else {
            io->read("1", digest);
            digest = N(digest).get_hash_val();
        }
        return true;
    }

    void update(const std::string &spos, const std::string &value) override {
        std::string key = value;
        Int pos = std::stoi(spos);
        io->write(itos(pos + num_leaf), key);
        for (Int id = pos + num_leaf; id >= 2; id /= 2) {
            modify_parent(id, key);
        }
        digest = key;
    }

    std::string gen_proof(const std::string &spos) override {
        std::string s, output;
        Int pos = std::stoi(spos);
        for (Int id = pos + num_leaf; id >= 2; id /= 2) {
            get_sibling(id, s);
            output.append(s);
        }
        return output;
    }
};

class MerkleSimple : public MerkleTree<Node> {
    void get_sibling(Int id, std::string &s) override {
        io->read(itos(id / 2 * 4 + 1 - id), s);
    }
    void modify_parent(Int id, std::string &key) override {
        std::string s;
        io->read(itos(id / 2 * 4 + 1 - id), s);
        key = calculateSHA256Hash((id & 1) == 0 ? key.append(s) : s.append(key));
        io->write(itos(id / 2), key);
    }
public:
    explicit MerkleSimple(Int height) : MerkleTree<Node>(height) {}
    std::string get_name() override {
        return "merkle_simple";
    }
};

class MerkleChild : public MerkleTree<NodeChild> {
    void get_sibling(Int id, std::string &s) override {
        io->read(itos(id / 2), s);
        s = s.substr((2 - (id & 1)) * 64, 64);
    }
    void modify_parent(Int id, std::string &key) override {
        std::string s;
        io->read(itos(id / 2), s);
        s = s.substr(64, 128).replace((id & 1) * 64, 64, key);
        key = calculateSHA256Hash(s);
        io->write(itos(id / 2), key + s);
    }
public:
    explicit MerkleChild(Int height) : MerkleTree<NodeChild>(height) {}
    std::string get_name() override {
        return "merkle_child";
    }
};


#endif //DUPTREE_MERKLE_HPP
