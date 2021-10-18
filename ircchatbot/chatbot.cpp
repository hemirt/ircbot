#include "chatbot.hpp"

#include <iostream>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

constexpr std::string_view config_db_name = "config.db";

Chatbot::Chatbot()
    : config_db(config_db_name)
{
    get_irc_nick_pass_from_config_db();
    init();
}

Chatbot::Chatbot(const std::string& irc_nick, const std::string& irc_pass)
    : config_db(config_db_name)
    , irc_nick(irc_nick)
    , irc_pass(irc_pass)
{
    init_config_db();
    init();
}

void Chatbot::init()
{
    init_auth(irc_nick, irc_pass);
    init_irc_client();
    // join without saving to db
    irc_client->join_channel(irc_nick);
    for (auto&& channel : channels.get_channels())
    {
        // join from db
        join_channel(channel);
    }
}

void Chatbot::get_irc_nick_pass_from_config_db()
{
    auto result = config_db.execute_prepared_statement("SELECT key, value FROM config WHERE key IN (?, ?) ORDER BY key ASC", { "irc_nick", "irc_pass" });
    if (result.rc != SQLITE_OK)
    {
        std::string msg = "Config DB error, unable to get irc_nick and irc_pass: " + result.errmsg;
        std::cerr << msg << std::endl;
        throw std::runtime_error(msg);
    }
    if (result.data.size() != 2)
    {
        std::string msg = "Config DB error, no irc_nick and irc_pass found";
        std::cerr << msg << std::endl;
        throw std::runtime_error(msg);
    }
    // ASC order: irc_nick, irc_pass
    if (result.data[0][1] && result.data[1][1])
    {
        irc_nick = *result.data[0][1];
        irc_pass = *result.data[1][1];
    }
    else
    {
        std::string msg = "Config DB error, null data in irc_nick or irc_pass found";
        std::cerr << msg << std::endl;
        throw std::runtime_error(msg);
    }
}

void Chatbot::init_config_db()
{
    auto result = config_db.execute_statement("CREATE TABLE IF NOT EXISTS config (key TEXT NOT NULL PRIMARY KEY, value TEXT NOT NULL);");
    if (result.rc != SQLITE_OK)
    {
        std::string msg = "Config DB error: " + result.errmsg;
        std::cerr << msg << std::endl;
        throw std::runtime_error(msg);
    }

    result = config_db.execute_prepared_statement("REPLACE INTO config (key, value) VALUES ('irc_nick', ?), ('irc_pass', ?);", { irc_nick, irc_pass });
    if (result.rc != SQLITE_OK)
    {
        std::string msg = "Config DB error: " + result.errmsg;
        std::cerr << msg << std::endl;
        throw std::runtime_error(msg);
    }
}

void Chatbot::init_auth(const std::string& irc_nick, const std::string& irc_pass)
{
    irc_auth.auth_sequence_messages.insert(irc_auth.auth_sequence_messages.end(), { "PASS " + irc_pass, "NICK " + irc_nick, "CAP REQ :twitch.tv/tags", "CAP REQ :twitch.tv/commands" });
}

void Chatbot::init_irc_client()
{
    irc_client = std::make_unique<IrcClient>(io_context, irc_auth, [this](IrcMessage&& ircmessage) { this->handle_ircmessage(std::move(ircmessage)); });
}

void Chatbot::handle_ircmessage(IrcMessage&& ircmessage)
{
    switch (ircmessage.type)
    {
    case IrcMessage::Type::PRIVMSG:
    {
        handle_privmsg(ircmessage);
        break;
    }
    case IrcMessage::Type::PING:
    {
        handle_ping(ircmessage);
        break;
    }
    default:
        std::cout << ircmessage.original_line << std::endl;
        break;
    }
}

void Chatbot::handle_ping(const IrcMessage& ircmessage)
{
    std::string pong = "PONG ";
    pong += ircmessage.params[0];
    irc_client->send_command(std::move(pong));
}

