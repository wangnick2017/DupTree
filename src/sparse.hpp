#ifndef DUPTREE_SPARSE_HPP
#define DUPTREE_SPARSE_HPP

#include "mem_checker.hpp"

class NodeSparse {
public:
    std::string key, leftKey, leftHash, rightKey, rightHash, value;
    bool isRoot, isLeaf;
    explicit NodeSparse(const std::string &key, const std::string &value) {
        isLeaf = true;
        isRoot = false;
        this->key = key;
        this->value = value;
    }
    explicit NodeSparse(const std::string &key, const std::string &lk, const std::string &lh, const std::string &rk, const std::string &rh) {
        isLeaf = false;
        isRoot = false;
        this->key = key;
        this->leftKey = lk;
        this->leftHash = lh;
        this->rightKey = rk;
        this->rightHash = rh;
    }
    explicit NodeSparse(const std::string &key, IO *io) {
        std::string v;
        io->read(key, v);
        isRoot = key == "*";
        this->key = key;
        if (v[0] == '!') {
            isLeaf = true;
            this->value = v.substr(1);
        } else {
            isLeaf = false;
            auto q = v.find(',');
            if (q != 1) {
                auto p = v.find(':');
                leftKey = v.substr(0, p);
                leftHash = v.substr(p + 1, 64);
            }
            if (q != v.length() - 2) {
                v = v.substr(q + 1);
                auto p = v.find(':');
                rightKey = v.substr(0, p);
                rightHash = v.substr(p + 1, 64);
            }
        }
    }
    std::string to_string() {
        return isLeaf ? "!" + value : leftKey + ":" + leftHash + "," + rightKey + ":" + rightHash;
    }
    std::string computeHash() {
        return isLeaf ? calculateSHA256Hash(value) : calculateSHA256Hash(leftHash + rightHash);
    }
    void write(IO *io) {
        io->write(key, to_string());
    }
    bool isLeft(const std::string &k) {
        return is_prefix(k, (isRoot ? "" : key) + "0");
    }
    bool isLeftPrefix(const std::string &k) {
        return is_prefix(k, leftKey);
    }
    bool isRightPrefix(const std::string &k) {
        return is_prefix(k, rightKey);
    }
};

class SparseSimple : public MemChecker {

protected:
    std::string digest;
    Int height;



public:
    explicit SparseSimple(Int height) {
        this->height = height;
        digest = "";
    }

    bool init(IO *io, bool create_db, std::string *values) override {
        this->io = io;
        if (!this->io->open()) {
            return false;
        }
        if (create_db) {
            this->io->write("*", ":,:");
            this->io->flush();
            this->digest = calculateSHA256Hash("");
        } else {
            //not available
        }
        return true;
    }

    void update(const std::string &spos, const std::string &value) override {
        std::string bin(hex_to_binary(spos));
        NodeSparse root("*", io);
        std::vector<NodeSparse> stack;
        std::vector<int> lefts;
        stack.push_back(root);

        std::string hashUp;
        for ( ; ; ) {
            NodeSparse &cur = stack[stack.size() - 1];
            bool isLeft = cur.isLeft(bin);
            lefts.push_back(isLeft);
            if (isLeft && cur.leftKey.empty() || !isLeft && cur.rightKey.empty()) {
                NodeSparse newLeaf(bin, value);
                newLeaf.write(io);
                hashUp = newLeaf.computeHash();
                if (isLeft)
                    cur.leftKey = bin;
                else
                    cur.rightKey = bin;
                break;
            }
            if (isLeft && cur.leftKey == bin || !isLeft && cur.rightKey == bin) {
                NodeSparse newLeaf(bin, value);
                newLeaf.write(io);
                hashUp = newLeaf.computeHash();
                break;
            }
            if (isLeft && !cur.isLeftPrefix(bin)) {
                NodeSparse newLeaf(bin, value);
                newLeaf.write(io);
                hashUp = newLeaf.computeHash();
                std::string newKey = common_prefix(bin, cur.leftKey);
                bool l = is_prefix(bin, newKey + "0");
                NodeSparse newNode(newKey, l ? bin : cur.leftKey, l ? hashUp : cur.leftHash,
                                         l ? cur.leftKey : bin, l ? cur.leftHash : hashUp);
                newNode.write(io);
                cur.leftKey = newKey;
                hashUp = newNode.computeHash();
                break;
            }
            if (!isLeft && !cur.isRightPrefix(bin)) {
                NodeSparse newLeaf(bin, value);
                newLeaf.write(io);
                hashUp = newLeaf.computeHash();
                std::string newKey = common_prefix(bin, cur.rightKey);
                bool l = is_prefix(bin, newKey + "0");
                NodeSparse newNode(newKey, l ? bin : cur.rightKey, l ? hashUp : cur.rightHash,
                                         l ? cur.rightKey : bin, l ? cur.rightHash : hashUp);
                newNode.write(io);
                cur.rightKey = newKey;
                hashUp = newNode.computeHash();
                break;
            }
            stack.emplace_back(isLeft ? cur.leftKey : cur.rightKey, io);
        }
        for (Int i = stack.size() - 1; i >= 0; --i) {
            NodeSparse &cur = stack[i];
            if (lefts[i]) {
                cur.leftHash = hashUp;
            } else {
                cur.rightHash = hashUp;
            }
            hashUp = cur.computeHash();
            cur.write(io);
        }
        this->digest = hashUp;
    }

