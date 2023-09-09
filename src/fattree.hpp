#ifndef DUPTREE_FATTREE_HPP
#define DUPTREE_FATTREE_HPP

#include "mem_checker.hpp"

class NodeFat {
public:
    std::string key, value;
    std::vector<std::string> keys, hashes;
    bool isRoot, isLeaf;
    explicit NodeFat(const std::string &key, const std::string &value) {
        isLeaf = true;
        isRoot = false;
        this->key = key;
        this->value = value;
    }
    explicit NodeFat(const std::string &key, const std::vector<std::string> &ks, const std::vector<std::string> &hs): keys(ks), hashes(hs) {
        isLeaf = false;
        isRoot = false;
        this->key = key;
    }
    explicit NodeFat(const std::string &key, IO *io) {
        std::string v;
        io->read(key, v);
        isRoot = key == "*";
        this->key = key;
        if (v[0] == '!') {
            isLeaf = true;
            this->value = v.substr(1);
        } else {
            isLeaf = false;
            Int pos = 0, pred = 0;
            for (int i = 0; i < 16; ++i) {
                while (v[pos] != ':')
                    ++pos;
                keys.emplace_back(v.substr(pred, pos - pred));
                pred = pos = pos + 1;
                while (pos < v.length() && v[pos] != ',')
                    ++pos;
                hashes.emplace_back(v.substr(pred, pos - pred));
                pred = pos = pos + 1;
            }
        }
    }
    std::string to_string() {
        if (isLeaf)
            return "!" + value;
        std::string output;
        for (int i = 0; i < 16; ++i) {
            output.append(keys[i] + ":" + hashes[i]);
            if (i < 15)
                output.append(",");
        }
        return output;
    }
    std::string computeHash() {
        if (isLeaf)
            return calculateSHA256Hash(value);
        std::string output;
        for (int i = 0; i < 16; ++i) {
            output.append(hashes[i]);
        }
        return calculateSHA256Hash(output);
    }
    void write(IO *io) {
        io->write(key, to_string());
    }
    int ofWhich(const std::string &k) {
        return hti[k[isRoot ? 0 : key.length()]];
    }
    bool isPrefixChild(int child, const std::string &k) {
        return is_prefix(k, keys[child]);
    }
};

class FatTree : public MemChecker {

protected:
    std::string digest;
    Int height;



public:
    explicit FatTree(Int height) {
        this->height = height;
        digest = "";
    }

    bool init(IO *io, bool create_db, std::string *values) override {
        this->io = io;
        if (!this->io->open()) {
            return false;
        }
        if (create_db) {
            this->io->write("*", ":,:,:,:,:,:,:,:,:,:,:,:,:,:,:,:");
            this->io->flush();
            this->digest = calculateSHA256Hash("");
        } else {
            //not available
        }
        return true;
    }

    void update(const std::string &spos, const std::string &value) override {
        std::string hex(spos);
        NodeFat root("*", io);
        std::vector<NodeFat> stack;
        std::vector<int> lefts;
        stack.push_back(root);

        int cnt = 0;
        std::string hashUp;
        for ( ; ; ) {

            NodeFat &cur = stack[stack.size() - 1];
            int which = cur.ofWhich(hex);
            lefts.push_back(which);
            if (++cnt > 42) {
                throw std::invalid_argument("");
            }
            if (cur.keys[which].empty()) {
                NodeFat newLeaf(hex, value);
                newLeaf.write(io);
                hashUp = newLeaf.computeHash();
                cur.keys[which] = hex;
                break;
            }
            if (cur.keys[which] == hex) {
                NodeFat newLeaf(hex, value);
                newLeaf.write(io);
                hashUp = newLeaf.computeHash();
                break;
            }
            if (!cur.isPrefixChild(which, hex)) {
                NodeFat newLeaf(hex, value);
                newLeaf.write(io);
                hashUp = newLeaf.computeHash();

                std::string newKey = common_prefix(hex, cur.keys[which]);
                std::vector<std::string> newKeys, newHashes;
                for (int i = 0; i < 16; ++i) {
                    newKeys.emplace_back("");
                    newHashes.emplace_back("");
                }
                NodeFat newNode(newKey, newKeys, newHashes);
                int w1 = newNode.ofWhich(hex);
                newNode.keys[w1] = hex;
                newNode.hashes[w1] = hashUp;
                int w2 = newNode.ofWhich(cur.keys[which]);
                newNode.keys[w2] = cur.keys[which];
                newNode.hashes[w2] = cur.hashes[which];
                newNode.write(io);
                cur.keys[which] = newKey;
                hashUp = newNode.computeHash();
                break;
            }
            stack.emplace_back(cur.keys[which], io);
        }
        for (Int i = stack.size() - 1; i >= 0; --i) {
            NodeFat &cur = stack[i];
            cur.hashes[lefts[i]] = hashUp;
            hashUp = cur.computeHash();
            cur.write(io);
        }
        this->digest = hashUp;
    }

