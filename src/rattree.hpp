#ifndef DUPTREE_RATTREE_HPP
#define DUPTREE_RATTREE_HPP

#include "mem_checker.hpp"

class NodeRat {
public:
    std::string hash, value;
    std::vector<std::string> hashes;
    std::vector<Int> pointers; // used only when hashes[i] == '$'
    std::string prefix;
    bool isRoot, isLeaf;
    explicit NodeRat(const std::string &key, const std::string &p, const std::string &value) : pointers(16, -1) {
        isLeaf = true;
        isRoot = false;
        this->hash = key;
        this->prefix = p;
        this->value = value;
    }
    explicit NodeRat(const std::string &key, const std::string &p, const std::vector<std::string> &hs) : hashes(hs), pointers(16, -1) {
        isLeaf = false;
        isRoot = false;
        this->hash = key;
        this->prefix = p;
    }
    explicit NodeRat(const std::string &key, IO *io, Int &num) : pointers(16, -1) {
        std::string v;
        io->read(key, v);
        num += v.length();
        this->hash = key;
        auto p = v.find('|');
        prefix = v.substr(0, p);
        v = v.substr(p + 1);
        isRoot = prefix.empty();
        if (v[0] == '!') {
            isLeaf = true;
            this->value = v.substr(1);
        } else {
            isLeaf = false;

            Int pos = 0, pred = 0;
            for (int i = 0; i < 16; ++i) {
                while (pos < v.length() && v[pos] != ',')
                    ++pos;
                hashes.emplace_back(v.substr(pred, pos - pred));
                pred = pos = pos + 1;
            }

        }
    }
    std::string to_string() {
        if (isLeaf)
            return prefix + "|!" + value;
        std::string output(prefix);
        output.append("|");
        for (int i = 0; i < 16; ++i) {
            output.append(hashes[i]);
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
    void write(IO *io, Int &num) {
        std::string tmp(to_string());
        num += tmp.length();
        io->write(hash, tmp);
    }
    int ofWhich(const std::string &k) {
        return hti[k[prefix.length()]];
    }
};

class RatTree : public MemChecker {
    Int num_read, num_write;
    void _compute(std::vector<NodeRat> &stack, Int pos) {
        NodeRat &cur = stack[pos];
        if (!cur.isLeaf) {
            for (int i = 0; i < 16; ++i) {
                if (cur.pointers[i] != -1) {
                    _compute(stack, cur.pointers[i]);
                    cur.hashes[i] = stack[cur.pointers[i]].hash;
                    cur.pointers[i] = -1;
                }
            }
        }
        cur.hash = cur.computeHash();
        cur.write(io, num_write);
    }

protected:
    std::string digest;
    Int height;
    std::vector<std::pair<std::string, std::string>> list;


public:
    explicit RatTree(Int height) {
        this->height = height;
        digest = "";
    }

    bool init(IO *io, bool create_db, std::string *values) override {
        this->io = io;
        if (!this->io->open()) {
            return false;
        }
        if (create_db) {
            this->digest = calculateSHA256Hash("");
            this->io->write(this->digest, "|,,,,,,,,,,,,,,,");
            this->io->flush();
        } else {
            //not available
        }
        return true;
    }

    void update(const std::string &spos, const std::string &value) override {
        list.emplace_back(spos, value);
    }

    std::pair<Int, Int> commit() override {
        num_read = num_write = 0;
        std::vector<NodeRat> stack;
        stack.emplace_back(this->digest, io, num_read);
        for (const auto& pair : list) {
            std::string hex(pair.first), value(pair.second);

            int cnt = 0;
            Int pos = 0;
            for ( ; ; ) {
                if (cnt++ > 40) {
                    throw std::invalid_argument("invalid loop");
                }
                NodeRat &cur = stack[pos];
                int which = cur.ofWhich(hex);

                if (cur.pointers[which] != -1) {
                    NodeRat &next = stack[cur.pointers[which]];
                    if (next.prefix == hex) {
                        next.value = value;
                        break;
                    }
                    if (!is_prefix(hex, next.prefix)) {
                        std::string np(next.prefix);
                        stack.emplace_back("", hex, value);

                        std::string newPrefix = common_prefix(hex, np);
                        std::vector<std::string> newHashes;
                        for (int i = 0; i < 16; ++i) {
                            newHashes.emplace_back("");
                        }
                        stack.emplace_back("", newPrefix, newHashes);
                        NodeRat &newNode = stack[stack.size() - 1];
                        int w1 = newNode.ofWhich(hex);
                        newNode.pointers[w1] = stack.size() - 2;
                        int w2 = newNode.ofWhich(np);
                        newNode.pointers[w2] = stack[pos].pointers[which];
                        stack[pos].pointers[which] = stack.size() - 1;
                        break;
                    }
                    pos = cur.pointers[which];
                } else {
                    if (cur.hashes[which].empty()) {
                        //new leaf
                        cur.pointers[which] = stack.size();
                        stack.emplace_back("", hex, value);
                        break;
                    }
                    NodeRat next(cur.hashes[which], io, num_read);
                    if (next.prefix == hex) {
                        next.value = value;
                        cur.pointers[which] = stack.size();
                        stack.push_back(next);
                        break;
                    }
                    if (!is_prefix(hex, next.prefix)) {
                        stack.emplace_back("", hex, value);

                        std::string newPrefix = common_prefix(hex, next.prefix);
                        std::vector<std::string> newHashes;
                        for (int i = 0; i < 16; ++i) {
                            newHashes.emplace_back("");
                        }
                        stack.emplace_back("", newPrefix, newHashes);
                        NodeRat &newNode = stack[stack.size() - 1];
                        int w1 = newNode.ofWhich(hex);
                        newNode.pointers[w1] = stack.size() - 2;
                        int w2 = newNode.ofWhich(next.prefix);
                        newNode.hashes[w2] = next.hash;

                        stack[pos].pointers[which] = stack.size() - 1;
                        break;
                    }
                    pos = cur.pointers[which] = stack.size();
                    stack.push_back(next);
                }
            }
        }
        _compute(stack, 0);
        list.clear();
        this->digest = stack[0].hash;
        return std::make_pair(num_read, num_write);
    }

    std::string gen_proof(const std::string &spos) override {
        std::string hex(spos), tmp;
        //io->read(hex, tmp);
        //if (tmp.empty())
        //    return "?";

        std::string output;
        std::string key = this->digest;

        int cnt = 0;
        for ( ; ; ) {
            NodeRat cur(key, io, num_read);
            if (cur.prefix == hex)
                break;
            if (!is_prefix(hex, cur.prefix)) {
                return "?";
            }
            int which = cur.ofWhich(hex);
            if (cur.hashes[which].empty()) {
                return "?";
            }
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
            key = cur.hashes[which];
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
        return "rat_tree";
    }

    ~RatTree() override {
        io->flush();
    }
};

class NodeRatPrefix {
public:
    std::string key, value;
    std::vector<std::string> keys, hashes;
    std::vector<Int> pointers;
    bool isRoot, isLeaf;
    int keyLen;
    explicit NodeRatPrefix(const std::string &key, const std::string &value) : pointers(16, -1) {
        isLeaf = true;
        isRoot = false;
        this->key = key;
        this->value = value;
        this->keyLen = 64;
    }
    explicit NodeRatPrefix(const std::string &key, const std::vector<std::string> &ks, const std::vector<std::string> &hs): pointers(16, -1), keys(ks), hashes(hs) {
        isLeaf = false;
        isRoot = false;
        this->key = key;
        this->keyLen = key.find('-');
    }
    explicit NodeRatPrefix(const std::string &key, IO *io) : pointers(16, -1) {
        std::string v;
        io->read(key, v);
        this->keyLen = key.find('-');
        isRoot = key[0] == '*';
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
        return hti[k[isRoot ? 0 : keyLen]];
    }
    bool isPrefixChild(int child, const std::string &k) {
        auto p = keys[child].find('-');
        return is_prefix(k, keys[child].substr(0, p));
    }
    void changeVersion(const std::string &v) {
        if (key.length() > keyLen) {
            key = key.substr(0, keyLen);
        }
        key.append("-");
        key.append(v);
    }
};

class RatPrefix : public MemChecker {
    void _compute(std::vector<NodeRatPrefix> &stack, Int pos) {
        NodeRatPrefix &cur = stack[pos];
        if (!cur.isLeaf) {
            for (int i = 0; i < 16; ++i) {
                if (cur.pointers[i] != -1) {
                    _compute(stack, cur.pointers[i]);
                    cur.hashes[i] = stack[cur.pointers[i]].computeHash();
                    cur.pointers[i] = -1;
                }
            }
        }
        cur.write(io);
    }

protected:
    std::string digest;
    Int height;
    Int num_updates = 0;
    Int version = 0;
    std::string strver = "0";
    std::vector<std::pair<std::string, std::string>> list;

public:
    explicit RatPrefix(Int height) {
        this->height = height;
        digest = "";
    }

    bool init(IO *io, bool create_db, std::string *values) override {
        this->io = io;
        if (!this->io->open()) {
            return false;
        }
        if (create_db) {
            this->io->write("*-0", ":,:,:,:,:,:,:,:,:,:,:,:,:,:,:,:");
            this->io->flush();
            this->digest = calculateSHA256Hash("");
        } else {
            //not available
        }
        return true;
    }

    void update(const std::string &spos, const std::string &value) override {
        list.emplace_back(spos, value);
    }

    std::pair<Int, Int> commit() override {
        std::vector<NodeRatPrefix> stack;
        stack.emplace_back("*-" + strver, io);
        ++version;
        strver = int_to_hex(version);
        stack[0].changeVersion(strver);
        std::string hashUp;
        for (const auto& pair : list) {
            std::string hex(pair.first), value(pair.second);

            int cnt = 0;
            Int pos = 0;
            for ( ; ; ) {
                if (cnt++ > 40) {
                    throw std::invalid_argument("invalid loop");
                }

                NodeRatPrefix &cur = stack[pos];
                int which = cur.ofWhich(hex);
                if (cur.keys[which].empty()) {
                    cur.keys[which] = hex + "-" + strver;
                    cur.pointers[which] = stack.size();
                    stack.emplace_back(hex + "-" + strver, value);
                    break;
                }
                if (cur.pointers[which] != -1) {
                    NodeRatPrefix &next = stack[cur.pointers[which]];
                    if (is_prefix(next.key, hex)) {
                        next.value = value;
                        break;
                    }
                    if (!cur.isPrefixChild(which, hex)) {
                        std::string np(next.key);
                        stack.emplace_back(hex + "-" + strver, value);

                        std::string newKey = common_prefix(hex, np) + "-" + strver;
                        std::vector<std::string> newKeys, newHashes;
                        for (int i = 0; i < 16; ++i) {
                            newKeys.emplace_back("");
                            newHashes.emplace_back("");
                        }
                        stack.emplace_back(newKey, newKeys, newHashes);
                        NodeRatPrefix &newNode = stack[stack.size() - 1];
                        int w1 = newNode.ofWhich(hex);
                        newNode.keys[w1] = hex + "-" + strver;
                        newNode.pointers[w1] = stack.size() - 2;
                        int w2 = newNode.ofWhich(np);
                        newNode.keys[w2] = np;
                        newNode.pointers[w2] = stack[pos].pointers[which];
                        stack[pos].keys[which] = newKey;
                        stack[pos].pointers[which] = stack.size() - 1;
                        break;
                    }
                    pos = cur.pointers[which];
                } else {
                    //warning below
                    if (is_prefix(cur.keys[which], hex)) {
                        cur.keys[which] = hex + "-" + strver;
                        cur.pointers[which] = stack.size();
                        stack.emplace_back(hex + "-" + strver, value);
                        break;
                    }
                    if (!cur.isPrefixChild(which, hex)) {
                        stack.emplace_back(hex + "-" + strver, value);

                        std::string newKey = common_prefix(hex, stack[pos].keys[which]) + "-" + strver;
                        std::vector<std::string> newKeys, newHashes;
                        for (int i = 0; i < 16; ++i) {
                            newKeys.emplace_back("");
                            newHashes.emplace_back("");
                        }
                        stack.emplace_back(newKey, newKeys, newHashes);
                        NodeRatPrefix &newNode = stack[stack.size() - 1];
                        int w1 = newNode.ofWhich(hex);
                        newNode.keys[w1] = hex + "-" + strver;
                        newNode.pointers[w1] = stack.size() - 2;
                        int w2 = newNode.ofWhich(stack[pos].keys[which]);
                        newNode.keys[w2] = stack[pos].keys[which];
                        newNode.hashes[w2] = stack[pos].hashes[which];
                        stack[pos].keys[which] = newKey;
                        stack[pos].pointers[which] = stack.size() - 1;
                        break;
                    }
                    stack.emplace_back(cur.keys[which], io);
                    stack[stack.size() - 1].changeVersion(strver);
                    stack[pos].keys[which] = stack[stack.size() - 1].key;
                    stack[pos].pointers[which] = stack.size() - 1;
                    pos = stack.size() - 1;
                }

            }
        }
        int c = stack.size();
        _compute(stack, 0);
        list.clear();
        this->digest = stack[0].computeHash();
        return std::make_pair(c, c);
    }

    std::string gen_proof(const std::string &spos) override {
        std::string hex(spos), tmp;
        //io->read(hex, tmp);
        //if (tmp.empty())
        //    return "?";

        std::string output;
        std::string key = "*-" + strver;

        for ( ; ; ) {
            NodeRatPrefix cur(key, io);
            int which = cur.ofWhich(hex);
            if (cur.keys[which].empty() || !cur.isPrefixChild(which, hex)) {
                return "?";
            }
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
            if (is_prefix(cur.keys[which], hex)) {
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
        return "rat_prefix_tree";
    }

    ~RatPrefix() override {
        io->flush();
    }
};

class NodeRatCompact {
public:
    std::string key, value, hash;
    std::vector<std::string> keys;
    std::vector<Int> pointers;
    bool isRoot, isLeaf;
    int keyLen;
    explicit NodeRatCompact(const std::string &key, const std::string &value) : pointers(16, -1) {
        isLeaf = true;
        isRoot = false;
        this->key = key;
        this->value = value;
        this->keyLen = 64;
    }
    explicit NodeRatCompact(const std::string &key, const std::vector<std::string> &ks, const std::string &hs): pointers(16, -1), keys(ks) {
        isLeaf = false;
        isRoot = false;
        this->key = key;
        this->keyLen = key.find('-');
        this->hash = hs;
    }
    explicit NodeRatCompact(const std::string &key, IO *io, Int &num, bool quick = false) : pointers(16, -1) {
        std::string v;
        io->read(key, v);
        num += v.length();
        this->hash = v.substr(0, 64);
        if (quick)
            return;
        this->keyLen = key.find('-');
        isRoot = key[0] == '*';
        this->key = key;
        v = v.substr(64);
        if (v[0] == '!') {
            isLeaf = true;
            this->value = v.substr(1);
        } else {
            isLeaf = false;
            Int pos = 0, pred = 0;
            for (int i = 0; i < 16; ++i) {
                while (pos < v.length() && v[pos] != ',')
                    ++pos;
                keys.emplace_back(v.substr(pred, pos - pred));
                pred = pos = pos + 1;
            }
        }
    }
    std::string to_string() {
        if (isLeaf)
            return hash + "!" + value;
        std::string output(hash);
        for (int i = 0; i < 16; ++i) {
            output.append(keys[i]);
            if (i < 15)
                output.append(",");
        }
        return output;
    }
    void write(IO *io, Int &num) {
        std::string tmp(to_string());
        num += tmp.length();
        io->write(key, tmp);
    }
    int ofWhich(const std::string &k) {
        return hti[k[isRoot ? 0 : keyLen]];
    }
    bool isPrefixChild(int child, const std::string &k) {
        auto p = keys[child].find('-');
        return is_prefix(k, keys[child].substr(0, p));
    }
    void changeVersion(const std::string &v) {
        if (key.length() > keyLen) {
            key = key.substr(0, keyLen);
        }
        key.append("-");
        key.append(v);
    }
};

class RatCompact : public MemChecker {
    Int num_read, num_write;
    void _compute(std::vector<NodeRatCompact> &stack, Int pos) {
        NodeRatCompact &cur = stack[pos];
        std::string tmp;
        if (!cur.isLeaf) {
            for (int i = 0; i < 16; ++i) {
                if (cur.pointers[i] != -1) {
                    _compute(stack, cur.pointers[i]);
                    //cur.hashes[i] = stack[cur.pointers[i]].computeHash();
                    tmp.append(stack[cur.pointers[i]].hash);
                    cur.pointers[i] = -1;
                } else {
                    std::string t;
                    io->read(cur.keys[i], t);
                    num_read += t.length();
                    tmp.append(t.substr(0, 64));
                }
            }
            cur.hash = calculateSHA256Hash(tmp);
        } else {
            cur.hash = calculateSHA256Hash(cur.value);
        }

        cur.write(io, num_write);
    }

protected:
    std::string digest;
    Int height;
    Int num_updates = 0;
    Int version = 0;
    std::string strver = "0";
    std::vector<std::pair<std::string, std::string>> list;

public:
    explicit RatCompact(Int height) {
        this->height = height;
        digest = "";
    }

    bool init(IO *io, bool create_db, std::string *values) override {
        this->io = io;
        if (!this->io->open()) {
            return false;
        }
        if (create_db) {
            this->digest = calculateSHA256Hash("");
            this->io->write("*-0", this->digest + ",,,,,,,,,,,,,,,");
            this->io->flush();
        } else {
            //not available
        }
        return true;
    }

    void update(const std::string &spos, const std::string &value) override {
        list.emplace_back(spos, value);
    }

    std::pair<Int, Int> commit() override {
        num_read = num_write = 0;
        std::vector<NodeRatCompact> stack;
        stack.emplace_back("*-" + strver, io, num_read);
        ++version;
        strver = int_to_hex(version);
        stack[0].changeVersion(strver);
        for (const auto& pair : list) {
            std::string hex(pair.first), value(pair.second);

            int cnt = 0;
            Int pos = 0;
            for ( ; ; ) {
                if (cnt++ > 40) {
                    throw std::invalid_argument("invalid loop");
                }

                NodeRatCompact &cur = stack[pos];
                int which = cur.ofWhich(hex);

                if (cur.pointers[which] != -1) {
                    NodeRatCompact &next = stack[cur.pointers[which]];
                    if (is_prefix(next.key, hex)) {
                        next.value = value;
                        break;
                    }
                    if (!cur.isPrefixChild(which, hex)) {
                        std::string np(next.key);
                        stack.emplace_back(hex + "-" + strver, value);

                        std::string newKey = common_prefix(hex, np) + "-" + strver;
                        std::vector<std::string> newKeys;
                        for (int i = 0; i < 16; ++i) {
                            newKeys.emplace_back("");
                        }
                        stack.emplace_back(newKey, newKeys, "");
                        NodeRatCompact &newNode = stack[stack.size() - 1];
                        int w1 = newNode.ofWhich(hex);
                        newNode.keys[w1] = hex + "-" + strver;
                        newNode.pointers[w1] = stack.size() - 2;
                        int w2 = newNode.ofWhich(np);
                        newNode.keys[w2] = np;
                        newNode.pointers[w2] = stack[pos].pointers[which];
                        stack[pos].keys[which] = newKey;
                        stack[pos].pointers[which] = stack.size() - 1;
                        break;
                    }
                    pos = cur.pointers[which];
                } else {
                    if (cur.keys[which].empty()) {
                        cur.keys[which] = hex + "-" + strver;
                        cur.pointers[which] = stack.size();
                        stack.emplace_back(hex + "-" + strver, value);
                        break;
                    }
                    //warning below
                    if (is_prefix(cur.keys[which], hex)) {
                        cur.keys[which] = hex + "-" + strver;
                        cur.pointers[which] = stack.size();
                        stack.emplace_back(hex + "-" + strver, value);
                        break;
                    }
                    if (!cur.isPrefixChild(which, hex)) {
                        stack.emplace_back(hex + "-" + strver, value);

                        std::string newKey = common_prefix(hex, stack[pos].keys[which]) + "-" + strver;
                        std::vector<std::string> newKeys;
                        for (int i = 0; i < 16; ++i) {
                            newKeys.emplace_back("");
                        }
                        stack.emplace_back(newKey, newKeys, "");
                        NodeRatCompact &newNode = stack[stack.size() - 1];
                        int w1 = newNode.ofWhich(hex);
                        newNode.keys[w1] = hex + "-" + strver;
                        newNode.pointers[w1] = stack.size() - 2;
                        int w2 = newNode.ofWhich(stack[pos].keys[which]);
                        newNode.keys[w2] = stack[pos].keys[which];
                        stack[pos].keys[which] = newKey;
                        stack[pos].pointers[which] = stack.size() - 1;
                        break;
                    }
                    stack.emplace_back(cur.keys[which], io, num_read);
                    stack[stack.size() - 1].changeVersion(strver);
                    stack[pos].keys[which] = stack[stack.size() - 1].key;
                    stack[pos].pointers[which] = stack.size() - 1;
                    pos = stack.size() - 1;
                }

            }
        }
        _compute(stack, 0);
        list.clear();
        this->digest = stack[0].hash;
        return std::make_pair(num_read, num_write);
    }

    std::string gen_proof(const std::string &spos) override {
        std::string hex(spos), tmp;
        //io->read(hex, tmp);
        //if (tmp.empty())
        //    return "?";

        std::string output;
        std::string key = "*-" + strver;

        for ( ; ; ) {
            NodeRatCompact cur(key, io, num_read);
            int which = cur.ofWhich(hex);
            if (cur.keys[which].empty() || !cur.isPrefixChild(which, hex)) {
                return "?";
            }
            //if (cur.isRoot && (isLeft && cur.rightKey.empty() || !isLeft && cur.leftKey.empty())) {
            //} else
            {
                output.append(std::string(1, '0' + which));
                for (int i = 0; i < 16; ++i) {
                    if (i != which) {
                        if (cur.keys[i].empty()) {
                            output.append(null64);
                        } else {
                            std::string t;
                            io->read(cur.keys[i], t);
                            output.append(t.substr(0, 64));
                        }
                    }
                }
            }
            if (is_prefix(cur.keys[which], hex)) {
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
        return "rat_compact_tree";
    }

    ~RatCompact() override {
        io->flush();
    }
};

class NodeRatPadding {
public:
    std::string key, value, hash;
    std::vector<std::string> keys;
    std::vector<Int> pointers;
    bool isRoot, isLeaf;
    int keyLen;
    explicit NodeRatPadding(const std::string &key, const std::string &value) : pointers(16, -1) {
        isLeaf = true;
        isRoot = false;
        this->key = key;
        this->value = value;
        this->keyLen = 64;
    }
    explicit NodeRatPadding(const std::string &key, const std::vector<std::string> &ks, const std::string &hs): pointers(16, -1), keys(ks) {
        isLeaf = false;
        isRoot = false;
        this->key = key;
        this->keyLen = key.find('-');
        this->hash = hs;
    }
    explicit NodeRatPadding(const std::string &key, IO *io, Int &num, bool quick = false) : pointers(16, -1) {
        std::string v;
        io->read(key, v);
        if (key.length() < 64)
            throw std::invalid_argument("invalid padding key length");
        num += v.length();
        this->hash = v.substr(0, 64);
        if (quick)
            return;
        this->keyLen = key.find('-');
        isRoot = key[0] == '*';
        this->key = key;
        v = v.substr(64);
        if (v[0] == '!') {
            isLeaf = true;
            this->value = v.substr(1);
        } else {
            isLeaf = false;
            Int pos = 0, pred = 0;
            for (int i = 0; i < 16; ++i) {
                while (pos < v.length() && v[pos] != ',')
                    ++pos;
                keys.emplace_back(v.substr(pred, pos - pred));
                pred = pos = pos + 1;
            }
        }
    }
    std::string to_string() {
        if (isLeaf)
            return hash + "!" + value;
        std::string output(hash);
        for (int i = 0; i < 16; ++i) {
            output.append(keys[i]);
            if (i < 15)
                output.append(",");
        }
        return output;
    }
    void write(IO *io, Int &num) {
        std::string tmp(to_string());
        num += tmp.length();
        if (key.length() < 64)
            throw std::invalid_argument("invalid padding key length");
        io->write(key, tmp);
    }
    int ofWhich(const std::string &k) {
        return hti[k[isRoot ? 0 : keyLen]];
    }
    bool isPrefixChild(int child, const std::string &k) {
        auto p = keys[child].find('-');
        return is_prefix(k, keys[child].substr(0, p));
    }
    void changeVersion(const std::string &v) {
        if (key.length() > keyLen) {
            key = key.substr(0, keyLen);
        }
        key.append("-");
        key.append(v);
        while (key.length() < 64)
            key += "&";
    }
};

class RatPadding : public MemChecker {
    Int num_read, num_write;
    void _compute(std::vector<NodeRatPadding> &stack, Int pos) {
        NodeRatPadding &cur = stack[pos];
        std::string tmp;
        if (!cur.isLeaf) {
            for (int i = 0; i < 16; ++i) {
                if (cur.pointers[i] != -1) {
                    _compute(stack, cur.pointers[i]);
                    //cur.hashes[i] = stack[cur.pointers[i]].computeHash();
                    tmp.append(stack[cur.pointers[i]].hash);
                    cur.pointers[i] = -1;
                } else {
                    std::string t;
                    io->read(cur.keys[i], t);
                    num_read += t.length();
                    tmp.append(t.substr(0, 64));
                }
            }
            cur.hash = calculateSHA256Hash(tmp);
        } else {
            cur.hash = calculateSHA256Hash(cur.value);
        }

        cur.write(io, num_write);
    }

protected:
    std::string digest;
    Int height;
    Int num_updates = 0;
    Int version = 0;
    std::string strver = "0";
    std::vector<std::pair<std::string, std::string>> list;

public:
    explicit RatPadding(Int height) {
        this->height = height;
        digest = "";
    }

    bool init(IO *io, bool create_db, std::string *values) override {
        this->io = io;
        if (!this->io->open()) {
            return false;
        }
        if (create_db) {
            this->digest = calculateSHA256Hash("");
            this->io->write("*-0&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&", this->digest + ",,,,,,,,,,,,,,,");
            this->io->flush();
        } else {
            //not available
        }
        return true;
    }

    void update(const std::string &spos, const std::string &value) override {
        list.emplace_back(spos, value);
    }

    std::pair<Int, Int> commit() override {
        num_read = num_write = 0;
        std::vector<NodeRatPadding> stack;
        std::string rootKey = "*-" + strver;
        while (rootKey.length() < 64)
            rootKey += "&";
        stack.emplace_back(rootKey, io, num_read);
        ++version;
        strver = int_to_hex(version);
        stack[0].changeVersion(strver);
        for (const auto& pair : list) {
            std::string hex(pair.first), value(pair.second);

            int cnt = 0;
            Int pos = 0;
            for ( ; ; ) {
                if (cnt++ > 40) {
                    throw std::invalid_argument("invalid loop");
                }

                NodeRatPadding &cur = stack[pos];
                int which = cur.ofWhich(hex);

                if (cur.pointers[which] != -1) {
                    NodeRatPadding &next = stack[cur.pointers[which]];
                    if (is_prefix(next.key, hex)) {
                        next.value = value;
                        break;
                    }
                    if (!cur.isPrefixChild(which, hex)) {
                        std::string np(next.key);
                        std::string leafKey = hex + "-" + strver;
                        while (leafKey.length() < 64)
                            leafKey += "&";
                        stack.emplace_back(leafKey, value);

                        std::string newKey = common_prefix(hex, np) + "-" + strver;
                        while (newKey.length() < 64)
                            newKey += "&";
                        std::vector<std::string> newKeys;
                        for (int i = 0; i < 16; ++i) {
                            newKeys.emplace_back("");
                        }
                        stack.emplace_back(newKey, newKeys, "");
                        NodeRatPadding &newNode = stack[stack.size() - 1];
                        int w1 = newNode.ofWhich(hex);
                        newNode.keys[w1] = leafKey;
                        newNode.pointers[w1] = stack.size() - 2;
                        int w2 = newNode.ofWhich(np);
                        newNode.keys[w2] = np;
                        newNode.pointers[w2] = stack[pos].pointers[which];
                        stack[pos].keys[which] = newKey;
                        stack[pos].pointers[which] = stack.size() - 1;
                        break;
                    }
                    pos = cur.pointers[which];
                } else {
                    if (cur.keys[which].empty()) {
                        std::string leafKey = hex + "-" + strver;
                        while (leafKey.length() < 64)
                            leafKey += "&";
                        cur.keys[which] = leafKey;
                        cur.pointers[which] = stack.size();
                        stack.emplace_back(leafKey, value);
                        break;
                    }
                    //warning below
                    if (is_prefix(cur.keys[which], hex)) {
                        std::string leafKey = hex + "-" + strver;
                        while (leafKey.length() < 64)
                            leafKey += "&";
                        cur.keys[which] = leafKey;
                        cur.pointers[which] = stack.size();
                        stack.emplace_back(leafKey, value);
                        break;
                    }
                    if (!cur.isPrefixChild(which, hex)) {
                        std::string leafKey = hex + "-" + strver;
                        while (leafKey.length() < 64)
                            leafKey += "&";
                        stack.emplace_back(leafKey, value);

                        std::string newKey = common_prefix(hex, stack[pos].keys[which]) + "-" + strver;
                        while (newKey.length() < 64)
                            newKey += "&";
                        std::vector<std::string> newKeys;
                        for (int i = 0; i < 16; ++i) {
                            newKeys.emplace_back("");
                        }
                        stack.emplace_back(newKey, newKeys, "");
                        NodeRatPadding &newNode = stack[stack.size() - 1];
                        int w1 = newNode.ofWhich(hex);
                        newNode.keys[w1] = leafKey;
                        newNode.pointers[w1] = stack.size() - 2;
                        int w2 = newNode.ofWhich(stack[pos].keys[which]);
                        newNode.keys[w2] = stack[pos].keys[which];
                        stack[pos].keys[which] = newKey;
                        stack[pos].pointers[which] = stack.size() - 1;
                        break;
                    }
                    stack.emplace_back(cur.keys[which], io, num_read);
                    stack[stack.size() - 1].changeVersion(strver);
                    stack[pos].keys[which] = stack[stack.size() - 1].key;
                    stack[pos].pointers[which] = stack.size() - 1;
                    pos = stack.size() - 1;
                }

            }
        }
        _compute(stack, 0);
        list.clear();
        this->digest = stack[0].hash;
        return std::make_pair(num_read, num_write);
    }

    std::string gen_proof(const std::string &spos) override {
        std::string hex(spos), tmp;
        //io->read(hex, tmp);
        //if (tmp.empty())
        //    return "?";

        std::string output;
        std::string key = "*-" + strver;
        while (key.length() < 64)
            key += "&";

        for ( ; ; ) {
            NodeRatPadding cur(key, io, num_read);
            int which = cur.ofWhich(hex);
            if (cur.keys[which].empty() || !cur.isPrefixChild(which, hex)) {
                return "?";
            }
            //if (cur.isRoot && (isLeft && cur.rightKey.empty() || !isLeft && cur.leftKey.empty())) {
            //} else
            {
                output.append(std::string(1, '0' + which));
                for (int i = 0; i < 16; ++i) {
                    if (i != which) {
                        if (cur.keys[i].empty()) {
                            output.append(null64);
                        } else {
                            std::string t;
                            io->read(cur.keys[i], t);
                            output.append(t.substr(0, 64));
                        }
                    }
                }
            }
            if (is_prefix(cur.keys[which], hex)) {
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
        return "rat_padding_tree";
    }

    ~RatPadding() override {
        io->flush();
    }
};

#endif //DUPTREE_RATTREE_HPP
