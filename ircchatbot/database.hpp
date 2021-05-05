#ifndef DATABASE_HPP_
#define DATABASE_HPP_

#include "sqlite3.h"

#include <memory>
#include <mutex>
#include <string_view>
#include <vector>
#include <utility>
#include <optional>
#include <variant>

class Database
{
public:
    Database(std::string_view db_file);

    using CallbackFun = int (*)(void*, int, char**, char**);
    struct Result
    {
        int rc;
        std::string errmsg;
        std::vector<std::string> colnames;
        std::vector<std::vector<std::optional<std::string>>> data;
    };
    using ValueType = std::variant<int, double, std::string>;
    Result execute_statement(const std::string& sql);
    Result execute_prepared_statement(const std::string& sql, std::vector<ValueType> values);
    std::pair<int, std::string> execute_statement(const std::string& sql, CallbackFun callback, void* data);

private:
    struct Sqlite3Deleter
    {
        Sqlite3Deleter(std::mutex* db_mutex)
            : db_mutex(db_mutex)
        {}

        Sqlite3Deleter()
            : Sqlite3Deleter(nullptr)
        {}

        void operator()(sqlite3* db) const
        {
            if (db) {
                if (db_mutex)
                {
                    std::lock_guard lk(*db_mutex);
                    sqlite3_close(db);
                }
                else
                {
                    sqlite3_close(db);
                }
            }
        }

        std::mutex* db_mutex;
    };

    std::mutex db_mutex;
    std::unique_ptr<sqlite3, Sqlite3Deleter> db;

    std::string db_filename;
};

#endif // DATABASE_HPP_