#include <iostream>

#include "chatbot.hpp"

#include <optional>

void run_chatbot(std::vector<std::string>& args)
{
    if (args.size() > 4)
    {
        Chatbot chatbot(args[1], args[2]);
        chatbot.users.add_user(args[3], args[4]);
        chatbot.users.add_admin(args[3], 100);

        std::cout << "Starting bot" << std::endl;
        chatbot.run();
        std::cout << "Quitting bot" << std::endl;
    }
    else
    {
        Chatbot chatbot;

        std::cout << "Starting bot" << std::endl;
        chatbot.run();
        std::cout << "Quitting bot" << std::endl;
    }
}


int main(int argc, char** argv)
{
    std::vector<std::string> args(argv, argv + argc);

    int start_attempts = 1;
    while (start_attempts <= 3)
    {
        try
        {
            run_chatbot(args);
            return 0;
        }
        catch (std::exception& e)
        {
            std::cerr << "Attempt " << start_attempts << " exception: " << e.what() << std::endl;
            std::cerr << "Retrying" << std::endl;
            ++start_attempts;
        }
    }

    return 1;
}