void Chatbot::handle_privmsg(const IrcMessage& ircmessage)
{
    /* add as known user */
    if (ircmessage.display_name.empty())
    {
        users.add_user(ircmessage.user_id, ircmessage.user);
    }
    else
    {
        users.add_user(ircmessage.user_id, ircmessage.user, ircmessage.display_name);
    }

    auto user_is_admin = users.get_admin_permissions(ircmessage.user_id);

    if (user_is_admin) /* check admin commands */
    {
        check_admin_commands(*user_is_admin, ircmessage);
    }
    else /* check banphrases */
    {
        /*
        auto timeout = commands_handler.is_banphrased(ircmessage.message);
        if (timeout)
        {
            ban_user(ircmessage.channel, ircmessage.user, timeout);
            return;
        }
        */
    }

    /* check textcommands */
    if (auto response = commands_handler.handle_privmsg(ircmessage); response && !commands_handler.is_banphrased(*response))
    {
        irc_client->send_message(ircmessage.channel, *response);
    }
}

void Chatbot::ban_user(std::string_view channel, std::string_view username, int timeout)
{
    if (timeout == -1)
    {
        irc_client->send_message(std::string(channel), "/ban " + std::string(username));
    }
    else
    {
        irc_client->send_message(std::string(channel), "/timeout " + std::string(username) + " " + std::to_string(timeout));
    }
}

void Chatbot::run()
{
    dummy_work = std::make_unique<boost::asio::io_service::work>(io_context);
    io_context.run();
}

void Chatbot::stop_gracefully()
{
    irc_client->quit();
    dummy_work.reset();
}

void Chatbot::join_channel(std::string_view channel_name)
{
    channels.add_channel(channel_name);
    irc_client->join_channel(channel_name);
}

void Chatbot::part_channel(std::string_view channel_name)
{
    channels.remove_channel(channel_name);
    irc_client->part_channel(channel_name);
}

std::string connect_tokens(const std::vector<std::string_view>& tokens, std::size_t start, std::size_t end)
{
    std::string result;
    for (std::size_t i = start; i < end; ++i)
    {
        result += tokens[i];
        result += ' ';
    }
    if (!result.empty())
    {
        // remove extra space
        result.pop_back();
    }
    return result;
}