    std::string gen_proof(const std::string &spos) override {
        std::string hex(spos), tmp;
        io->read(hex, tmp);
        if (tmp.empty())
            return "?";

        std::string output;
        std::string key = "*";

        int cnt = 0;
        for ( ; ; ) {
            if (++cnt > 41)
                throw std::invalid_argument("");
            NodeFat cur(key, io);
            int which = cur.ofWhich(hex);
            //if (cur.isRoot && (isLeft && cur.rightKey.empty() || !isLeft && cur.leftKey.empty())) {
            //} else
            {
                output.append(std::string(1, '0' + which));
                for (int i = 0; i < 16; ++i) {
                    if (i != which) {
                        output.append(cur.hashes[i].empty() ? null64 : cur.hashes[i]);
                    }
                }
            }
            if (cur.keys[which] == hex) {
                break;
            }
            key = cur.keys[which];
        }
        return output;
    }

    bool verify_proof(const std::string &spos, const std::string &value, const std::string &proof) override {
        if (proof[0] == '?')
            return true;
        std::string s, key = calculateSHA256Hash(value);
        for (Int i = proof.length() - 1 - 64 * 15; i >= 0; i -= 1 + 64 * 15) {
            int which = proof[i] - '0';
            Int pos = i + 1;
            s = "";
            for (int j = 0; j < 16; ++j) {
                if (j == which) {
                    s.append(key);
                } else {
                    if (proof[pos] != null64[0]) {
                        s.append(proof.substr(pos, 64));
                    }
                    pos += 64;
                }
            }
            key = calculateSHA256Hash(s);
        }
        return key == digest;
    }

    std::string get_name() override {
        return "fat_tree";
    }

    ~FatTree() override {
        io->flush();
    }
};

