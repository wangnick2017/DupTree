#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <chrono>
#include <cmath>
#include "tools.hpp"
#include "merkle.hpp"
#include "duptree.hpp"
#include "duptree_child.hpp"
#include "duptree_plus.hpp"
#include "sparse.hpp"
#include "rattree.hpp"

using namespace std;

int testDuptree(int argc, char** argv)
{
    int sample_num = 1000000;
    vector<Int> sample0(10);
    vector<Int> sample1(sample_num);
    vector<Int> sample2(sample_num);
    Int height = stoi(argv[1]);
    Int log_height = (Int)log2((double)height);
    Int num_leaf = 1LL << height;
    Int height_boundary = height;
    //Int height_boundary = 4;
    Int base_height = 4;
    Int base_height_boundary = base_height;
    cout << height << " " << log_height << endl;
    bool create_db = argc < 3 || (stoi(argv[2]) == 1);
    bool delete_db = argc < 4 || (stoi(argv[3]) == 1);

    vector<MemChecker *> checkers = {
            new MerkleSimple(height),
            //new DupTreeSimple(height, height_boundary),
            //new DupTreeBlock(height, height_boundary),
            //new DupTreeChild(height, height_boundary),
            new DupTreePlus<DupTreeSimple>(height, base_height, base_height_boundary),
            //new DupTreePlus<DupTreeChild>(height, base_height, base_height_boundary),
    };

    string rs = random_string();
    for (auto checker : checkers) {
        cout << "\n------ " << checker->get_name() << " ------\n";

        auto *io = new IOLevelDB("db_" + checker->get_name(), 10000);
        // auto *io = new IORocksDB("db_" + checker->get_name(), 10000);
        if (!checker->init(io, create_db, nullptr)) {
            cerr << "open database error: " << io->get_name() << endl;
            return EXIT_FAILURE;
        }
        cout << "-- init end\n";
        io->batch_size = 10000000; //infinity

        random_sample(sample0, 10, num_leaf);
        random_sample(sample1, sample_num, num_leaf);
        random_sample(sample2, sample_num, num_leaf);
        bool flag = true;
        for (Int i: sample0) {
            std::string si(itos(i));
            checker->update(si, rs);
            std::string proof = checker->gen_proof(si);
            flag = flag && checker->verify_proof(si, rs, proof);
        }
        cout << "-- small test: " << (flag ? "passed" : "failed") << endl;

        std::string temp;
        auto start = chrono::high_resolution_clock::now();
        for (Int i: sample1) {
            temp = checker->gen_proof(itos(i));
        }
        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::microseconds>(end - start).count();
        cout << "-- read (generate proof): \t" << right << setw(20) << duration << endl;
        temp = calculateSHA256Hash(temp);

        start = chrono::high_resolution_clock::now();
        for (int i = 0; i < sample_num; ++i) {
            checker->update(itos(sample2[i]), rs);
            if ((i + 1) % 10000 == 0) {
                io->flush();
            }
        }
        end = chrono::high_resolution_clock::now();
        duration = chrono::duration_cast<chrono::microseconds>(end - start).count();
        cout << "-- write (update proof): \t" << right << setw(20) << duration << endl;

        delete checker;
        if (delete_db) {
            io->destroy();
        }
        delete io;
    }
    return 0;
}

