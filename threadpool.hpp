#ifndef __M_POOL_H__
#define __M_POOL_H__
/*
 * 线程池的实现：线程安全的任务队列+线程
 * 任务类：
 *    class MyTask{}
 * 线程池类：
 *    class ThreadPool{}
 */

#include <iostream>
#include<sstream>
#include<thread>
#include<queue>
#include<vector>
#include<pthread.h>
#include<stdlib.h>
#include<time.h>
#include<unistd.h>
#define MAX_THREAD 5
#define MAX_QUEUE 10
typedef void(*handler_t)(int);
class ThreadTask{
    private:
        int _data;
        handler_t _handler;
    public:
        ThreadTask(int data,handler_t handle)
        :_data(data),_handler(handle) {}
        void SetTask(int data,handler_t handle){
          _data = data;
          _handler = handle;
          return;
        }
        void TaskRun(){
          return _handler(_data);
        }
};



class ThreadPool{
    private:
        std::queue<ThreadTask> _queue;
        int _capacity;
        pthread_mutex_t _mutex;
        pthread_cond_t _cond_pro;
        pthread_cond_t _cond_con;
        int _thr_max;
    private:
         void thr_start(){
            pthread_mutex_lock(&_mutex);
            while(_queue.empty()){
                pthread_cond_wait(&_cond_con,&_mutex);
            }
            ThreadTask tt = _queue.front();
            _queue.pop();
            pthread_mutex_unlock(&_mutex);
            tt.TaskRun();
            pthread_cond_signal(&_cond_pro);
            return;
        }
    public:
        ThreadPool(int maxq = MAX_QUEUE, int maxt = MAX_THREAD)
          :_capacity(maxq),_thr_max(maxt){
            pthread_mutex_init(&_mutex,NULL);
            pthread_cond_init(&_cond_con,NULL);
            pthread_cond_init(&_cond_pro,NULL);
          }
        ~ThreadPool(){
            pthread_cond_destroy(&_cond_con);
            pthread_cond_destroy(&_cond_pro);
            pthread_mutex_destroy(&_mutex);
        }
        bool PoolInit(){
            for(int i = 0; i < _thr_max; i++){
                 std::thread thr(&ThreadPool::thr_start,this);
                 thr.detach();
            }
            return true;
        }
        bool TaskPush(ThreadTask &tt){
            pthread_mutex_lock(&_mutex);
            while(_queue.size() == _capacity){
                pthread_cond_wait(&_cond_pro,&_mutex);
            }
            _queue.push(tt);
            pthread_mutex_unlock(&_mutex);
            pthread_cond_signal(&_cond_con);
            return true;
        } 
};





#endif
