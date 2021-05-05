#ifndef COMMANDS_HANDLER_HPP
#define COMMANDS_HANDLER_HPP

#include "database.hpp"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <boost/algorithm/string/regex.hpp>

#include "ircmessage.hpp"

class CommandsHandler
{
public:
    CommandsHandler();

    bool add_banphrase(std::string_view phrase, int timeout);
    bool remove_banphrase(std::string_view phrase);

    bool add_textcommand(std::string_view trigger, std::string_view response);
    bool remove_textcommand(std::string_view trigger);

    /* handle PRIVMSG IrcMessages */
    std::optional<std::string> handle_privmsg(const IrcMessage& ircmessage);
    int is_banphrased(std::string_view line);

private:
    Database commands_db;
    std::map<boost::regex, int> banphrases;
    std::map<std::string, std::string, std::less<>> textcommands;

    void init_db();
};

#endif // COMMANDS_HANDLER_HPP