    std::string gen_proof(const std::string &spos) override {
        std::string bin(hex_to_binary(spos)), tmp;
        io->read(bin, tmp);
        if (tmp.empty())
            return "?";

        std::string output;
        std::string key = "*";

        for ( ; ; ) {
            NodeSparse cur(key, io);
            bool isLeft = cur.isLeft(bin);
            //if (cur.isRoot && (isLeft && cur.rightKey.empty() || !isLeft && cur.leftKey.empty())) {
            //} else
            {
                output.append(isLeft ? "0" : "1");
                std::string sibling(isLeft ? cur.rightHash : cur.leftHash);
                if (sibling.empty()) {
                    sibling = null64;
                }
                output.append(sibling);
            }
            if (isLeft && cur.leftKey == bin || !isLeft && cur.rightKey == bin) {
                break;
            }
            key = isLeft ? cur.leftKey : cur.rightKey;
        }
        return output;
    }

    bool verify_proof(const std::string &spos, const std::string &value, const std::string &proof) override {
        if (proof[0] == '?')
            return true;
        std::string s, key = calculateSHA256Hash(value);
        for (Int i = proof.length() - 65; i >= 0; i -= 65) {
            s = proof.substr(i + 1, 64);
            if (s[0] == '@')
                s = "";
            key = calculateSHA256Hash(proof[i] == '0' ? key.append(s) : s.append(key));
        }
        return key == digest;
    }

    std::string get_name() override {
        return "sparse_simple";
    }

    ~SparseSimple() override {
        io->flush();
    }
};

class SparseBalance : public MemChecker {

protected:
    std::string digest;
    Int num_leaf, height;



public:
    explicit SparseBalance(Int height) {
        this->height = height;
        this->num_leaf = 0;
        digest = "";
    }

    bool init(IO *io, bool create_db, std::string *values) override {
        this->io = io;
        if (!this->io->open()) {
            return false;
        }
        if (create_db) {
            this->io->write("*", ":,:");
            this->io->flush();
            this->digest = calculateSHA256Hash("");
        } else {
            //not available
        }
        return true;
    }

    void update(const std::string &spos, const std::string &value) override {


        std::string bin;

        io->read("?" + spos, bin);

        if (bin.empty()) {
            bin = int_to_binary(this->num_leaf, this->height);
            io->write("?" + spos, bin);
            this->num_leaf++;
        }

        NodeSparse root("*", io);
        std::vector<NodeSparse> stack;
        std::vector<int> lefts;
        stack.push_back(root);

        std::string hashUp;
        for ( ; ; ) {
            NodeSparse &cur = stack[stack.size() - 1];
            bool isLeft = cur.isLeft(bin);
            lefts.push_back(isLeft);
            if (isLeft && cur.leftKey.empty() || !isLeft && cur.rightKey.empty()) {
                NodeSparse newLeaf(bin, value);
                newLeaf.write(io);
                hashUp = newLeaf.computeHash();
                if (isLeft)
                    cur.leftKey = bin;
                else
                    cur.rightKey = bin;
                break;
            }
            if (isLeft && cur.leftKey == bin || !isLeft && cur.rightKey == bin) {
                NodeSparse newLeaf(bin, value);
                newLeaf.write(io);
                hashUp = newLeaf.computeHash();
                break;
            }
            if (isLeft && !cur.isLeftPrefix(bin)) {
                NodeSparse newLeaf(bin, value);
                newLeaf.write(io);
                hashUp = newLeaf.computeHash();
                std::string newKey = common_prefix(bin, cur.leftKey);
                bool l = is_prefix(bin, newKey + "0");
                NodeSparse newNode(newKey, l ? bin : cur.leftKey, l ? hashUp : cur.leftHash,
                                   l ? cur.leftKey : bin, l ? cur.leftHash : hashUp);
                newNode.write(io);
                cur.leftKey = newKey;
                hashUp = newNode.computeHash();
                break;
            }
            if (!isLeft && !cur.isRightPrefix(bin)) {
                NodeSparse newLeaf(bin, value);
                newLeaf.write(io);
                hashUp = newLeaf.computeHash();
                std::string newKey = common_prefix(bin, cur.rightKey);
                bool l = is_prefix(bin, newKey + "0");
                NodeSparse newNode(newKey, l ? bin : cur.rightKey, l ? hashUp : cur.rightHash,
                                   l ? cur.rightKey : bin, l ? cur.rightHash : hashUp);
                newNode.write(io);
                cur.rightKey = newKey;
                hashUp = newNode.computeHash();
                break;
            }
            stack.emplace_back(isLeft ? cur.leftKey : cur.rightKey, io);
        }
        for (Int i = stack.size() - 1; i >= 0; --i) {
            NodeSparse &cur = stack[i];
            if (lefts[i]) {
                cur.leftHash = hashUp;
            } else {
                cur.rightHash = hashUp;
            }
            hashUp = cur.computeHash();
            cur.write(io);
        }
        this->digest = hashUp;
    }

