#include "users.hpp"

#include <iostream>

constexpr std::string_view users_db_name = "users.db";

Users::Users()
    : users_db(users_db_name)
{
    init_db();
}

void Users::init_db()
{
    auto result = users_db.execute_statement("PRAGMA foreign_keys = ON;");
    if (result.rc != SQLITE_OK)
    {
        std::string msg = "Users DB table error - unable to enable foreign_keys: " + result.errmsg;
        std::cerr << msg << std::endl;
        throw std::runtime_error(msg);
    }

    result = users_db.execute_statement("CREATE TABLE IF NOT EXISTS users (twitchid TEXT NOT NULL PRIMARY KEY, username TEXT NOT NULL UNIQUE, displayname TEXT UNIQUE);");
    if (result.rc != SQLITE_OK)
    {
        std::string msg = "Users DB table users error: " + result.errmsg;
        std::cerr << msg << std::endl;
        throw std::runtime_error(msg);
    }

    result = users_db.execute_statement("CREATE TABLE IF NOT EXISTS admins (twitchid TEXT NOT NULL PRIMARY KEY, permissions INT NOT NULL DEFAULT 0, FOREIGN KEY(twitchid) REFERENCES users(twitchid) ON UPDATE CASCADE ON DELETE CASCADE);");
    if (result.rc != SQLITE_OK)
    {
        std::string msg = "Users DB table admins error: " + result.errmsg;
        std::cerr << msg << std::endl;
        throw std::runtime_error(msg);
    }
}

bool Users::add_user(std::string_view twitchid, std::string_view username)
{
    auto result = users_db.execute_prepared_statement("INSERT INTO users (twitchid, username) VALUES (?, ?);", { std::string(twitchid), std::string(username) });
    return result.rc == SQLITE_OK;
}

bool Users::add_user(std::string_view twitchid, std::string_view username, std::string_view displayname)
{
    auto result = users_db.execute_prepared_statement("INSERT INTO users (twitchid, username, displayname) VALUES (?, ?, ?) ON CONFLICT DO UPDATE SET displayname=?3;", { std::string(twitchid), std::string(username), std::string(displayname) });
    return result.rc == SQLITE_OK;
}

bool Users::add_admin(std::string_view twitchid, int permissions)
{
    auto result = users_db.execute_prepared_statement("INSERT INTO admins (twitchid, permissions) VALUES (?, ?);", { std::string(twitchid), permissions });
    return result.rc == SQLITE_OK;
}

bool Users::remove_admin(std::string_view twitchid)
{
    auto result = users_db.execute_prepared_statement("DELETE FROM admins WHERE twitchid=?;", { std::string(twitchid) });
    return result.rc == SQLITE_OK;
}

std::optional<Users::User> Users::get_user_by_twitchid(std::string_view twitchid)
{
    auto result = users_db.execute_prepared_statement("SELECT twitchid, username, displayname FROM users WHERE twitchid=?;", { std::string(twitchid) });
    if (result.rc != SQLITE_OK || result.data.size() != 1 || result.data[0].size() != 3)
    {
        return std::nullopt;
    }
    return User{ *result.data[0][0], *result.data[0][1], result.data[0][2] };
}

std::optional<Users::User> Users::get_user_by_username(std::string_view username)
{
    auto result = users_db.execute_prepared_statement("SELECT twitchid, username, displayname FROM users WHERE username=?;", { std::string(username) });
    if (result.rc != SQLITE_OK || result.data.size() != 1 || result.data[0].size() != 3)
    {
        return std::nullopt;
    }
    return User{ *result.data[0][0], *result.data[0][1], result.data[0][2] };
}

std::optional<Users::User> Users::get_user_by_displayname(std::string_view displayname)
{
    auto result = users_db.execute_prepared_statement("SELECT twitchid, username, displayname FROM users WHERE displayname=?;", { std::string(displayname) });
    if (result.rc != SQLITE_OK || result.data.size() != 1 || result.data[0].size() != 3)
    {
        return std::nullopt;
    }
    return User{ *result.data[0][0], *result.data[0][1], result.data[0][2] };
}

std::optional<int> Users::get_admin_permissions(std::string_view twitchid)
{
    auto result = users_db.execute_prepared_statement("SELECT permissions FROM admins WHERE twitchid=?;", { std::string(twitchid) });
    if (result.rc != SQLITE_OK || result.data.size() != 1 || result.data[0].size() != 1 || !result.data[0][0])
    {
        return std::nullopt;
    }
    return std::stoi(*result.data[0][0]);
}