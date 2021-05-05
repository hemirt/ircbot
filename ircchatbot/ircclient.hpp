#ifndef IRCCLIENT_HPP_
#define IRCCLIENT_HPP_

#include <boost/asio.hpp>

#include <cstddef>
#include <string>
#include <queue>

#include "ircauthsequence.hpp"
#include "ircmessage.hpp"

class IrcClient
{
public:
    using IrcMessageHandler = std::function<void(IrcMessage&&)>;
    IrcClient(boost::asio::io_context& io_context, const IrcAuthSequence& auth, IrcMessageHandler ircmessage_handler);

    void send_command(const std::string& command);
    void send_command(std::string&& command);

    void join_channel(std::string_view channel_name);
    void part_channel(std::string_view channel_name);

    void send_message(std::string_view channel_name, const std::string& message);

    void quit();

private:
    void connect(int attempt = 0);
    void on_host_resolve(const boost::system::error_code& error, boost::asio::ip::tcp::resolver::results_type results, int attempt);
    void try_reconnect_until_max_attempts(int attempt, const std::string& message);
    void report_error(const std::string& message);
    void on_connected(const boost::system::error_code& error, int attempt);
    void login();
    void add_auth_messages_to_queue();

    void write_raw(std::string&& raw_msg);

    void start_write();
    void handle_write(const boost::system::error_code& error, std::size_t bytes_transferred);

    void start_read();
    void handle_read(const boost::system::error_code& error, std::size_t bytes_transferred);

    IrcAuthSequence auth;

    boost::asio::io_context& io_context;
    boost::asio::ip::tcp::socket socket;
    boost::asio::ip::tcp::resolver resolver;

    constexpr static int max_attempts = 5;
    std::queue<std::string> raw_messages;
    boost::asio::streambuf read_buffer;

    IrcMessageHandler ircmessage_handler;

    static constexpr std::size_t max_irc_command_length = 510;

    bool quit_in_progress = false;
};

#endif // IRCCLIENT_HPP_