/*class NodeFatter {
public:
    std::string key, value;
    std::vector<std::vector<std::string>> keys, hashes;
    bool isRoot, isLeaf;
    explicit NodeFatter(const std::string &key, const std::string &value) {
        isLeaf = true;
        isRoot = false;
        this->key = key;
        this->value = value;
    }
    explicit NodeFatter(const std::string &key, const std::vector<std::string> &ks, const std::vector<std::string> &hs): keys(ks), hashes(hs) {
        isLeaf = false;
        isRoot = false;
        this->key = key;
    }
    explicit NodeFatter(const std::string &key, IO *io) {
        std::string v;
        io->read(key, v);
        isRoot = key == "*";
        this->key = key;
        if (v[0] == '!') {
            isLeaf = true;
            this->value = v.substr(1);
        } else {
            isLeaf = false;
            Int pos = 0, pred = 0;
            for (int i = 0; i < 16; ++i) {
                while (v[pos] != ':')
                    ++pos;
                keys.emplace_back(v.substr(pred, pos - pred));
                pred = pos = pos + 1;
                while (pos < v.length() && v[pos] != ',')
                    ++pos;
                hashes.emplace_back(v.substr(pred, pos - pred));
                pred = pos = pos + 1;
            }
        }
    }
    std::string to_string() {
        if (isLeaf)
            return "!" + value;
        std::string output;
        for (int i = 0; i < 16; ++i) {
            output.append(keys[i] + ":" + hashes[i]);
            if (i < 15)
                output.append(",");
        }
        return output;
    }
    std::string computeHash() {
        if (isLeaf)
            return calculateSHA256Hash(value);
        std::string output;
        for (int i = 0; i < 16; ++i) {
            output.append(hashes[i]);
        }
        return calculateSHA256Hash(output);
    }
    void write(IO *io) {
        io->write(key, to_string());
    }
    int ofWhich(const std::string &k) {
        return hti[k[isRoot ? 0 : key.length()]];
    }
    bool isPrefixChild(int child, const std::string &k) {
        return is_prefix(k, keys[child]);
    }
};

class FatterTree : public MemChecker {

protected:
    std::string digest;
    Int height;



public:
    explicit FatterTree(Int height) {
        this->height = height;
        digest = "";
    }

    bool init(IO *io, bool create_db, std::string *values) override {
        this->io = io;
        if (!this->io->open()) {
            return false;
        }
        if (create_db) {
            this->io->write("*", ":,:,:,:,:,:,:,:,:,:,:,:,:,:,:,:");
            this->io->flush();
            this->digest = calculateSHA256Hash("");
        } else {
            //not available
        }
        return true;
    }

    void update(const std::string &spos, const std::string &value) override {
        std::string hex(spos);
        NodeFatter root("*", io);
        std::vector<NodeFatter> stack;
        std::vector<int> lefts;
        stack.push_back(root);

        int cnt = 0;
        std::string hashUp;
        for ( ; ; ) {

            NodeFatter &cur = stack[stack.size() - 1];
            int which = cur.ofWhich(hex);
            lefts.push_back(which);
            if (++cnt > 42) {
                throw std::invalid_argument("");
            }
            if (cur.keys[which].empty()) {
                NodeFat newLeaf(hex, value);
                newLeaf.write(io);
                hashUp = newLeaf.computeHash();
                cur.keys[which] = hex;
                break;
            }
            if (cur.keys[which] == hex) {
                NodeFat newLeaf(hex, value);
                newLeaf.write(io);
                hashUp = newLeaf.computeHash();
                break;
            }
            if (!cur.isPrefixChild(which, hex)) {
                NodeFat newLeaf(hex, value);
                newLeaf.write(io);
                hashUp = newLeaf.computeHash();

                std::string newKey = common_prefix(hex, cur.keys[which]);
                std::vector<std::string> newKeys, newHashes;
                for (int i = 0; i < 16; ++i) {
                    newKeys.emplace_back("");
                    newHashes.emplace_back("");
                }
                NodeFat newNode(newKey, newKeys, newHashes);
                int w1 = newNode.ofWhich(hex);
                newNode.keys[w1] = hex;
                newNode.hashes[w1] = hashUp;
                int w2 = newNode.ofWhich(cur.keys[which]);
                newNode.keys[w2] = cur.keys[which];
                newNode.hashes[w2] = cur.hashes[which];
                newNode.write(io);
                cur.keys[which] = newKey;
                hashUp = newNode.computeHash();
                break;
            }
            stack.emplace_back(cur.keys[which], io);
        }
        for (Int i = stack.size() - 1; i >= 0; --i) {
            NodeFat &cur = stack[i];
            cur.hashes[lefts[i]] = hashUp;
            hashUp = cur.computeHash();
            cur.write(io);
        }
        this->digest = hashUp;
    }

    std::string gen_proof(const std::string &spos) override {
        std::string hex(spos), tmp;
        io->read(hex, tmp);
        if (tmp.empty())
            return "?";

        std::string output;
        std::string key = "*";

        int cnt = 0;
        for ( ; ; ) {
            if (++cnt > 41)
                throw std::invalid_argument("");
            NodeFat cur(key, io);
            int which = cur.ofWhich(hex);
            //if (cur.isRoot && (isLeft && cur.rightKey.empty() || !isLeft && cur.leftKey.empty())) {
            //} else
            {
                output.append(std::string(1, '0' + which));
                for (int i = 0; i < 16; ++i) {
                    if (i != which) {
                        output.append(cur.hashes[i].empty() ? null64 : cur.hashes[i]);
                    }
                }
            }
            if (cur.keys[which] == hex) {
                break;
            }
            key = cur.keys[which];
        }
        return output;
    }

    bool verify_proof(const std::string &spos, const std::string &value, const std::string &proof) override {
        if (proof[0] == '?')
            return true;
        std::string s, key = calculateSHA256Hash(value);
        for (Int i = proof.length() - 1 - 64 * 15; i >= 0; i -= 1 + 64 * 15) {
            int which = proof[i] - '0';
            Int pos = i + 1;
            s = "";
            for (int j = 0; j < 16; ++j) {
                if (j == which) {
                    s.append(key);
                } else {
                    if (proof[pos] != null64[0]) {
                        s.append(proof.substr(pos, 64));
                    }
                    pos += 64;
                }
            }
            key = calculateSHA256Hash(s);
        }
        return key == digest;
    }

    std::string get_name() override {
        return "fatter_tree";
    }

    ~FatterTree() override {
        io->flush();
    }
};*/


class NodeFatMint {
public:
    std::string key, value;
    std::vector<std::string> hashes;
    bool isRoot, isLeaf;
    explicit NodeFatMint(const std::string &key, const std::string &value) {
        isLeaf = true;
        isRoot = false;
        this->key = key;
        this->value = value;
    }
    explicit NodeFatMint(const std::string &key, const std::vector<std::string> &hs) : hashes(hs) {
        isLeaf = false;
        isRoot = false;
        this->key = key;
    }
    explicit NodeFatMint(const std::string &key, IO *io) {
        std::string v;
        io->read(key, v);
        isRoot = key == "*";
        this->key = key;
        if (v[0] == '!') {
            isLeaf = true;
            this->value = v.substr(1);
        } else {
            isLeaf = false;
            for (Int i = 0; i < v.length(); i += 64) {
                hashes.emplace_back(v.substr(i, 64));
            }
            while (hashes.size() < 16) {
                hashes.emplace_back("");
            }
        }
    }
    std::string to_string() {
        if (isLeaf)
            return "!" + value;
        std::string output;
        for (int i = 0; i < 16; ++i) {
            output.append(hashes[i]);
        }
        return output;
    }
    std::string computeHash() {
        if (isLeaf)
            return calculateSHA256Hash(value);
        std::string output;
        for (int i = 0; i < 16; ++i) {
            output.append(hashes[i]);
        }
        return calculateSHA256Hash(output);
    }
    void write(IO *io) {
        io->write(key, to_string());
    }
};

