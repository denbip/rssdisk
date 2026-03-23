#include "curl_http.h"

std::once_flag curl_http::load_curl;
std::unordered_map<std::string, ip_array_item> curl_http::IP_ARRAYS;
std::mutex curl_http::LOCK_IP_ARRAYS;

MUTEX_TYPE *curl_http::mutex_buf;

ts_deque<curl_http::threaded_deque_item> curl_http::DEQUE_MSG;
std::thread curl_http::DEQUE_THREAD;
std::atomic<bool> curl_http::DEUQE_IS_RUNNING(ATOMIC_VAR_INIT(true));
bool curl_http::is_inited(false);


curl_http::curl_http()
{
}

std::pair<std::string, std::string> curl_http::explode_uri(const std::string& uri)
{
    std::pair<std::string, std::string> ret;
    std::size_t i = 0u;
    for (; i < uri.size(); ++i)
    {
        if (uri[i] == '?')
        {
            ret.first = uri.substr(0, i);
            ret.second = uri.substr(i + 1);
            break;
        }
    }

    if (ret.first.empty()) ret.first = uri;

    return ret;
}

/**
 * ПОСЫЛАЕМ СМС
 */
std::string curl_http::simpleGet_old(std::string url, int timeOut, int timeUotUSec)
{
    int sock;
    struct sockaddr_in client;
    int PORT = 80;

    basefunc_std::replaceAll(url, "http://", "");
    basefunc_std::replaceAll(url, "https://", "");

    std::string host_ = "";
    std::string url_ = url;

    std::vector<std::string> urlExp = basefunc_std::split(url, '/');
    if (urlExp.size() < 2)
    {
        basefunc_std::cout("Error url " + url, "curl_http::simpleGet", basefunc_std::COLOR::RED_COL);
        basefunc_std::log("Error url " + url, "ERROR_URL");
        return "";
    }

    host_ = urlExp[0];
    basefunc_std::replaceAll(url_, host_, "");

    std::vector<std::string> hostExp = basefunc_std::split(host_, ':');
    if (hostExp.size() == 2)
    {
        PORT = std::atoi(hostExp[1].c_str());
        host_ = hostExp[0];
    }

    struct hostent *host = NULL;

    for (int n = 0; n < 10; ++n)
    {
        if ((host == NULL) || (host->h_addr == NULL))
        {
            if (n != 0)
            {
                basefunc_std::cout("Error retrieving DNS information SMS ... try again ..." + host_, "curl_http::simpleGet", basefunc_std::COLOR::RED_COL);
                basefunc_std::log("Error retrieving DNS information URL: " + url, "$date/ERR_DNS_RETRIEVING");
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            host = gethostbyname(host_.c_str());
        }
        else
        {
            break;
        }
    }

    if ((host == NULL) || (host->h_addr == NULL))
    {
        basefunc_std::cout("Error retrieving DNS information URL: " + url, "curl_http::simpleGet", basefunc_std::COLOR::RED_COL);
        basefunc_std::log("Error retrieving DNS information URL: " + url, "ERROR_DNS_URL");
        return "";
    }

    bzero(&client, sizeof(client));
    client.sin_family = AF_INET;
    client.sin_port = htons( PORT );
    memcpy(&client.sin_addr, host->h_addr, host->h_length);

    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0)
    {
        basefunc_std::cout("Error creating socket: " + url, "curl_http::simpleGet", basefunc_std::COLOR::RED_COL);
        basefunc_std::log("Error creating socket: " + url, "ERROR_SOCKET_URL");
        return "";
    }

    timeval timeout;
    timeout.tv_sec = timeOut;
    timeout.tv_usec = timeUotUSec;

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout)) < 0)
    {
        close(sock);
        basefunc_std::cout("Set Soct opt failed", "curl_http::simpleGet", basefunc_std::COLOR::RED_COL);
        basefunc_std::log("Set Soct opt failed", "SETSOCK_OPT_FAILED");
        return "";
    }
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *) &timeout, sizeof(timeout)) < 0)
    {
        close(sock);
        basefunc_std::cout("Set Soct opt failed", "curl_http::simpleGet", basefunc_std::COLOR::RED_COL);
        basefunc_std::log("Set Soct opt failed", "SETSOCK_OPT_FAILED");
        return "";
    }

    if ( connect(sock, (struct sockaddr *)&client, sizeof(client)) < 0 )
    {
        close(sock);
        basefunc_std::cout("Could not connect to Server URL: " + url, "curl_http::simpleGet", basefunc_std::COLOR::RED_COL);
        basefunc_std::log("Could not connect to Server URL: " + url, "CONNECT_ERROR_URL");
        return "";
    }

    std::stringstream ss;
    ss << "GET "<< url_ <<" HTTP/1.0\r\n"
       << "Host: " + host_ + "\r\n"
       << "Accept: application/json\r\n"
       << "\r\n\r\n";
    std::string request = ss.str();

    basefunc_std::cout(url, "curl_http::simpleGet", basefunc_std::COLOR::GREEN_COL);

    if (send(sock, request.c_str(), request.length(), 0) != (int)request.length())
    {
        close(sock);
        basefunc_std::cout("Error sending request URL: " + url, "curl_http::simpleGet", basefunc_std::COLOR::RED_COL);
        basefunc_std::log("Error sending request URL: " + url, "SEND_ERROR_URL");
        return "";
    }

    char cur[1024];
    bzero(cur, 1024);
    std::string resp = "";
    while ( read(sock, &cur, 1022) > 0 )
    {
        resp += cur;
        bzero(cur, 1024);
    }
    close(sock);

    basefunc_std::cout(resp);

    int pos = resp.find("\r\n\r\n");
    if (resp.find("\r\n\r\n") != std::string::npos)
    {
        if (resp.size() > pos + 4) resp = resp.substr(pos + 4);
    }
    else
    {
        basefunc_std::cout("Header not found: " + url, "curl_http::simpleGet", basefunc_std::COLOR::YELLOW_COL);
        return resp;
    }

    return resp;
}
