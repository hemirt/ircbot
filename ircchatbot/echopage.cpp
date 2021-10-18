#include "echopage.hpp"

#include <iostream>
#include <stdexcept>

namespace Hemirt::Utility
{


EchoPage::EchoPage()
{
    curl = curl_easy_init();
    if (curl) {
        chunk = curl_slist_append(chunk, "Accept: text/plain");
    } else {
        std::cerr << "EchoPage curl error" << std::endl;
        throw std::runtime_error("EchoPage curl error");
    }
}

EchoPage::~EchoPage()
{
    curl_slist_free_all(chunk);
    curl_easy_cleanup(curl);
}

static size_t
WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

std::string EchoPage::echo_page(const std::string& page)
{
    std::lock_guard lk(curl_mtx);
    std::string read_buffer;
    
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    curl_easy_setopt(curl, CURLOPT_URL, page.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_reset(curl);
    
    if (res)
    {
        return std::string("Error: ") + curl_easy_strerror(res);
    }
    
    return read_buffer;
}

}