#include "ircclient.hpp"

#include <iostream>
#include <exception>
#include <istream>
#include <memory>

#include "ircmessage.hpp"

IrcClient::IrcClient(boost::asio::io_context& io_context, const IrcAuthSequence& auth, IrcMessageHandler ircmessage_handler)
    : io_context(io_context)
    , socket(io_context)
    , resolver(io_context)
    , auth(auth)
    , ircmessage_handler(ircmessage_handler)
{
    add_auth_messages_to_queue();
    connect();
}

void IrcClient::connect(int attempt)
{
    socket.close();

    auto handler = [this, attempt](auto&& error, auto&& results)
    {
        on_host_resolve(error, results, attempt);
    };

    resolver.async_resolve("irc.chat.twitch.tv", "6667", handler);
}

void IrcClient::on_host_resolve(const boost::system::error_code& error, boost::asio::ip::tcp::resolver::results_type results, int attempt)
{
    if (error)
    {
        std::string error_message = "Host resolution encountered error " + std::to_string(error.value()) + " with message " + error.message();
        try_reconnect_until_max_attempts(attempt, error_message);
        return;
    }

    if (results.empty())
    {
        try_reconnect_until_max_attempts(attempt, "Failed to resolve: empty results");
        return;
    }

    auto handler = [this, attempt](auto&& error)
    {
        on_connected(error, attempt);
    };

    socket.async_connect(*results, handler);
}

void IrcClient::on_connected(const boost::system::error_code& error, int attempt)
{
    if (error)
    {
        std::string error_message = "Connecting encountered error " + std::to_string(error.value()) + " with message " + error.message();
        try_reconnect_until_max_attempts(attempt, error_message);
        return;
    }

    start_read();
    start_write();
}

void IrcClient::start_read()
{
    if (quit_in_progress)
    {
        return;
    }

    auto handler = [this](auto&& error, auto&& bytes_transferred)
    {
        handle_read(error, bytes_transferred);
    };
    boost::asio::async_read_until(socket, read_buffer, "\r\n", handler);
}

void IrcClient::try_reconnect_until_max_attempts(int attempt, const std::string& message)
{

    if (attempt < max_attempts)
    {
        connect(attempt + 1);
    }
    else
    {
        report_error(message);
    }
}

void IrcClient::report_error(const std::string& message)
{
    if (!quit_in_progress)
    {
        std::cerr << message << std::endl;
        throw std::runtime_error(message);
    }
}

void IrcClient::add_auth_messages_to_queue()
{
    for (auto&& command : auth.auth_sequence_messages)
    {
        if (command.size() > max_irc_command_length)
        {
            raw_messages.push(command.substr(0, max_irc_command_length) + "\r\n");
        }
        else
        {
            raw_messages.push(command + "\r\n");
        }
    }
}

void IrcClient::join_channel(std::string_view channel_name)
{
    send_command("JOIN #" + std::string(channel_name));
}

void IrcClient::part_channel(std::string_view channel_name)
{
    send_command("PART #" + std::string(channel_name));
}

void IrcClient::send_message(std::string_view channel_name, const std::string& message)
{
    send_command("PRIVMSG #" + std::string(channel_name) + " :" + message);
}

void IrcClient::send_command(const std::string& command)
{
    if (command.size() > max_irc_command_length)
    {
        write_raw(command.substr(0, max_irc_command_length) + "\r\n");
    }
    else
    {
        write_raw(command + "\r\n");
    }
}

void IrcClient::send_command(std::string&& command)
{
    if (command.size() > max_irc_command_length)
    {
        command.erase(max_irc_command_length);
    }
    command += "\r\n";
    write_raw(std::move(command));
}


void IrcClient::write_raw(std::string&& raw_msg)
{
    auto empty = raw_messages.empty();
    raw_messages.push(std::move(raw_msg));
    if (empty)
    {
        start_write();
    }
}

void IrcClient::start_write()
{
    if (quit_in_progress)
    {
        return;
    }
    auto&& raw_msg = raw_messages.front();

    auto handler = [this](auto&& error, auto&& bytes_transferred)
    {
        handle_write(error, bytes_transferred);
    };

    boost::asio::async_write(socket, boost::asio::buffer(raw_msg.data(), raw_msg.size()), handler);
}

void IrcClient::handle_write(const boost::system::error_code& error, std::size_t bytes_transferred)
{
    if (error)
    {
        std::string error_message = "Write error " + std::to_string(error.value()) + " with message " + error.message();
        report_error(error_message);
        return;
    }

    auto&& raw_msg = raw_messages.front();
    if (bytes_transferred < raw_msg.size())
    {
        raw_msg.erase(0, bytes_transferred);
    }
    else
    {
        raw_messages.pop();
    }

    if (!raw_messages.empty())
    {
        start_write();
    }
}

void IrcClient::handle_read(const boost::system::error_code& error, std::size_t bytes_transferred)
{
    if (error)
    {
        std::string error_message = "Write error " + std::to_string(error.value()) + " with message " + error.message();
        report_error(error_message);
        return;
    }

    std::istream is(&read_buffer);
    std::string line;
    // read until \r\n and get line including \r
    std::getline(is, line);
    // remove \r 
    line.pop_back();

    ircmessage_handler(IrcMessage(std::move(line)));

    start_read();
}

void IrcClient::quit()
{
    quit_in_progress = true;
    socket.close();
}