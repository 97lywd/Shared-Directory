#ifndef __M_HTTP_H__
#define __M_HTTP_H__
#include"tcpsocket.hpp"
#include<unordered_map>
#include<boost/algorithm/string.hpp>
#include<sstream>
class HttpRequest{
    public:
      std::string _method;
      std::string _path;
      std::unordered_map <std::string, std::string> _param;
      std::unordered_map <std::string, std::string> _headers;
      std::string _body;
    private:
        bool RecvHeader(Tcpsocket &sock,std::string &header){
            //1. 探测性接收大量数据
            while(1){
            std::string tmp;
            if(sock.RecvPeek(tmp) == false){
                std::cout << "recv header error\n";
                return false;
            }
            //2. 判断是否包含整个头部\r\n\r\n
            size_t pos = tmp.find("\r\n\r\n",0);
            //3. 判断当前接收数据长度
            if(pos == std::string::npos && tmp.size() == 8192){
                return false;
            }else if(pos != std::string::npos){
            //4. 若包含整个头部，则直接获取头部
                size_t header_length = pos;
                sock.Recv(header,header_length);
                sock.Recv(tmp,4);
                return true;
              }
            }
            return true;
        }
        bool FirstLinePrase(std::string &line){
            //GET / HTTP/1.1
            std::vector<std::string> line_list;
            boost::split(line_list,line,boost::is_any_of(" "), boost::token_compress_on);
            if(line_list.size() != 3){
                std::cerr << "prase first line error\n";
                return false;
            }
            _method = line_list[0];
            size_t pos = line_list[1].find("?",0);
            if(pos == std::string::npos){
                _path = line_list[1];
            }else{
                _path = line_list[1].substr(0,pos);
                std::string query_string;    //查询字符串
                query_string = line_list[1].substr(pos + 1);
                std::vector<std::string> param_list;
                boost::split(param_list,query_string,boost::is_any_of("&"),boost::token_compress_on);
                for(auto i : param_list){
                    size_t param_pos = -1;
                    param_pos = i.find("=");
                    if(param_pos == std::string::npos){
                        std::cerr << "parse param error\n";
                        return false;
                    }
                    std::string key = i.substr(0,param_pos);
                    std::string val = i.substr(param_pos + 1);
                    _param[key] = val;
                }
            }
            return true;
        }
        bool PathIsLegal();
    public:
        int RequestParse(Tcpsocket &sock){
            //1.接收http头部
            std::string header;
            if(RecvHeader(sock,header) == false){
                return 400;
            }
            //2.对整个头部进行按照\r\n进行分隔 - 得到一个list
            std::vector<std::string> header_list;
            boost::split(header_list,header,boost::is_any_of("\r\n"),boost::token_compress_on);
            //3.list[0] -- 首行，进行首行解析
            if(FirstLinePrase(header_list[0]) == false){
                return 400;
            }
            //4.头部分割解析
            size_t pos = 0;
            for(int i = 1; i < header_list.size(); i++){
                pos = header_list[i].find(": ");
                if(pos == std::string::npos){
                    std::cerr << "header parse error\n";
                    return false;
                }
                std::string key = header_list[i].substr(0,pos);
                std::string val = header_list[i].substr(pos + 2);
                 _headers[key] = val;
            }
            //5.请求信息校验
            //6.接受正文
            auto it = _headers.find("Content-Length");
            if(it != _headers.end()){
                std::stringstream tmp;
                tmp << it->second;
                int64_t file_len;
                tmp >> file_len;
                sock.Recv(_body,file_len);
            }
            return 200;
        }
};
class HttpResponse{
    public:
        int _status;
        std::string _body;
        std::unordered_map<std::string,std::string> _headers;
    public:
        bool SetHeader(const std::string &key, const std::string &val){
            _headers[key] = val;
            return true;
        }
        bool ErrorProcess(Tcpsocket &sock){
            std::string body = "bad request";
            std::stringstream tmp;
            tmp << "HTTP/1.1 404 Bad\r\n";
            tmp << "Content-Length: " << std::to_string(body.size()) << "\r\n";
            tmp << "\r\n";
            sock.Send(tmp.str());
            sock.Send(_body);
            return true;
        }
        bool NormalProcess(Tcpsocket &sock){
            //组织首行
            std::stringstream tmp;
            tmp << "HTTP/1.1" << " " << _status << " " << GetDesc();
            tmp << "\r\n";
            if(_headers.find("Content-Length") == _headers.end()){
                std::string len = std::to_string(_body.size());
                _headers["Content-Length"] = len;
            }
            //组织头部
            for(auto i : _headers){
                tmp << i.first << ": " << i.second << "\r\n";
            }
            tmp << "\r\n";
            sock.Send(tmp.str());
            sock.Send(_body);
            return true;
        }
    private:
        std::string GetDesc(){
            switch (_status) {
              case 400 : return "Bad Request";
              case 404 : return "Not found";
              case 200 : return "Ok";
            }
            return "unknow";
        }
};

#endif