class FatMint : public MemChecker {

protected:
    std::string digest;
    Int num_leaf, height;



public:
    explicit FatMint(Int height) {
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
        std::string hex;
        io->read("?" + spos, hex);
        bool isNew = false;
        if (hex.empty()) {
            hex = int_to_hex(this->num_leaf, this->height);
            io->write("?" + spos, hex);
            this->num_leaf++;
            isNew = true;
        }

        NodeFatMint root("*", io);
        std::vector<NodeFatMint> stack;
        stack.push_back(root);

        Int p = 0;
        Int val = 0;

        std::string hashUp;
        for ( ; ; ++p) {
            NodeFatMint &cur = stack[stack.size() - 1];
            int which = hti[hex[p]];
            val = val + ((1ll << (4 * p)) * which);
            if (isNew && val + ((1ll << (4 * p)) * (15 - which)) >= num_leaf - 1 || !isNew && val + (1 << (4 * (p + 1))) >= num_leaf) {
                NodeFatMint newLeaf(hex, value);
                newLeaf.write(io);
                hashUp = newLeaf.computeHash();
                break;
            }
            std::string newKey = hex.substr(0, p + 1);
            if (isNew && val + (1 << (4 * (p + 1))) == num_leaf - 1) {
                NodeFatMint newLeaf(hex, value);
                newLeaf.write(io);
                hashUp = newLeaf.computeHash();

                std::vector<std::string> newKeys, newHashes;
                newHashes.emplace_back(cur.hashes[which]);
                newHashes.emplace_back(hashUp);
                for (int i = 2; i < 16; ++i) {
                    newHashes.emplace_back("");
                }
                NodeFatMint newNode(newKey, newHashes);

                newNode.write(io);
                hashUp = newNode.computeHash();
                break;
            }
            stack.emplace_back(newKey, io);
        }
        for (Int i = stack.size() - 1; i >= 0; --i) {
            NodeFatMint &cur = stack[i];
            cur.hashes[hti[hex[i]]] = hashUp;
            hashUp = cur.computeHash();
            cur.write(io);
        }
        this->digest = hashUp;
    }

    std::string gen_proof(const std::string &spos) override {
        std::string hex;

        io->read("?" + spos, hex);
        if (hex.empty())
            return "?";
        std::string output;
        std::string key = "*";

        Int p = 0;
        Int val = 0;

        for ( ; ; ++p) {
            NodeFatMint cur(key, io);
            int which = hti[hex[p]];
            val = val + ((1ll << (4 * p)) * which);
            //if (cur.isRoot && (isLeft && cur.rightKey.empty() || !isLeft && cur.leftKey.empty())) {
            //} else
            {
                output.append(std::string(1, '0' + which));
                for (int i = 0; i < 16; ++i) {
                    if (i != which) {
                        output.append(cur.hashes[i].empty() ? null64 : cur.hashes[i]);
                    }
                }
            }
            if (val + ((1ll << (4 * (p + 1)))) >= num_leaf) {
                break;
            }
            key = hex.substr(0, p + 1);
        }
        return output;
    }

    bool verify_proof(const std::string &spos, const std::string &value, const std::string &proof) override {
        if (proof[0] == '?')
            return true;
        std::string s, key = calculateSHA256Hash(value);
        for (Int i = proof.length() - 1 - 64 * 15; i >= 0; i -= 1 + 64 * 15) {
            int which = proof[i] - '0';
            Int pos = i + 1;
            s = "";
            for (int j = 0; j < 16; ++j) {
                if (j == which) {
                    s.append(key);
                } else {
                    if (proof[pos] != null64[0]) {
                        s.append(proof.substr(pos, 64));
                    }
                    pos += 64;
                }
            }
            key = calculateSHA256Hash(s);
        }
        return key == digest;
    }

    std::string get_name() override {
        return "fat_mint";
    }

    ~FatMint() override {
        io->flush();
    }
};

#endif //DUPTREE_FATTREE_HPP
