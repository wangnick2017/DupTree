#include "tools.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <openssl/sha.h>
#include <vector>
#include <random>
#include <chrono>
#include <iomanip>
#include <algorithm>

std::string calculateSHA256Hash(const std::string& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data.c_str(), data.length());
    SHA256_Final(hash, &sha256);
    std::stringstream ss;
    for (int j = 0; j < 1; ++j) {
        for (unsigned char i : hash) {
            ss << std::setfill('0') << std::setw(2) << std::hex << static_cast<Int>(i);
        }
    }
    return ss.str();
}

std::string random_string(Int len) {
    std::stringstream ss;
    for (Int i = 0; i < len; ++i) {
        ss << char(random() % 10007);
    }
    return ss.str();
}

Int up_to(Int x, Int v) {
    while (x * 2 < v) {
        x *= 2;
    }
    return x;
}

Int up_to_max(Int x, Int v) {
    while (x * 2 + 1 < v) {
        x = x * 2 + 1;
    }
    return x;
}

void random_sample(std::vector<Int> &sample, Int number, Int max_value) {
    static std::mt19937_64 eng((std::random_device())());
    std::uniform_int_distribution<Int> dist(0, max_value - 1);
    for (int i = 0; i < number; ++i) {
        sample[i] = dist(eng);
    }
}

std::map<char, std::string> htb = {
        {'0', "0000"},
        {'1', "0001"},
        {'2', "0010"},
        {'3', "0011"},
        {'4', "0100"},
        {'5', "0101"},
        {'6', "0110"},
        {'7', "0111"},
        {'8', "1000"},
        {'9', "1001"},
        {'A', "1010"},
        {'B', "1011"},
        {'C', "1100"},
        {'D', "1101"},
        {'E', "1110"},
        {'F', "1111"}};

std::map<char, int> hti = {
        {'0', 0},
        {'1', 1},
        {'2', 2},
        {'3', 3},
        {'4', 4},
        {'5', 5},
        {'6', 6},
        {'7', 7},
        {'8', 8},
        {'9', 9},
        {'A', 10},
        {'B', 11},
        {'C', 12},
        {'D', 13},
        {'E', 14},
        {'F', 15},
        {'a', 10},
        {'b', 11},
        {'c', 12},
        {'d', 13},
        {'e', 14},
        {'f', 15}};

std::string null64 = "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@";

std::string hex_to_binary(const std::string &hex) {
    std::string output;
    for (char i : hex) {
        char j = i;
        if (j >= 'a' && j <= 'z') {
            j = 'A' + (j - 'a');
        }
        output.append(htb[j]);
    }
    return output;
}

std::string int_to_binary(Int decimal, Int numBits) {
    char s[numBits + 1];
    s[numBits] = 0;
    s[0] = '0' + (decimal & 1);
    Int i = 1;
    Int l = 2;
    for (; l <= decimal; ++i, l <<= 1) {
        if ((decimal & l) > 0) {
            s[i] = '1';
        }
        else {
            s[i] = '0';
        }
    }
    for (; i < numBits; ++i)
        s[i] = '0';

    std::string binary(s);
    return binary;
}

std::string int_to_hex(Int decimal, Int numBits) {
    char s[numBits + 1];
    s[numBits] = 0;
    Int i = 0;
    for (; decimal > 0; ++i, decimal >>= 4) {
        Int tmp = decimal & 15;
        s[i] = tmp < 10 ? '0' + tmp : 'A' + (tmp - 10);
    }
    for (; i < numBits; ++i)
        s[i] = '0';

    std::string hex(s);
    return hex;
}

std::string int_to_hex(Int decimal) {
    char s[20];
    Int i = 0;
    for (; decimal > 0; ++i, decimal >>= 4) {
        Int tmp = decimal & 15;
        s[i] = tmp < 10 ? '0' + tmp : 'A' + (tmp - 10);
    }
    s[i] = 0;

    std::string hex(s);
    return hex;
}

bool is_prefix(const std::string &str, const std::string &pre) {
    return str.compare(0, pre.size(), pre) == 0;
}

std::string common_prefix(const std::string &s1, const std::string &s2) {
    std::string result;
    int len = std::min(s1.length(), s2.length());

    // Compare str1 and str2
    for (int i = 0; i < len; i++) {
        if (s1[i] != s2[i])
            break;
        result += s1[i];
    }

    return (result);
}