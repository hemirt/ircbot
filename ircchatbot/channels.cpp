#include "channels.hpp"

#include <iostream>

constexpr std::string_view channels_db_name = "channels.db";

Channels::Channels()
    : channels_db(channels_db_name)
{
    init_db();
}

void Channels::init_db()
{
    auto result = channels_db.execute_statement("CREATE TABLE IF NOT EXISTS channels (channel TEXT NOT NULL PRIMARY KEY);");
    if (result.rc != SQLITE_OK)
    {
        std::string msg = "Channels DB table channels error: " + result.errmsg;
        std::cerr << msg << std::endl;
        throw std::runtime_error(msg);
    }

    {
        result = channels_db.execute_statement("SELECT * FROM channels;");
        if (result.rc != SQLITE_OK)
        {
            std::string msg = "Channels DB table channels select all error: " + result.errmsg;
            std::cerr << msg << std::endl;
            throw std::runtime_error(msg);
        }

        for (auto&& line : result.data)
        {
            channels.insert(*line[0]);
        }
    }
}

const std::set<std::string, std::less<>>& Channels::get_channels() const
{
    return channels;
}

bool Channels::add_channel(std::string_view channel)
{
    auto result = channels_db.execute_prepared_statement("INSERT INTO channels (channel) VALUES (?);", { std::string(channel) });

    channels.insert(std::string(channel));

    return result.rc == SQLITE_OK;
}

bool Channels::remove_channel(std::string_view channel)
{
    auto result = channels_db.execute_prepared_statement("DELETE FROM channels WHERE channel=?;", { std::string(channel) });

    if (auto it = channels.find(channel); it != channels.end())
    {
        channels.erase(it);
    }

    return result.rc == SQLITE_OK;
}