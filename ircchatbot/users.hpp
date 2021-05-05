#ifndef USERS_HPP_
#define USERS_HPP_

#include "database.hpp"

#include <string>
#include <optional>
#include <string_view>

class Users
{
public:
    Users();
    bool add_admin(std::string_view twitchid, int permissions);
    bool remove_admin(std::string_view twitchid);
    bool add_user(std::string_view twitchid, std::string_view username);
    bool add_user(std::string_view twitchid, std::string_view username, std::string_view displayname);

    struct User
    {
        std::string twitchid;
        std::string username;
        std::optional<std::string> displayname;
    };

    std::optional<User> get_user_by_twitchid(std::string_view twitchid);
    std::optional<User> get_user_by_username(std::string_view username);
    std::optional<User> get_user_by_displayname(std::string_view displayname);
    std::optional<int> get_admin_permissions(std::string_view twitchid);

private:
    Database users_db;
    void init_db();
};

#endif // USERS_HPP_