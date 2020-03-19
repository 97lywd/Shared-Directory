/*
 * 
 * server类：服务器的整体结构流程
 *
 */
#include"tcpsocket.hpp"
#include"epollwait.hpp"
#include"http.hpp"
#include"threadpool.hpp"
#include<fstream>
#include <boost/filesystem.hpp>
#define WWW_ROOT "./www"
class Server{
public:
  bool Start(int port){
      bool ret =  _lst_sock.SocketInit(port);
      if(ret == false)
          return false;
      ret = _pool.PoolInit();
      if(ret == false)
          return false;
      ret = _epoll.Init();
      if(ret == false)
          return false;
      _epoll.Add(_lst_sock);
      while(1){
          std::vector<Tcpsocket> list;
          ret = _epoll.Wait(list);
          if(ret == false){
              sleep(1);
              continue;
          }
          for(int i = 0; i < list.size(); i++){
              if(list[i].GetFd() == _lst_sock.GetFd()){
                  Tcpsocket cli_sock;
                  ret = _lst_sock.Accept(cli_sock);
                  if(ret == false)
                      continue;
                  cli_sock.SetNonBlock();
                  _epoll.Add(cli_sock);
              }else{
                  ThreadTask tt(list[i].GetFd(),ThreadHandler);
                  _pool.TaskPush(tt);
                  _epoll.Del(list[i]);
               }
          }
      }
      _lst_sock.Close();
  }
  static void ThreadHandler(int sockfd){
      Tcpsocket sock;
      sock.SetFd(sockfd);
      HttpResponse rsp;  //响应       
      HttpRequest req;  //请求
      int status = req.RequestParse(sock);   //解析请求
      if(status != 200){
          //解析失败 --- 响应错误
          rsp._status = status;
          rsp.ErrorProcess(sock);
          sock.Close();
          std::cout << "request parse error\n";
          return;
      }
      HttpProcess(req,rsp);   //根据请求，进行响应
      rsp.NormalProcess(sock);  //发送处理结果给客户端
      sock.Close();
      std::cout << "request process over\n";
      return;
  }
  static bool HttpProcess(HttpRequest &req, HttpResponse &rsp){
      //若请求是一个POST请求 -- 则多进程CGI进行处理
      //若请求是一个GET请求 -- 且查询字符串不为空 --- CGI
      //否则，请求是GET，并且查询字符串为空
      //判断这个请求是目录还是文件
      std::string realpath = WWW_ROOT + req._path;
      if(!boost::filesystem::exists(realpath)){
          rsp._status = 404;
          return false;
      }
      if((req._method == "GET" && req._param.size() != 0) || req._method == "POST"){
          //此时是文件上传请求
          CGIProcess(req,rsp);
      }else{
          //此时是基本的文件下载/目录列表请求
          if(boost::filesystem::is_directory(realpath)){
              //查看目录
              Listshow(req,rsp);
              rsp.SetHeader("Content-Type","text/html");
          }else{
              //普通文件下载请求
              RangeDownload(req,rsp);
              return true;
          }
      }
      rsp._status = 200;
      return true;
  }
      static bool CGIProcess(HttpRequest &req, HttpResponse &rsp){
          int pipe_in[2],pipe_out[2];
          if(pipe(pipe_in) < 0 || pipe(pipe_out) < 0){
              std::cerr << "create pipe error\n";
              return false;
          }
          int pid = fork();
          if(pid < 0){
              return false;
          }else if(pid == 0){
              close(pipe_in[0]);   //子进程写，父进程读   关闭读端
              close(pipe_out[1]);   //子进程读，父进程写  关闭写端
              dup2(pipe_out[0],0);
              dup2(pipe_in[1],1);
              setenv("METHOD",req._method.c_str(),1);
              setenv("PATH",req._path.c_str(),1);
              for(auto i : req._headers){
                  setenv(i.first.c_str(),i.second.c_str(),1);
              }
              std::string realpath = WWW_ROOT + req._path;
              execl(realpath.c_str(),realpath.c_str(),NULL);
              exit(0);
          }
          close(pipe_in[1]);
          close(pipe_out[0]);
          write(pipe_out[1],&req._body[0],req._body.size());
          while(1){
              char buff[1024] = {0};
              int ret = read(pipe_in[0],buff,1024);
              if(ret == 0){
                  break;
              }
              buff[ret] = '\0';
              rsp._body += buff;
          }
          close(pipe_in[0]);
          close(pipe_out[1]);
          return true;
      }
      static int64_t str_to_digit(const std::string val){
          std::stringstream tmp;
          tmp << val;
          int64_t dig = 0;
          tmp >> dig;
          return dig;
      }
      static bool RangeDownload(HttpRequest &req,HttpResponse &rsp){
          std::string realpath = WWW_ROOT + req._path;
          int64_t data_len = boost::filesystem::file_size(realpath);
          int64_t last_mtime = boost::filesystem::last_write_time(realpath);
          std::string etag = std::to_string(data_len) + std::to_string(last_mtime);
          auto it = req._headers.find("Range");
          if(it == req._headers.end()){
              Download(realpath,0,data_len,rsp._body);
              rsp._status = 200;
          }
          else{
              std::string range = it->second;
              std::string unit = "bytes=";
              size_t pos = range.find(unit);
              if(pos == std::string::npos){
                  return false;
              }
              pos += unit.size();
              size_t pos2 = range.find("-",pos);
              if(pos2 == std::string::npos){
                  return false;
              }
              std::string start = range.substr(pos,pos2 - pos);
              std::string end = range.substr(pos2 + 1);
              int64_t dig_start,dig_end;
              dig_start = str_to_digit(end);
              if(end.size() == 0){
                  dig_end = data_len - 1;
              }else{
                  dig_end =str_to_digit(end);
              }
              int64_t range_len = dig_end - dig_start + 1;
              Download(realpath,dig_start,range_len,rsp._body);
              std::stringstream tmp;
              tmp << "bytes " << dig_start << "-" << dig_end << "/" << data_len;
              rsp.SetHeader("Content-Range",tmp.str());
              rsp._status = 206;
          }
          rsp.SetHeader("Content-Type","application/octet-stream");
          rsp.SetHeader("Accept-Ranges","bytes");
          rsp.SetHeader("ETag",etag);
          return true; 
      }
      static bool Download(std::string &path,int64_t start, int64_t len,std::string &body){
          int64_t fsize = boost::filesystem::file_size(path);
          body.resize(fsize);
          std::ifstream file(path);
          if(!file.is_open()){
              std::cerr << "open file error\n";
              return false;
          }
          file.seekg(start,std::ios::beg);
          file.read(&body[0],len);
          if(!file.good()){
              std::cerr << "read file data error\n";
              return false;
          }
          file.close();
          return true;
      }
      static bool Listshow(HttpRequest &req,HttpResponse &rsp){
          std::string realpath = WWW_ROOT + req._path;
          std::string req_path = req._path;
          std::stringstream tmp;
          tmp << "<html><head><style>";
          tmp << "*{margin : 0;}";
          tmp << ".main-window{height : 100%;width : 80%;margin : 0 auto;}";
          tmp << ".upload{position : relative;height : 20%;width : 100%;background-color : #33c0b9; text-align:center;}";
          tmp << ".listshow{position : relative;height : 80%;width : 100%;background : #6fcad6;}";
          tmp << "</style></head>";
          tmp << "<body><div class='main-window'>";
          tmp << "<div class='upload'>";
          tmp << "<form action='/upload' method='POST'";
          tmp << "enctype='multipart/form-data'>";
          tmp << "<div class='upload-btn'>";
          tmp << "<input type='file' name='fileupload'>";
          tmp << "<input type='submit' name='submit'>";
          tmp << "</div></form>";
          tmp << "</div><hr />";
          tmp << "<div calss='listshow'><ol>";
          boost::filesystem::directory_iterator begin(realpath);
          boost::filesystem::directory_iterator end;
          for(;begin != end; ++begin){
              std::string pathname = begin->path().string();
              std::string name = begin->path().filename().string();
              std::string uri = req_path + name;
              if(boost::filesystem::is_directory(pathname)){
              tmp << "<li><strong><a href='";
              tmp << uri <<"'>";
              tmp << name << "/";
              tmp << "</a><br /></strong>";
              tmp << "<small>filetype: directory";
              tmp << "</small></li>";
              }else{
              int64_t ssize = boost::filesystem::file_size(begin->path());
              int64_t mtime = boost::filesystem::last_write_time(begin->path());
              tmp << "<li><strong><a href='";
              tmp << uri << "'>";
              tmp << name;
              tmp << "</a><br /></strong>";
              tmp << "<small>modified: ";
              tmp << mtime;
              tmp << "<br />filetype: application-ostream";
              tmp << ssize / 1024 << "kybtes";
              tmp << "</small></li>";
              }
          }
          tmp << "</ol></div><hr /></div></body></html>";
          rsp._body = tmp.str();
          rsp._status = 200;
          rsp.SetHeader("Content-Type","test/html");
          return true;
      }
private:
  Tcpsocket _lst_sock;
  ThreadPool _pool;
  Epoll _epoll;
};