    std::string gen_proof(const std::string &spos) override {
        std::string bin;

        io->read("?" + spos, bin);
        if (bin.empty())
            return "?";
        std::string output;
        std::string key = "*";

        for ( ; ; ) {
            NodeSparse cur(key, io);
            bool isLeft = cur.isLeft(bin);
            //if (cur.isRoot && (isLeft && cur.rightKey.empty() || !isLeft && cur.leftKey.empty())) {
            //} else
            {
                output.append(isLeft ? "0" : "1");
                std::string sibling(isLeft ? cur.rightHash : cur.leftHash);
                if (sibling.empty()) {
                    sibling = null64;
                }
                output.append(sibling);
            }
            if (isLeft && cur.leftKey == bin || !isLeft && cur.rightKey == bin) {
                break;
            }
            key = isLeft ? cur.leftKey : cur.rightKey;
        }
        return output;
    }

    bool verify_proof(const std::string &spos, const std::string &value, const std::string &proof) override {
        if (proof[0] == '?')
            return true;
        std::string s, key = calculateSHA256Hash(value);
        for (Int i = proof.length() - 65; i >= 0; i -= 65) {
            s = proof.substr(i + 1, 64);
            if (s[0] == '@')
                s = "";
            key = calculateSHA256Hash(proof[i] == '0' ? key.append(s) : s.append(key));
        }
        return key == digest;
    }

    std::string get_name() override {
        return "sparse_balance";
    }

    ~SparseBalance() override {
        io->flush();
    }
};

class NodeMint {
public:
    std::string key, leftHash, rightHash, value;
    bool isRoot, isLeaf;
    explicit NodeMint(const std::string &key, const std::string &value) {
        isLeaf = true;
        isRoot = false;
        this->key = key;
        this->value = value;
    }
    explicit NodeMint(const std::string &key, const std::string &lh, const std::string &rh) {
        isLeaf = false;
        isRoot = key == "@";
        this->key = key;
        this->leftHash = lh;
        this->rightHash = rh;
    }
    explicit NodeMint(const std::string &key, IO *io) {
        std::string v;
        io->read(key, v);
        isRoot = key == "@";
        this->key = key;
        if (v[0] == '!') {
            isLeaf = true;
            this->value = v.substr(1);
        } else {
            isLeaf = false;
            leftHash = v.substr(0, 64);
            if (v.length() > 64) {
                rightHash = v.substr(64, 64);
            }
        }
    }
    std::string to_string() {
        return isLeaf ? "!" + value : leftHash + rightHash;
    }
    std::string computeHash() {
        return isLeaf ? calculateSHA256Hash(value) : calculateSHA256Hash(leftHash + rightHash);
    }
    void write(IO *io) {
        io->write(key, to_string());
    }
};

class SparseMint : public MemChecker {

protected:
    std::string digest;
    Int num_leaf, height;



public:
    explicit SparseMint(Int height) {
        this->height = height;
        this->num_leaf = 0;
        digest = "";
    }

    bool init(IO *io, bool create_db, std::string *values) override {
        this->io = io;
        if (!this->io->open()) {
            return false;
        }
        if (create_db) {
            this->io->write("@", "");
            this->io->flush();
            this->digest = calculateSHA256Hash("");
        } else {
            //not available
        }
        return true;
    }

