#ifndef CHATBOT_HPP_
#define CHATBOT_HPP_

#include <boost/asio.hpp>

#include <memory>

#include "ircauthsequence.hpp"
#include "ircclient.hpp"
#include "ircmessage.hpp"
#include "database.hpp"
#include "users.hpp"
#include "commandshandler.hpp"
#include "channels.hpp"

class Chatbot
{
public:
    // irc_nick and irc_pass from db
    Chatbot();
    // irc_nick and irc_pass from arguments, saved to db
    Chatbot(const std::string& irc_nick, const std::string& irc_pass);

    void run();
    void stop_gracefully();
    void join_channel(std::string_view channel_name);
    void part_channel(std::string_view channel_name);

    Users users;
private:
    std::unique_ptr<IrcClient> irc_client;
    IrcAuthSequence irc_auth;
    std::string irc_nick;
    std::string irc_pass;

    void init();

    void init_auth(const std::string& irc_nick, const std::string& irc_pass);
    void init_irc_client();

    void handle_ircmessage(IrcMessage&& ircmessage);

    boost::asio::io_context io_context;
    std::unique_ptr<boost::asio::io_service::work> dummy_work;

    Database config_db;
    void init_config_db();
    void get_irc_nick_pass_from_config_db();

    void handle_ping(const IrcMessage& ircmessage);
    void handle_privmsg(const IrcMessage& ircmessage);

    CommandsHandler commands_handler;
    void ban_user(std::string_view channel, std::string_view user_id, int timeout);
    bool check_admin_commands(int permissions, const IrcMessage& ircmessage);

    Channels channels;
};

#endif // CHATBOT_HPP_
