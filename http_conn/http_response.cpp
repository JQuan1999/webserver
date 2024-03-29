#include "http_response.h"

std::string http_response::list_html = "/filelist.html";
std::string http_response::index_html = "/index.html";

// http状态码 转化为数字
std::unordered_map<HTTP_CODE, int> http_response::httpCode2number = {
    {NO_REQUEST, -1},
    {GET_REQUEST, 200},
    {BAD_REQUEST, 400},
    {FORBIDDEN_REQUEST, 403},
};

// 状态码
std::unordered_map<int, std::string> http_response::code_status = {
    {200, "OK"},
    {400, "Bad Request"},
    {403, "Forbidden"},
    {404, "Not Found"},
};

// 错误状态码对应的html文件
std::unordered_map<int, std::string> http_response::error_code_path = {
    {400, "/400.html"},
    {403, "/403.html"},
    {404, "/404.html"},
};


std::unordered_map<std::string, std::string> http_response::suffix_type = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};


http_response::http_response(const std::string& r_dir, const std::string& h_dir, const int& connfd)
{
    fd = connfd;
    res_dir = r_dir;
    htmls_dir = h_dir;
    mm_file_address = nullptr;
}

http_response::~http_response()
{
    unmapFile();
}

void http_response::unmapFile()
{
    if(mm_file_address)
    {
        munmap(mm_file_address, file_stat.st_size);
        mm_file_address = nullptr;
    }
}

// 待完成：解析相对路径和绝对路径、并判断请求文件是否存在
void http_response::init(HTTP_CODE h_code, POST_CODE p_code, bool keep_alive, const std::string& http_method, const std::string& get_mode, const std::string& url)
{
    assert(httpCode2number.count(h_code));
    if(mm_file_address)
    {
        unmapFile();
    }
    file_stat = {0};
    code = httpCode2number[h_code]; // 状态码转换
    is_keep_alive = keep_alive;
    method = http_method;
    mode = get_mode;
    _updateIndexHtml();
    
    if(method == "GET"){
        if(mode.size() == 0){
            _initNormal(url);
        }else if(mode == "download"){
            _initDownLoad(url);
        }else if(mode == "delete"){
            _initDelete(url);
        }
    }else{
        _initPost(p_code);
    }
    LOG_DEBUG("fd: %d, request url: %s, httpcode: %d, the returned file path:%s", fd, url.c_str(), code, request_file.c_str());
    assert(stat(request_file.c_str(), &file_stat) == 0 ); // 初始话后的文件一定是可以找到且能访问的
}

void http_response::_initNormal(const std::string& url)
{
    if(url.size() == 0 || (url.size() == 1 && url[0] == '/') || (url == "/index.html" || url == "index.html")){
        request_file = htmls_dir + index_html;
    }else{
        request_file = res_dir + url;
        if(access(request_file.c_str(), F_OK) < 0){
            code = 404;
            request_file = htmls_dir + error_code_path[code];
        }
    }
}

void http_response::_initDownLoad(const std::string& url)
{
    assert(url.size() != 0);
    request_file = res_dir + url;
    if(stat(request_file.c_str(), &file_stat) < 0){
        LOG_DEBUG("fd: %d, the download file path %s is not existed", fd, request_file.c_str());
        code = 404;
        request_file = htmls_dir + error_code_path[code];
    }
}


void http_response::_initDelete(const std::string& url)
{
    struct stat st;
    std::string file_path = res_dir + url;
    // 文件路径存在且不是目录
    if(stat(file_path.c_str(), &st) == 0 && !S_ISDIR(st.st_mode)){
        int ret = remove(file_path.c_str());
        if(ret == 0){
            LOG_INFO("fd: %d, the client request delete the file: %s, delete sucessfully!", fd, url.c_str());
            request_file = htmls_dir + "/delete_sucess.html";
        }else{
            LOG_INFO("fd: %d, the client request delete the file: %s, delete failed!", fd, url.c_str());
            request_file = htmls_dir + "/delete_failed.html";
        }
    }else{
        LOG_INFO("fd: %d, the filepath of the delete file: %s has problem, delete failed!", fd, url.c_str());
        request_file = htmls_dir + "/delete_failed.html";
    }
}