    void update(const std::string &spos, const std::string &value) override {
        std::string bin;
        io->read("?" + spos, bin);
        bool isNew = false;
        if (bin.empty()) {
            bin = int_to_binary(this->num_leaf, this->height);
            io->write("?" + spos, bin);
            this->num_leaf++;
            isNew = true;
        }

        NodeMint root("@", io);
        std::vector<NodeMint> stack;
        stack.push_back(root);

        Int p = 0;
        Int val = 0, la = 0;

        std::string hashUp;

        std::string sval, cval;
        bool contd = false;
        for ( ; ; ++p) {
            NodeMint &cur = stack[stack.size() - 1];
            bool isLeft = bin[p] == '0';
            val = val + (isLeft ? 0 : (1ll << p));
            cval.append(isLeft ? "0" : "1");
            if (isNew && num_leaf <= 2 || !isNew && val + (1 << (p + 1)) >= num_leaf) {
                NodeMint newLeaf(bin, value);
                newLeaf.write(io);
                hashUp = newLeaf.computeHash();
                break;
            }

            std::string newKey;
            if (!contd) {
                contd = true;
                la = isLeft ? -1 : 1;
            } else if (bin[p] != bin[p - 1]) {
                contd = false;
                sval = cval;
                la = 0;
            } else {
                la += isLeft ? -1 : 1;
            }
            newKey = sval + (char)('@' + la);
            if (isNew && val + (1 << (p + 1)) == num_leaf - 1) {
                NodeMint newLeaf(bin, value);
                newLeaf.write(io);
                hashUp = newLeaf.computeHash();

                NodeMint newNode(newKey, isLeft ? cur.leftHash : cur.rightHash, hashUp);
                newNode.write(io);
                hashUp = newNode.computeHash();
                break;
            }
            stack.emplace_back(newKey, io);
        }
        for (Int i = stack.size() - 1; i >= 0; --i) {
            NodeMint &cur = stack[i];
            if (bin[i] == '0') {
                cur.leftHash = hashUp;
            } else {
                cur.rightHash = hashUp;
            }
            hashUp = cur.computeHash();
            cur.write(io);
        }
        this->digest = hashUp;
    }

    std::string gen_proof(const std::string &spos) override {
        std::string bin;

        io->read("?" + spos, bin);
        if (bin.empty())
            return "?";
        std::string output;
        std::string key = "@";

        Int p = 0;
        Int val = 0, la = 0;
        std::string sval, cval;
        bool contd = false;

        for ( ; ; ++p) {
            NodeMint cur(key, io);
            bool isLeft = bin[p] == '0';
            val = val + (isLeft ? 0 : (1 << p));
            cval.append(isLeft ? "0" : "1");
            //if (cur.isRoot && (isLeft && cur.rightKey.empty() || !isLeft && cur.leftKey.empty())) {
            //} else
            {
                output.append(isLeft ? "0" : "1");
                std::string sibling(isLeft ? cur.rightHash : cur.leftHash);
                if (sibling.empty()) {
                    sibling = null64;
                }
                output.append(sibling);
            }
            if (val + (1 << (p + 1)) >= num_leaf) {
                break;
            }
            if (!contd) {
                contd = true;
                la = isLeft ? -1 : 1;
            } else if (bin[p] != bin[p - 1]) {
                contd = false;
                sval = cval;
                la = 0;
            } else {
                la += isLeft ? -1 : 1;
            }
            key = sval + (char)('@' + la);
        }
        return output;
    }

    bool verify_proof(const std::string &spos, const std::string &value, const std::string &proof) override {
        if (proof[0] == '?')
            return true;
        std::string s, key = calculateSHA256Hash(value);
        for (Int i = proof.length() - 65; i >= 0; i -= 65) {
            s = proof.substr(i + 1, 64);
            if (s[0] == '@')
                s = "";
            key = calculateSHA256Hash(proof[i] == '0' ? key.append(s) : s.append(key));
        }
        return key == digest;
    }

    std::string get_name() override {
        return "sparse_mint";
    }

    ~SparseMint() override {
        io->flush();
    }
};

class NodeMint2 {
public:
    std::string key, leftHash, rightHash, value;
    bool isRoot, isLeaf;
    explicit NodeMint2(const std::string &key, const std::string &value) {
        isLeaf = true;
        isRoot = false;
        this->key = key;
        this->value = value;
    }
    explicit NodeMint2(const std::string &key, const std::string &lh, const std::string &rh) {
        isLeaf = false;
        isRoot = false;
        this->key = key;
        this->leftHash = lh;
        this->rightHash = rh;
    }
    explicit NodeMint2(const std::string &key, IO *io) {
        std::string v;
        io->read(key, v);
        isRoot = key == "*";
        this->key = key;
        if (v[0] == '!') {
            isLeaf = true;
            this->value = v.substr(1);
        } else {
            isLeaf = false;
            leftHash = v.substr(0, 64);
            if (v.length() > 64) {
                rightHash = v.substr(64, 64);
            }
        }
    }
    std::string to_string() {
        return isLeaf ? "!" + value : leftHash + rightHash;
    }
    std::string computeHash() {
        return isLeaf ? calculateSHA256Hash(value) : calculateSHA256Hash(leftHash + rightHash);
    }
    void write(IO *io) {
        io->write(key, to_string());
    }
};

