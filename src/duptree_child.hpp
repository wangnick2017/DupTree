#ifndef DUPTREE_DUPTREE_CHILD_HPP
#define DUPTREE_DUPTREE_CHILD_HPP


#include <utility>
#include <vector>
#include "tools.hpp"
#include "io.hpp"
#include "mem_checker.hpp"
#include "node.hpp"

class DupTreeChild : public MerkleBase {
private:
    Int boundary;

    //gen db
    std::pair<NodeChild, std::string> gen_cal(Int id, std::string *values) {
        if (id >= boundary) {
            if (id >= num_leaf) {
                NodeChild leaf(values == nullptr ? calculateSHA256Hash(random_string()) : values[id - num_leaf]);
                io->write(itos(id), leaf.to_string());
                return std::make_pair(leaf, "");
            }
            auto left_child = gen_cal(id * 2, values);
            auto right_child = gen_cal(id * 2 + 1, values);
            NodeChild node(left_child.first, right_child.first);
            io->write(itos(id), node.to_string());
            return std::make_pair(node, "");
        }
        auto left_child = gen_cal(id * 2, values);
        auto right_child = gen_cal(id * 2 + 1, values);
        NodeChild node(left_child.first, right_child.first);
        if (id * 2 + 1 < boundary) {
            Int idp = up_to(id * 2 + 1, boundary);
            io->write(itos(idp), right_child.second);
        }
        return std::make_pair(node, left_child.second.append(node.to_string()));
    }

    void gen_merge(Int id) {
        if (id >= boundary) {
            return;
        }
        if (id * 2 + 1 < boundary) {
            Int idp = up_to(id * 2 + 1, boundary);
            std::string pre;
            io->read(itos(idp), pre);

            Int idl = up_to(id, boundary);
            std::string prel;
            io->read(itos(idl), prel);

            io->write(itos(idp), pre.append(prel.substr(pre.length())));
        }
        gen_merge(id * 2);
        gen_merge(id * 2 + 1);
    }

    //gen db
    std::pair<std::string, std::string> update_cal(Int id, Int pos, Int level, const std::string &new_val, const std::string &ref) {
        if (id * 2 >= boundary) {
            return std::make_pair(new_val.substr(0, 64), new_val);
        }
        Int lr = Int((id + id + 1 - (1LL << level)) == (pos >> (height - level)));
        auto up = update_cal(id * 2 + lr, pos, level + 1, new_val, ref);

        std::string s, key, v;
        s = ref.substr(ref.length() - 64 * 3 * level + 64, 128).replace(lr * 64, 64, up.first);
        key = calculateSHA256Hash(s);
        v = key + s;

        if (lr == 1) {
            Int idp = up_to(id * 2 + 1, boundary);
            io->write(itos(idp), up.second);
        }

        return std::make_pair(key, up.second.append(v));
    }

    void update_merge(Int id, Int pos, Int level, const std::string &ref) {
        if (id * 2 >= boundary) {
            return;
        }
        Int lr = Int((id + id + 1 - (1LL << level)) == (pos >> (height - level)));
        Int l = up_to(id * 2 + (1 - lr), boundary);
        Int r = up_to_max(id * 2 + (1 - lr), boundary);
        std::string s;
        Int start = (Int)ref.length() - 64 * 3 * level;
        std::string rem = ref.substr(start);
        for (Int i = l; i <= r; ++i) {
            io->read(itos(i), s);
            io->write(itos(i), s.replace(start, std::string::npos, rem));
        }

        if (lr == 1) {
            Int idp = up_to(id * 2 + 1, boundary);
            std::string pre;
            io->read(itos(idp), pre);
            io->write(itos(idp), pre.append(rem));
        }

        update_merge(id * 2 + lr, pos, level + 1, ref);
    }

public:
    explicit DupTreeChild(Int height, Int height_boundary) : MerkleBase(height) {
        this->boundary = 1LL << height_boundary;
    }

    bool init(IO *io, bool create_db, std::string *values) override {
        this->io = io;
        if (!this->io->open()) {
            return false;
        }
        if (create_db) {
            auto cal = gen_cal(1, values);
            io->write(itos(boundary / 2), cal.second);
            digest = cal.first.get_hash_val();
            gen_merge(1);
            this->io->flush();
        } else {
            std::string value;
            io->read(itos(boundary / 2), value);
            digest = value.substr(value.length() - 64 * 3, 64);
        }
        return true;
    }


    void update(const std::string &spos, const std::string &value) override {
        //++timestamp;
        std::string s, ss;
        std::string key = value;
        std::string siblings;

        std::string v = value;
        Int pos = std::stoi(spos);
        Int id = pos + num_leaf;
        for (; ; id /= 2) {
            io->write(itos(id), v);
            io->read(itos(id / 2), siblings);

            s = siblings.substr(64, 128).replace((id & 1) * 64, 64, key);
            key = calculateSHA256Hash(s);
            v = key + s;

            if (id / 2 < boundary) {
                break;
            }
        }

        auto cal = update_cal(1, pos, 1, v, siblings);
        digest = cal.first;
        update_merge(1, pos, 1, cal.second);
    }

    std::string gen_proof(const std::string &spos) override {//non
        Int lr;
        std::string s, output;
        Int pos = std::stoi(spos);
        Int id = pos + num_leaf;
        for (; id >= boundary; id /= 2) {
            io->read(itos(id / 2), s);
            lr = id & 1;
            output.append(s.substr((2 - lr) * 64, 64));
        }
        for (Int i = 64 * 3; id >= 2; i += 64 * 3, id /= 2) {
            lr = id & 1;
            output.append(s.substr(i + (2 - lr) * 64, 64));
        }
        return output;
    }

    std::string get_name() override {
        return "duptree_child";
    }
};


#endif //DUPTREE_DUPTREE_CHILD_HPP
