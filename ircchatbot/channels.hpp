#ifndef CHANNELS_HPP
#define CHANNELS_HPP

#include <set>
#include <string>
#include <string_view>

#include "database.hpp"

class Channels
{
public:
    Channels();

    const std::set<std::string, std::less<>>& get_channels() const;
    bool add_channel(std::string_view channel);
    bool remove_channel(std::string_view channel);
private:
    Database channels_db;
    void init_db();

    std::set<std::string, std::less<>> channels;
};

#endif // CHANNELS_HPP