void Chatbot::check_admin_commands(int permissions, const IrcMessage& ircmessage)
{
    if (permissions < 100)
    {
        return;
    }

    std::vector<std::string_view> tokens;
    boost::algorithm::split(tokens, ircmessage.message, boost::algorithm::is_any_of(" "));
    if (tokens.size() == 0)
    {
        return;
    }

    auto&& trigger = tokens[0];
    if (trigger == "!quit")
    {
        stop_gracefully();
        return;
    }
    else if (trigger == "!addcmd" && tokens.size() >= 3)
    {
        auto&& command_trigger = tokens[1];
        std::string response = connect_tokens(tokens, 2, tokens.size());

        if (commands_handler.add_textcommand(command_trigger, response))
        {
            irc_client->send_message(ircmessage.channel, std::string(ircmessage.user) + ", added a new command");
        }
        else
        {
            irc_client->send_message(ircmessage.channel, std::string(ircmessage.user) + ", failed to add a new command");
        }
        return;
    }
    else if (trigger == "!delcmd" && tokens.size() >= 2)
    {
        auto&& command_trigger = tokens[1];

        if (commands_handler.remove_textcommand(command_trigger))
        {
            irc_client->send_message(ircmessage.channel, std::string(ircmessage.user) + ", removed a command");
        }
        else
        {
            irc_client->send_message(ircmessage.channel, std::string(ircmessage.user) + ", failed to remove a command");
        }
        return;
    }
    else if (trigger == "!addbanphrase" && tokens.size() >= 3)
    {
        std::string timeout_str(tokens.back());
        int timeout = std::atoi(timeout_str.c_str());
        if (timeout == 0)
        {
            irc_client->send_message(ircmessage.channel, std::string(ircmessage.user) + ", zero or no timeout duration provided");
            return;
        }
        std::string phrase = connect_tokens(tokens, 1, tokens.size() - 1);

        if (commands_handler.add_banphrase(phrase, timeout))
        {
            irc_client->send_message(ircmessage.channel, std::string(ircmessage.user) + ", added a new banphrase");
        }
        else
        {
            irc_client->send_message(ircmessage.channel, std::string(ircmessage.user) + ", failed to add a new banphrase");
        }
        return;
    }
    else if (trigger == "!delbanphrase" && tokens.size() >= 2)
    {
        std::string phrase = connect_tokens(tokens, 1, tokens.size());

        if (commands_handler.remove_banphrase(phrase))
        {
            irc_client->send_message(ircmessage.channel, std::string(ircmessage.user) + ", removed a banphrase");
        }
        else
        {
            irc_client->send_message(ircmessage.channel, std::string(ircmessage.user) + ", failed to remove a banphrase");
        }
        return;
    }
    else if (trigger == "!addadmin" && tokens.size() >= 3)
    {
        auto&& newadmin = tokens[1];
        std::string perms(tokens[2]);
        int new_perm = std::atoi(perms.c_str());
        if (new_perm > permissions)
        {
            irc_client->send_message(ircmessage.channel, std::string(ircmessage.user) + ", not enough permissions");
            return;
        }

        auto AdminUser = users.get_user_by_username(newadmin);
        if (AdminUser)
        {
            users.add_admin(AdminUser->twitchid, new_perm);
            std::string message(ircmessage.user);
            message += ", added new admin ";
            message += newadmin;
            irc_client->send_message(ircmessage.channel, message);
        }
        else
        {
            irc_client->send_message(ircmessage.channel, std::string(ircmessage.user) + ", could not find user in database");
        }
        return;
    }
    else if (trigger == "!deladmin" && tokens.size() >= 2)
    {
        auto&& newadmin = tokens[1];

        auto AdminUser = users.get_user_by_username(newadmin);
        if (AdminUser)
        {
            auto other_perm = users.get_admin_permissions(AdminUser->twitchid);
            if (other_perm)
            {
                if (other_perm > permissions)
                {
                    irc_client->send_message(ircmessage.channel, std::string(ircmessage.user) + ", not enough permissions");
                }
                else
                {
                    users.remove_admin(AdminUser->twitchid);
                    irc_client->send_message(ircmessage.channel, std::string(ircmessage.user) + ", removed admin user " + AdminUser->username);
                }
            }
            else
            {
                irc_client->send_message(ircmessage.channel, std::string(ircmessage.user) + ", user is not an admin");
            }
        }
        else
        {
            irc_client->send_message(ircmessage.channel, std::string(ircmessage.user) + ", could not find user in database");
        }
        return;
    }
    else if (trigger == "!joinchn" && tokens.size() >= 2)
    {
        auto&& channel = tokens[1];
        join_channel(channel);
        irc_client->send_message(ircmessage.channel, std::string(ircmessage.user) + ", joined channel " + std::string(channel));
    }
    else if (trigger == "!partchn" && tokens.size() >= 2)
    {
        auto&& channel = tokens[1];
        part_channel(channel);
        irc_client->send_message(ircmessage.channel, std::string(ircmessage.user) + ", parted channel " + std::string(channel));
    }
    else if (trigger == "!cmdaddchn" && tokens.size() >= 3)
    {
        auto&& cmd = tokens[1];
        auto&& channel = tokens[2];
        
        auto ret = commands_handler.add_channel_to_command(cmd, channel);
        irc_client->send_message(ircmessage.channel, std::string(ircmessage.user) + ", " + (ret ? "true" : "false"));
    }
    else if (trigger == "!cmdadduid" && tokens.size() >= 3)
    {
        auto&& cmd = tokens[1];
        auto&& uid = tokens[2];
        
        auto ret = commands_handler.add_userid_to_command(cmd, uid);
        irc_client->send_message(ircmessage.channel, std::string(ircmessage.user) + ", " + (ret ? "true" : "false"));
    }
    else if (trigger == "!cmdtogglechns" && tokens.size() >= 2)
    {
        auto&& cmd = tokens[1];
        
        auto ret = commands_handler.toggle_channels_to_command(cmd);
        irc_client->send_message(ircmessage.channel, std::string(ircmessage.user) + ", " + std::to_string(ret));
    }
    else if (trigger == "!cmdtoggleuids" && tokens.size() >= 2)
    {
        auto&& cmd = tokens[1];
        
        auto ret = commands_handler.toggle_userids_to_command(cmd);
        irc_client->send_message(ircmessage.channel, std::string(ircmessage.user) + ", " + std::to_string(ret));
    }
}