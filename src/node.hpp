#ifndef DUPTREE_NODE_HPP
#define DUPTREE_NODE_HPP

#include <utility>
#include <vector>
#include <string>
#include "tools.hpp"

class Node {
protected:
    std::string hash_val;

public:
    Node() = default;

    explicit Node(const std::string &val) {
        hash_val = val;
    }

    Node(const Node &left_child, const Node &right_child) {
        hash_val = calculateSHA256Hash(left_child.hash_val + right_child.hash_val);
    }

    virtual std::string get_hash_val() {
        return hash_val;
    }

    virtual std::string to_string() {
        return hash_val;
    }
};

class NodeChild : public Node {
    std::vector<std::string> children;
    bool leaf_node = false;

public:
    NodeChild() = default;

    explicit NodeChild(const std::string &val) : Node(val) {
        children.push_back(val);
        leaf_node = true;
    }

    NodeChild(const NodeChild &left_child, const NodeChild &right_child) {
        children.push_back(left_child.hash_val);
        children.push_back(right_child.hash_val);
        hash_val = calculateSHA256Hash(left_child.hash_val + right_child.hash_val);
        leaf_node = false;
    }

    std::string to_string() override {
        if (leaf_node)
            return children[0];
        return hash_val + children[0] + children[1];
    }
};

#endif //DUPTREE_NODE_HPP
