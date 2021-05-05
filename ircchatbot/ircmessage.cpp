#include "ircmessage.hpp"

#include <exception>
#include <iostream>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

IrcMessage::IrcMessage(const std::string& line)
    : original_line(line)
{
    parse(original_line);
}

IrcMessage::IrcMessage(std::string&& line)
    : original_line(std::move(line))
{
    parse(original_line);
}

void IrcMessage::parse(std::string_view line)
{
    /*
        IRC (RFC 1459)
            <message>  ::= [':' <prefix> <SPACE> ] <command> <params> <crlf>
            <prefix>   ::= <servername> | <nick> [ '!' <user> ] [ '@' <host> ]
            <command>  ::= <letter> { <letter> } | <number> <number> <number>
            <SPACE>    ::= ' ' { ' ' }
            <params>   ::= <SPACE> [ ':' <trailing> | <middle> <params> ]

            <middle>   ::= <Any *non-empty* sequence of octets not including SPACE
                           or NUL or CR or LF, the first of which may not be ':'>
            <trailing> ::= <Any, possibly *empty*, sequence of octets not including
                             NUL or CR or LF>

            <crlf>     ::= CR LF

        IRC3 this with Message Tags
            <message>       ::= ['@' <tags> <SPACE>] [':' <prefix> <SPACE> ] <command> <params> <crlf>
            <tags>          ::= <tag> [';' <tag>]*
            <tag>           ::= <key> ['=' <escaped_value>]
            <key>           ::= [ <client_prefix> ] [ <vendor> '/' ] <key_name>
            <client_prefix> ::= '+'
            <key_name>      ::= <non-empty sequence of ascii letters, digits, hyphens ('-')>
            <escaped_value> ::= <sequence of zero or more utf8 characters except NUL, CR, LF, semicolon (`;`) and SPACE>
            <vendor>        ::= <host>

        line no longer includes <crlf>
    */

    // parse tags
    if (line.starts_with('@'))
    {
        line.remove_prefix(1);
        auto pos = line.find(' ');
        parse_tags(line.substr(0, pos));
        line.remove_prefix(pos + 1);
    }

    // parse prefix
    if (line.starts_with(':'))
    {
        line.remove_prefix(1);
        auto pos = line.find(' ');
        prefix = line.substr(0, pos);
        line.remove_prefix(pos + 1);
    } // else not present (empty)

    // parse command
    {
        auto pos = line.find(' ');
        command = line.substr(0, pos);
        line.remove_prefix(pos + 1);
    }

    // parse params
    while (!line.empty())
    {
        if (line.starts_with(':'))
        {
            line.remove_prefix(1);
            params.push_back(line);
            break;
        }
        else
        {
            auto pos = line.find(' ');
            params.push_back(line.substr(0, pos));
            if (pos == std::string_view::npos)
            {
                break;
            }
            line.remove_prefix(pos + 1);
        }
    }

    parse_command();
}

void IrcMessage::parse_tags(std::string_view tags_line)
{
    std::vector<std::string_view> tagvalues;
    boost::algorithm::split(tagvalues, tags_line, boost::algorithm::is_any_of(";"), boost::algorithm::token_compress_on);
    for (auto&& tagvalue : tagvalues)
    {
        if (auto index = tagvalue.find('='); index != std::string_view::npos)
        {
            tags.insert({ tagvalue.substr(0, index), Tag{tagvalue.substr(index + 1)} });
        }
        else
        {
            tags.insert({ tagvalue, Tag{} });
        }
    }
}

void IrcMessage::parse_command()
{
    if (command == "CLEARMSG") type = Type::CLEARMSG;
    else if (command == "GLOBALUSERSTATE") type = Type::GLOBALUSERSTATE;
    else if (command == "PRIVMSG")
    {
        type = Type::PRIVMSG;
        auto pos = prefix.find('!');
        if (params.size() < 2 || params[0].size() < 2 || pos == std::string_view::npos) {
            std::string msg = "Error parsing PRIVMSG: " + original_line;
            std::cerr << msg << std::endl;
            throw std::runtime_error(msg);
        }
        channel = params[0].substr(1);
        message = params[1];
        user = prefix.substr(0, pos);
        user_id = tags["user-id"].value();
        display_name = tags["display-name"].value();
        message_id = tags["id"].value();
    }
    else if (command == "ROOMSTATE") type = Type::ROOMSTATE;
    else if (command == "USERNOTICE") type = Type::USERNOTICE;
    else if (command == "USERSTATE") type = Type::USERSTATE;
    else if (command == "PING") type = Type::PING;
}