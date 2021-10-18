#ifndef ECHOPAGE_HPP_
#define ECHOPAGE_HPP_

#include <curl/curl.h>
#include <mutex>
#include <string>

namespace Hemirt::Utility
{
    
class EchoPage
{
public:
    static EchoPage& get_instance()
    {
        static EchoPage instance;
        return instance;
    }
    
    std::string echo_page(const std::string& page);
   
private:
    EchoPage();
    ~EchoPage();
    EchoPage(const EchoPage&) = delete;
    EchoPage& operator=(const EchoPage&) = delete;
    EchoPage(EchoPage&&) = delete;
    EchoPage& operator=(EchoPage&&) = delete;
    
    std::mutex curl_mtx;
    struct curl_slist *chunk;
    CURL *curl;
};

}





#endif // ECHOPAGE_HPP_