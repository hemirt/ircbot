#ifndef IRCMESSAGE_HPP_
#define IRCMESSAGE_HPP_

#include <string>
#include <vector>
#include <string_view>
#include <map>
#include <optional>

#include <boost/algorithm/string/replace.hpp>

class IrcMessage
{
public:
    IrcMessage(const std::string& line);
    IrcMessage(std::string&& line);

    enum class Type
    {
        UNKNOWN,
        CLEARMSG,
        GLOBALUSERSTATE,
        PRIVMSG,
        ROOMSTATE,
        USERNOTICE,
        USERSTATE,
        PING
    };

    struct Tag
    {
        Tag()
        {}

        Tag(std::string_view raw_value)
            : raw_value(raw_value)
        {}

        std::string_view raw_value;
        const std::string& value()
        {
            if (escaped_value)
            {
                return *escaped_value;
            }
            escaped_value = std::string(raw_value);
            boost::algorithm::replace_all(*escaped_value, "\\:", ";");
            boost::algorithm::replace_all(*escaped_value, "\\s", " ");
            boost::algorithm::replace_all(*escaped_value, "\\\\", "\\");
            boost::algorithm::replace_all(*escaped_value, "\\r", "\r");
            boost::algorithm::replace_all(*escaped_value, "\\n", "\n");
            return *escaped_value;
        }
    private:
        std::optional<std::string> escaped_value;
    };

    Type type = Type::UNKNOWN;
    std::map<std::string_view, Tag> tags;
    std::string_view prefix;
    std::string_view command;
    std::vector<std::string_view> params;
    std::string_view channel;
    std::string_view user;
    std::string_view user_id;
    std::string_view display_name;
    std::string_view message;
    std::string_view message_id;

    const std::string original_line;

private:
    void parse(std::string_view line);
    void parse_tags(std::string_view tags_line);
    void parse_command();
};

#endif // IRCMESSAGE_HPP_