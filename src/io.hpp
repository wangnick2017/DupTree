#ifndef DUPTREE_IO_HPP
#define DUPTREE_IO_HPP

#include <string>
#include <leveldb/db.h>
#include <iostream>
#include <utility>
#include "leveldb/write_batch.h"
#include "tools.hpp"

class IO {
public:
    virtual bool open() = 0;
    virtual void write(const std::string &key, const std::string &value) = 0;
    virtual void flush() = 0;
    virtual bool read(const std::string &key, std::string &value) = 0;
    virtual std::string get_name() = 0;
    virtual void destroy() = 0;
    virtual ~IO() = default;
};

class IOLevelDB : public IO {
protected:
    leveldb::DB *db;

    std::string db_name;
    leveldb::WriteOptions write_options;
    leveldb::ReadOptions read_options;
    leveldb::WriteBatch batch;


public:
    Int batch_size;
    std::unordered_map<std::string, std::string> buffer;
    explicit IOLevelDB(std::string name, Int batch = 10000, bool write_sync = true) : batch_size(batch), db_name(std::move(name)) {
        write_options.sync = write_sync;
        db = nullptr;
    }

    bool open() override {
        leveldb::Options options;
        options.create_if_missing = true;
        leveldb::DestroyDB(db_name, options);
        leveldb::Status status = leveldb::DB::Open(options, db_name, &db);

        if (!status.ok()) {
            std::cerr << "Unable to open/create test database " << db_name << std::endl;
            std::cerr << status.ToString() << std::endl;
            return false;
        }
        return true;
    }

    void write(const std::string &key, const std::string &value) override {
        buffer[key] = value;
        if (buffer.size() >= batch_size) {
            for (const auto& i : buffer)
                batch.Put(i.first, i.second);
            db->Write(write_options, &batch);
            buffer.clear();
            batch.Clear();
        }
    }

    void flush() override {
        if (buffer.size() >= 0) {
            for (const auto& i : buffer)
                batch.Put(i.first, i.second);
            db->Write(write_options, &batch);
            buffer.clear();
            batch.Clear();
        }
    }

    bool read(const std::string &key, std::string &value) override {
        auto it = buffer.find(key);
        if (it != buffer.end()) {
            value = it->second;
            return true;
        }
        return !db->Get(read_options, key, &value).IsNotFound();
    }

    std::string get_name() override {
        return db_name;
    }

    void destroy() override {
        delete db;
        db = nullptr;
        leveldb::DestroyDB(db_name, leveldb::Options());
    }

    ~IOLevelDB() override {
        delete db;
    }
};

class IOMultiple : public IO {
    std::string identifier;
    IO *io;
public:
    explicit IOMultiple(IO *io, Int id = 1) : identifier(itos(id) + "::"), io(io) {}

    void change_id(Int id) {
        identifier = itos(id) + "::";
    }

    bool open() override {
        return true;
    }
    void write(const std::string &key, const std::string &value) override {
        io->write(identifier + key, value);
    }
    void flush() override {
        //not sure
    }
    bool read(const std::string &key, std::string &value) override {
        return io->read(identifier + key, value);
    }

    std::string get_name() override {
        return io->get_name() + identifier;
    }

    void destroy() override {}

    ~IOMultiple() override = default;
};

#endif //DUPTREE_IO_HPP