class SparseMint2 : public MemChecker {

protected:
    std::string digest;
    Int num_leaf, height;



public:
    explicit SparseMint2(Int height) {
        this->height = height;
        this->num_leaf = 0;
        digest = "";
    }

    bool init(IO *io, bool create_db, std::string *values) override {
        this->io = io;
        if (!this->io->open()) {
            return false;
        }
        if (create_db) {
            this->io->write("*", "");
            this->io->flush();
            this->digest = calculateSHA256Hash("");
        } else {
            //not available
        }
        return true;
    }

    void update(const std::string &spos, const std::string &value) override {
        std::string bin;
        io->read("?" + spos, bin);
        bool isNew = false;
        if (bin.empty()) {
            bin = int_to_binary(this->num_leaf, this->height);
            io->write("?" + spos, bin);
            this->num_leaf++;
            isNew = true;
        }

        NodeMint2 root("*", io);
        std::vector<NodeMint2> stack;
        stack.push_back(root);

        Int p = 0;
        Int val = 0;

        std::string hashUp;
        for ( ; ; ++p) {
            NodeMint2 &cur = stack[stack.size() - 1];
            bool isLeft = bin[p] == '0';
            val = val + (isLeft ? 0 : (1ll << p));
            if (isNew && num_leaf <= 2 || !isNew && val + (1 << (p + 1)) >= num_leaf) {
                NodeMint2 newLeaf(bin, value);
                newLeaf.write(io);
                hashUp = newLeaf.computeHash();
                break;
            }
            std::string newKey = bin.substr(0, p + 1);
            if (isNew && val + (1 << (p + 1)) == num_leaf - 1) {
                NodeMint2 newLeaf(bin, value);
                newLeaf.write(io);
                hashUp = newLeaf.computeHash();

                NodeMint2 newNode(newKey, isLeft ? cur.leftHash : cur.rightHash, hashUp);
                newNode.write(io);
                hashUp = newNode.computeHash();
                break;
            }
            stack.emplace_back(newKey, io);
        }
        for (Int i = stack.size() - 1; i >= 0; --i) {
            NodeMint2 &cur = stack[i];
            if (bin[i] == '0') {
                cur.leftHash = hashUp;
            } else {
                cur.rightHash = hashUp;
            }
            hashUp = cur.computeHash();
            cur.write(io);
        }
        this->digest = hashUp;
    }

    std::string gen_proof(const std::string &spos) override {
        std::string bin;

        io->read("?" + spos, bin);
        if (bin.empty())
            return "?";
        std::string output;
        std::string key = "*";

        Int p = 0;
        Int val = 0;

        for ( ; ; ++p) {
            NodeMint2 cur(key, io);
            bool isLeft = bin[p] == '0';
            val = val + (isLeft ? 0 : (1 << p));
            //if (cur.isRoot && (isLeft && cur.rightKey.empty() || !isLeft && cur.leftKey.empty())) {
            //} else
            {
                output.append(isLeft ? "0" : "1");
                std::string sibling(isLeft ? cur.rightHash : cur.leftHash);
                if (sibling.empty()) {
                    sibling = null64;
                }
                output.append(sibling);
            }
            if (val + (1 << (p + 1)) >= num_leaf) {
                break;
            }
            key = bin.substr(0, p + 1);
        }
        return output;
    }

    bool verify_proof(const std::string &spos, const std::string &value, const std::string &proof) override {
        if (proof[0] == '?')
            return true;
        std::string s, key = calculateSHA256Hash(value);
        for (Int i = proof.length() - 65; i >= 0; i -= 65) {
            s = proof.substr(i + 1, 64);
            if (s[0] == '@')
                s = "";
            key = calculateSHA256Hash(proof[i] == '0' ? key.append(s) : s.append(key));
        }
        return key == digest;
    }

    std::string get_name() override {
        return "sparse_mint2 Original";
    }

    ~SparseMint2() override {
        io->flush();
    }
};

#endif //DUPTREE_SPARSE_HPP