void http_response::_initPost(POST_CODE p_code)
{
    if(p_code == POST_GET_CONTENT){
        request_file = htmls_dir + "/post_sucess.html";
    }else if(p_code == POST_FAILED){
        request_file = htmls_dir + "/post_failed.html";
    }else if(p_code == POST_FILE_EXISTED){
        request_file = htmls_dir + "/post_existed.html";
    }else{
        code = 403;
        request_file = htmls_dir + error_code_path[code];
    }
}


const char* http_response::getFileAddress()
{
    return mm_file_address;
}

size_t http_response::getFileBytes()
{
    return file_stat.st_size;
}


void http_response::_updateIndexHtml()
{
    std::string index_content, temp_line;
    std::string filelist_path = htmls_dir + list_html;
    assert(access(filelist_path.c_str(), F_OK) == 0); // 文件路径存在

    std::ifstream filelist(filelist_path.c_str());
    // 找到插入位置
    while(1)
    {
        std::getline(filelist, temp_line, '\n');
        index_content += temp_line + '\n';
        if(temp_line == "<!--插入位置-->")
        {
            break;
        }
    }

    // 获取files文件下的所有文件名
    DIR* dir = opendir(res_dir.c_str());
    assert(dir != nullptr);

    struct dirent* stdinfo;
    while(1)
    {
        // 获取文件夹下的所有文件
        stdinfo = readdir(dir);
        if(stdinfo == nullptr) break;
        std::string name = stdinfo->d_name;
        if(name == "." || name == "..") continue;

        // 加入表格内容
        index_content += "            <tr><td class=\"col1\">" + name +
                    "</td> <td class=\"col2\"><a href=\"/download/" + name +
                    "\">下载</a></td> <td class=\"col3\"><a href=\"/delete/" + name +
                    "\" onclick=\"return confirmDelete();\">删除</a></td></tr>" + "\n";
    }

    // 加上插入位置之后的内容
    while(std::getline(filelist, temp_line, '\n')){
        index_content += temp_line + "\n";
    }

    std::string save_path = htmls_dir + index_html;
    std::ofstream save_file(save_path, std::ios_base::out);
    save_file<<index_content;

    filelist.close();
    save_file.close();
}


void http_response::response(buffer& write_buf)
{
    // 1.GET 1.1 普通GET 1.2 delete GET 1.3 download GET
    // 2.Post
    _writeStateLine(write_buf);
    _writeHeader(write_buf); 
    _writeContent(write_buf);
}


// 写入状态行
void http_response::_writeStateLine(buffer& write_buf)
{
    // 检测文件类型：是否能打开、是否是目录
    std::string status;
    if(code_status.count(code))
    {
        status = code_status[code];
    }
    else{
        code = 400;
        status = code_status[code];
    }
    write_buf += "HTTP/1.1 " + std::to_string(code) + " " + status + "\r\n";
}

// 写入头部字段
void http_response::_writeHeader(buffer& write_buf)
{
    // connection
    if(is_keep_alive)
    {
        write_buf += "Connection: keep-alive\r\n";
    }else{
        write_buf += "Connection: close\r\n";
    }
    
    // 文件类型(根据request_file文件类型回复content-type)
    int pos = request_file.find('.');
    std::string suffix;
    if(pos != -1){
        suffix = request_file.substr(pos, request_file.size() - pos);
        std::transform(suffix.begin(), suffix.end(), suffix.begin(), tolower);
        // std::cout<<"suffix = "<<suffix<<std::endl;
    }
    if(suffix_type.count(suffix)){
        write_buf += "Content-type: " + suffix_type[suffix] + "\r\n";
    }else{
        write_buf += "Content-type: text/html; chartset=UTF-8\r\n";
    }
    write_buf += "Content-Length: " + std::to_string(file_stat.st_size) + "\r\n\r\n";
}


// 写入传输内容
void http_response::_writeContent(buffer& write_buf)
{
    int src_fd = open(request_file.c_str(), O_RDONLY);
    assert(src_fd > 0);
    int* ret = (int*)mmap(nullptr, file_stat.st_size, PROT_READ, MAP_PRIVATE, src_fd, 0);
    assert(*ret != -1);

    mm_file_address = (char*)ret;
    close(src_fd);
}