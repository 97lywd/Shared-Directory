#include<iostream>
#include<fstream>
#include<unistd.h>
#include<sstream>
#include<vector>
#include<boost/filesystem.hpp>
#include<boost/algorithm/string.hpp>
#include<stdio.h>
#include<stdlib.h>
#define WWW_ROOT "./www/"
class Boundary{
  public:
    int64_t _start_addr;
    int64_t _data_len;
    std::string _name;
    std::string _filename;
};
bool headerParse(std::string &header,Boundary &file){
    std::cerr << "header:" << header << std::endl;
    std::vector<std::string> list;
    boost::split(list,header,boost::is_any_of("\r\n"),boost::token_compress_on);
    for(int i = 0; i < list.size(); i++){
        std::string sep = ": ";
        size_t pos = list[i].find(sep);
        if(pos == std::string::npos){
            return false;
        }
        std::string key = list[i].substr(0,pos);
        std::string val = list[i].substr(pos + sep.size());
        if(key != "Content-Disposition"){
            continue;
        }
        std::string name_field = "name=\"";
        std::string filename_sep = "filename=\"";
        pos = val.find(name_field);
        if(pos == std::string::npos){
            continue;
        }
        pos = val.find(filename_sep);
        if(pos == std::string::npos){
            return false;
        }
        pos += filename_sep.size();
        size_t next_pos = val.find("\"",pos);
        if(next_pos == std::string::npos){
            return false;
        }
        file._filename = val.substr(pos,next_pos - pos);
        file._name = "fileupload";
    }
    return true;
}
bool Getheader(const std::string &key, std::string &val){
    std::string body;
    char *ptr = getenv(key.c_str());
    if(ptr == NULL){
        return false;
    }
    val = ptr;
    return true;
}
bool BoundaryParse(std::string &body,std::vector<Boundary> &list){
    std::string cont_b = "boundary=";
    std::string tmp;
    if(Getheader("Content-Type",tmp) == false){
        return false;
    }
    size_t pos,next_pos;
    pos = tmp.find(cont_b);
    if(pos == std::string::npos){
        return false;
    }
    std::string boundary = tmp.substr(pos + cont_b.size());
    std::string dash = "--";
    std::string craf = "\r\n";
    std::string tail = "\r\n\r\n";
    std::string f_boundary = dash + boundary + craf;
    std::string m_boundary = craf + dash + boundary;
    std::string l_boundary = "\r\n--" + boundary + "--\r\n";
    pos = body.find(dash + boundary + craf);
    if(pos != 0){
        std::cerr << "first boundary error\n";
        return false;
    }
    pos += f_boundary.size();
    while(pos < body.size()){
        next_pos = body.find(tail,pos);
        if(next_pos == std::string::npos){
            return false;
        }
        std::string header = body.substr(pos,next_pos - pos);
        pos = next_pos + tail.size();   //数据起始位置
        next_pos = body.find(m_boundary,pos); //找\r\n--boundary
        if(next_pos == std::string::npos){
            return false;
        }
        int64_t offet = pos; 
        int64_t length = next_pos - pos;
        next_pos +=  m_boundary.size();   //指向\r\n
        pos = body.find(craf,next_pos);
        if(pos == std::string::npos){
            return false;
        }
        pos += craf.size();    //指向下一个boundary起始地址
        //如果没有boundary，则指向数据的结尾
        Boundary file;
        file._data_len = length;
        file._start_addr = offet;
        headerParse(header,file);
        list.push_back(file);
    }
    return true;
}

bool StorageFile(std::string &body,std::vector<Boundary> &list){
    for(int i = 0; i < list.size(); i++){
        if(list[i]._name != "fileupload"){
            continue;
        }
        std::string realpath = WWW_ROOT + list[i]._filename;
        std::ofstream file(realpath);
        if(!file.is_open()){
            std::cerr << "open file" << realpath << "failed\n";
            return false;
        }
        file.write(&body[list[i]._start_addr],list[i]._data_len);
        if(!file.good()){
            std::cerr << "write file error\n";
            return false;
        }
        file.close();
    }
    return true;
}
int main(int argc,char* argv[],char* env[])
{
    std::string body;
    std::string err = "<html>Failed!!!</html>";
    std::string suc = "<html>Sucess!!<html>";
    char *count_len = getenv("Content-Length");
    if(count_len != NULL){
        std::stringstream tmp;
        tmp << count_len;
        int64_t fsize;
        tmp >> fsize;
        body.resize(fsize);
        int rlen = 0;
        while(rlen < fsize){
            int ret = read(0,&body[0],fsize - rlen);
            if(ret <= 0){
                exit(-1);
            }
            rlen += ret;
        }
        std::cerr << body << std::endl;
        bool ret;
        std::vector<Boundary> list;
        ret = BoundaryParse(body,list);
        //BoundaryParse(body,list);
        if(ret == false){
            std::cerr << "boundary parse error\n";
            std::cout << err;
            return -1;
        }
        ret =  StorageFile(body,list);
        if(ret == false){
            std::cerr << "storage error\n";
            std::cout << err;
            return -1;
        }
        std::cout << suc;
        return 0;
    }
    std::cout << err;
    return 0;
}
