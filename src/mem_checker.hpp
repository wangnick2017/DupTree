#ifndef DUPTREE_MEM_CHECKER_HPP
#define DUPTREE_MEM_CHECKER_HPP

#include "io.hpp"

class MemChecker {
protected:
    IO *io = nullptr;
public:
    virtual bool init(IO *io, bool create_db, std::string *values) = 0;
    virtual void update(const std::string &spos, const std::string &value) = 0;
    virtual std::pair<Int, Int> commit() = 0;
    virtual std::string gen_proof(const std::string &spos) = 0;
    virtual bool verify_proof(const std::string &spos, const std::string &value, const std::string &proof) = 0;
    virtual std::string get_name() = 0;
    virtual ~MemChecker() = default;
};

class MerkleBase : public MemChecker {
protected:
    std::string digest;
    Int num_leaf, height;

    explicit MerkleBase(Int height) {
        this->height = height;
        this->num_leaf = 1LL << height;
        digest = "";
    }

public:
    std::pair<Int, Int> commit() override {}
    bool verify_proof(const std::string &spos, const std::string &value, const std::string &proof) override {
        std::string s, key = value;
        Int pos = std::stoi(spos);
        for (Int i = 0, id = pos + num_leaf; id >= 2; i += 64, id /= 2) {
            s = proof.substr(i, 64);
            key = calculateSHA256Hash((id & 1) == 0 ? key.append(s) : s.append(key));
        }
        return key == digest;
    }

    std::string get_digest() {
        return digest;
    }

    ~MerkleBase() override {
        io->flush();
    }
};

#endif //DUPTREE_MEM_CHECKER_HPP
