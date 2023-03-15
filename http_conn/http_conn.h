#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <netinet/in.h>
#include <cstring>
#include <string>
#include <sys/epoll.h>
#include <memory>
#include <arpa/inet.h>
#include <sys/uio.h>

#include "http_request.h"
#include "http_response.h"
#include "http_enum.h"
#include "../buffer/buffer.h"


// 客户连接请求的类
class httpConn
{
public:
    httpConn();
    void init(int, sockaddr_in, const std::string&, uint32_t);
    ~httpConn();
    int httpRead(int&);
    bool parseHttpRequest();
    bool process();
    bool isKeepAlive();
    int httpWrite(int&);
    void closeData();
    const char* getIp();
    const int getPort();
    const buffer& getReadBuf();
    const buffer& getWriteBuf();

private:
    bool _readProcess();

private:
    struct sockaddr_in address;
    int sockfd;
    int port;
    char ip[20];
    std::string resource_dir;
    uint32_t conn_mode; // 事件类型

    std::unique_ptr<buffer> read_buffer;
    std::unique_ptr<buffer> write_buffer;

    std::string root_path; // 服务器根目录

    std::unique_ptr<http_request> m_request; // 请求解析
    std::unique_ptr<http_response> m_response; // 请求回复
    struct iovec m_iv[2];
    int m_iv_cnt;
};

#endif