int testSparsetree() {
    int sample_num = 1000000;
    vector<Int> sample0(50);
    vector<Int> sample1(sample_num);
    vector<Int> sample2(sample_num);
    Int height = 40;
    cout << height << endl;
    bool create_db = true;
    bool delete_db = true;

    vector<MemChecker *> checkers = {
            //new SparseSimple(height),
            //new SparseBalance(height),
            //new SparseMint(height),
            //new SparseMint2(height),
            //new FatTree(height),
            //new FatMint(height),
            new RatTree(height), //baseline
            //new RatPrefix(height),
            new RatPadding(height), //Prefix with padding
            new RatCompact(height), //Prefix without padding
    };

    //bool sync = stoi(argv[2]) == 1;
    bool sync = false;
    //Int batchSize = stoi(argv[1]);
    Int batchSize = 10000;

    ofstream outfile("debug.txt");
    for (auto checker : checkers) {


        string rs = random_string();

        cout << "\n------ " << checker->get_name() << " ------\n";

        auto *io = new IOLevelDB("db_" + checker->get_name(), batchSize, sync);
        if (!checker->init(io, create_db, nullptr)) {
            cerr << "open database error: " << io->get_name() << endl;
            return EXIT_FAILURE;
        }
        cout << "-- init end\n";

        /*random_sample(sample0, 20, num_leaf);
        random_sample(sample1, sample_num, num_leaf);
        random_sample(sample2, sample_num, num_leaf);
        bool flag = true;
        for (Int i: sample0) {
            std::stringstream stream;
            stream << std::hex << i;
            std::string si( stream.str() );
            rs = random_string();
            checker->update(si, rs);
            std::string proof = checker->gen_proof(si);
            flag = flag && checker->verify_proof(si, rs, proof);
            rs = random_string();
            checker->update(si, rs);
            proof = checker->gen_proof(si);
            flag = flag && checker->verify_proof(si, rs, proof);
        }
        cout << "-- small test: " << (flag ? "passed" : "failed") << endl;*/
        std::ifstream infile;

        std::string temp, line, value;
        Int cnt;


        for (int b = 0; b <= 3; ++b) {
            long durations = 0;
            Int nums_read = 0, nums_write = 0;
            std::stringstream ss;
            ss << std::setw(2) << std::setfill('0') << b;
            infile.open("eth" + ss.str() + ".txt");
            cnt = 0;
            Int nb = 0;
            while (std::getline(infile, line)) {
                if (line[0] == 'r') {
                    std::getline(infile, line);
                    while (line[0] == 'U' || line[0] == 'G') {
                        std::getline(infile, line);
                    }
                    line = line.substr(2);
                    for (int j = 0; j < line.length(); ++j) {
                        if (line[j] >= 'a' && line[j] <= 'z') {
                            line[j] = 'A' + (line[j] - 'a');
                        }
                        if ((line[j] < 'A' || line[j] > 'F') && (line[j] < '0' || line[j] > '9'))
                            throw std::invalid_argument("invalid char in key");
                    }
                    if (line.length() != 40)
                        throw std::invalid_argument("invalid key for write");

                    auto start = chrono::high_resolution_clock::now();
                    temp = checker->gen_proof(line);
                    auto end = chrono::high_resolution_clock::now();
                    auto duration = chrono::duration_cast<chrono::microseconds>(end - start).count();
                    durations += duration;
                    ++cnt;
                } else if (line[0] == 'w') {
                    std::getline(infile, line);
                    while (line[0] == 'U' || line[0] == 'G') {
                        std::getline(infile, line);
                    }
                    std::getline(infile, value);
                    line = line.substr(2);
                    for (int j = 0; j < line.length(); ++j) {
                        if (line[j] >= 'a' && line[j] <= 'z') {
                            line[j] = 'A' + (line[j] - 'a');
                        }
                        if ((line[j] < 'A' || line[j] > 'F') && (line[j] < '0' || line[j] > '9'))
                            throw std::invalid_argument("invalid char in key");
                    }
                    if (line.length() != 40)
                        throw std::invalid_argument("invalid key for write");
                    value = line + value + "@" + to_string(nb);

                    auto start = chrono::high_resolution_clock::now();
                    checker->update(line, value);
                    auto end = chrono::high_resolution_clock::now();
                    auto duration = chrono::duration_cast<chrono::microseconds>(end - start).count();
                    durations += duration;
                    ++cnt;
                } else if (line[0] == 'n') {

                    auto start = chrono::high_resolution_clock::now();
                    auto c = checker->commit();
                    auto end = chrono::high_resolution_clock::now();
                    auto duration = chrono::duration_cast<chrono::microseconds>(end - start).count();
                    durations += duration;
                    nums_read += c.first;
                    nums_write += c.second;

                    nb++;
                    outfile << nb << " \t" << io->buffer.size() << endl;

                    if (nb % 100000 == 0) {
                        cout << "-- Stage " << std::to_string(nb) << ": \t" << right << setw(20) << durations;
                        cout << "\t\tread:\t" << nums_read << "\twrite:\t" << nums_write << endl;
                        durations = 0;
                    }
                }
            }
            infile.close();
            cout << "-- time " << ss.str() << ": \t" << right << setw(20) << durations << endl;
            temp = calculateSHA256Hash(temp);
        }


        delete checker;
        if (delete_db) {
            io->destroy();
        }
        delete io;
    }
    outfile.close();

    return 0;
}

int main(int argc, char** argv) {
    if (argc > 1)
        return testDuptree(argc, argv);
    else
        return testSparsetree();
}
