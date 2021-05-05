#include "database.hpp"

#include <iostream>

Database::Database(std::string_view db_file)
    : db_filename(db_file)
{
    sqlite3* tmp;
    if (int rc = sqlite3_open(db_filename.c_str(), &tmp); rc != SQLITE_OK)
    {
        throw std::runtime_error("Unable to open sqlite3 connection: " + std::to_string(rc));
    }
    db.reset(tmp);
}

static int default_callback(void* result_ptr, int argc, char** argv, char** colnames)
{
    auto& result = *static_cast<Database::Result*>(result_ptr);

    if (result.colnames.empty())
    {
        for (int i = 0; i < argc; ++i)
        {
            result.colnames.push_back(colnames[i]);
        }
    }

    result.data.push_back({});
    auto& new_row = result.data.back();
    for (int i = 0; i < argc; ++i)
    {
        if (argv[i])
        {
            new_row.push_back(argv[i]);
        }
        else
        {
            new_row.push_back(std::nullopt);
        }
    }

    return 0;
}

std::pair<int, std::string> Database::execute_statement(const std::string& sql, CallbackFun callback, void* data)
{
    std::lock_guard lock(db_mutex);
    std::pair<int, std::string> ret;

    char* errmsg;
    ret.first = sqlite3_exec(db.get(), sql.c_str(), callback, data, &errmsg);
    if (errmsg)
    {
        ret.second = errmsg;
        sqlite3_free(errmsg);
    }
    return ret;
}

Database::Result Database::execute_statement(const std::string& sql)
{
    std::lock_guard lock(db_mutex);
    Result result;

    char* errmsg;
    result.rc = sqlite3_exec(db.get(), sql.c_str(), default_callback, &result, &errmsg);
    if (errmsg)
    {
        result.errmsg = errmsg;
        sqlite3_free(errmsg);
    }
    return result;
}

Database::Result Database::execute_prepared_statement(const std::string& sql, std::vector<ValueType> values)
{
    std::lock_guard lock(db_mutex);

    auto on_error = [this](int rc) {
        Result result;
        result.rc = rc;
        result.errmsg = sqlite3_errmsg(db.get());
        return result;
    };

    sqlite3_stmt* stmt;
    if (int rc = sqlite3_prepare_v2(db.get(), sql.c_str(), sql.size() + 1, &stmt, nullptr); rc != SQLITE_OK)
    {
        return on_error(rc);
    }

    int param = 1;
    for (auto&& value : values)
    {
        if (auto pval = std::get_if<int>(&value))
        {
            if (int rc = sqlite3_bind_int(stmt, param, *pval); rc != SQLITE_OK)
            {
                return on_error(rc);
            }
        }
        else if (auto pval = std::get_if<double>(&value))
        {
            if (int rc = sqlite3_bind_double(stmt, param, *pval); rc != SQLITE_OK)
            {
                return on_error(rc);
            }
        }
        else if (auto pval = std::get_if<std::string>(&value))
        {
            if (int rc = sqlite3_bind_text(stmt, param, pval->c_str(), pval->size(), SQLITE_STATIC); rc != SQLITE_OK)
            {
                return on_error(rc);
            }
        }
        else
        {
            throw std::runtime_error("Unhandled ValueType");
        }

        ++param;
    }

    Result result;
    int rc = sqlite3_step(stmt);
    int column_count = sqlite3_column_count(stmt);

    for (int col = 0; col < column_count; ++col)
    {
        result.colnames.push_back(std::string(sqlite3_column_name(stmt, col)));
    }

    while (rc == SQLITE_ROW)
    {
        result.data.push_back({});
        auto& new_row = result.data.back();
        for (int col = 0; col < column_count; ++col)
        {
            const unsigned char* value_ptr = sqlite3_column_text(stmt, col);
            if (value_ptr)
            {
                new_row.push_back(std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, col))));
            }
            else
            {
                new_row.push_back(std::nullopt);
            }
        }

        rc = sqlite3_step(stmt);
    }

    result.rc = sqlite3_finalize(stmt);
    if (result.rc != SQLITE_OK)
    {
        result.errmsg = sqlite3_errmsg(db.get());
    }

    return result;
}