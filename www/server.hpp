/*
 * 
 * server类：服务器的整体结构流程
 *
 */
#include"tcpsocket.hpp"
#include"epollwait.hpp"
#include"http.hpp"
#include"threadpool.hpp"
#include <boost/filesystem.hpp>
#define WWW_ROOT "./www"
class Server{
public:
  bool Start(int port){
      _pool.PoolInit();
      bool ret =  _lst_sock.SocketInit(port);
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
          return;
      }
      HttpProcess(req,rsp);   //根据请求，进行响应
      rsp.NormalProcess(sock);  //发送处理结果给客户端
      sock.Close();
      return;
  }
  static bool HttpProcess(HttpRequest &req, HttpResponse &rsp){
      //若请求是一个POST请求 -- 则多进程CGI进行处理
      //若请求是一个GET请求 -- 且查询字符串不为空 --- CGI
      //否则，请求是GET，并且查询字符串为空
      //判断这个请求是目录还是文件
      std::string realpath = WWW_ROOT + req._path;
      if(!boost::filesystem::exists(req._path)){
          rsp._status = 404;
          return false;
      }
      if((req._method == "GET" && req._param.size() != 0) || req._method == "POST"){
          //此时是文件上传请求
      }else{
          //此时是基本的文件下载/目录列表请求
          if(boost::filesystem::is_directory(realpath)){
              //查看目录
              Listshow(realpath,rsp._body);
              rsp.SetHeader("Content-Type","text/html");
          }else{
              //普通文件下载请求
          }
      }
      rsp._status = 200;
      return true;
  }
      static bool Listshow(std::string &path,std::string &body){
          std::stringstream tmp;
          tmp << "<html><head><style>";
          tmp << "*{margin : 0;}";
          tmp << ".main-window{height : 100%;width : 80%;margin : 0 auto;}";
          tmp << ".upload{position : relative;height : 20%;width : 100%;background-color : #33c0b9;}";
          tmp << ".listshow{position : relative;height : 80%;width : 100%;background : #6fcad6;}";
          tmp << "</style></head>";
          tmp << "<body><div class='main-window'>";
          tmp << "<div class='upload'></div><hr />";
          tmp << "<div calss='listshow'><ol>";
          boost::filesystem::directory_iterator begin(path);
          boost::filesystem::directory_iterator end;
          while(begin != end){
              std::string pathname = begin->path().string();
              std::string name = begin->path().filename().string();
              int64_t ssize = boost::filesystem::file_size(name);
              int64_t mtime = boost::filesystem::last_write_time(name);
              begin++;
              std::cout << "pathname:[" << pathname << "\n";
              std::cout << "name:[" << name << "]\n";
              std::cout << "mtime:[" << mtime << "]\n";
              std::cout << "ssize:[" << ssize << "]\n";
              tmp << "<li><strong><a href='#'>";
              tmp << name;
              tmp << "</a><br /></strong>";
              tmp << "<small>modified: ";
              tmp << mtime;
              tmp << "<br />filetype: application-ostream";
              tmp << ssize / 1024 << "kybtes";
              tmp << "</small></li>";
          }
          body = tmp.str();
      return true;
  }
private:
  Tcpsocket _lst_sock;
  ThreadPool _pool;
  Epoll _epoll;
};
