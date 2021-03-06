/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: jian yi, eyjian@qq.com
 */
#ifndef MOOON_SYS_SIGNAL_HANDLER_H
#define MOOON_SYS_SIGNAL_HANDLER_H
#include "mooon/sys/config.h"
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
SYS_NAMESPACE_BEGIN

// 信号基础知识：
// SIGCHLD等信号是不可靠的，比如理论上触发了两次SIGCHLD信号
// ，但实际只发生了一次信号事件，CSignalHandler::handle()采用循环的方式处理了这种情况
// ，信号在内核中是存储在一个队列中，因此是先进先出的
// ，如果一个信号被阻塞，则当信号发生时，进程将感知不到该信号，但可以使用sigwait()将信号从队列中取出来
// ，对于多线程，当信号发生时，由哪个线程来处理信号是不定的，安全的处理方式是阻塞需要处理的信号，
// ，然后将由专门的单个线程来处理信号事件。

/***
 * Linux信号处理工具
 * 在主线程中阻塞需要处理的信号
 * ，然后在专门的线程中循环调用CSignalHandler::handle()对信号进行处理
 * 使用方法，请参见类定义后的示例。
 */
class CSignalHandler
{
public:
    /***
     * 忽略指定信号
     * @signo 需要被忽略的信号
     * 调用成功返回true，否则返回false，出错原因可通过errno取得
     */
    static bool ignore_signal(int signo) throw ();

    /***
     * 阻塞指定的信号
     * @signo 需要被阻塞的信号
     * 调用成功返回true，否则返回false，出错原因可通过errno取得
     */
    static bool block_signal(int signo) throw ();

    /***
     * 等待信号发生
     * ，如果没有信号发生，则调用阻塞，直到有信号发生时，才会解除阻塞立即返回
     * 出错返回-1，出错原因可通过errno取得，调用成功返回非-1值
     */
    static int wait_signal() throw ();

    /***
     * 等待信号
     * 如果没有信号发生，则调用会被阻塞
     * @on_terminated 进程本身收到SIGTERM退出信号时被回调
     * @on_child_end 收到SIGCHLD信号时被回调
     * @on_signal_handler 收到SIGTERM和SIGCHLD外的信号时被回调
     * @on_exception 异常回调，在handle()中系统调用异常时触发，可不关注
     *
     * @child_pid 子进程ID
     * @child_exited_status 子进程退出状态，可利用WIFEXITED、WEXITSTATUS等宏进一步处理，详情请查看waitpid的man说明
     * @signo 发生的信号
     * @errcode 系统调用（sigemptyset/sigaddset/sigwait/waitpid）出错时的errno值
     */
    static void handle(
            void (*on_terminated)(),
            void (*on_child_end)(pid_t child_pid, int child_exited_status),
            void (*on_signal_handler)(int signo),
            void (*on_exception)(int errcode)) throw ();

private:
    static sigset_t _sigset;
    static std::vector<int> _signo_array;
};

////////////////////////////////////////////////////////////////////////////////
/***
 * 使用示例：
 *
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // strsignal

// 用来指标信号线程是否退出
static bool g_to_exit = false;

static void* signal_thread(void* param); // 专门处理信号的线程
static void on_terminated();
static void on_child_end(pid_t child_pid, int child_exited_status);
static void on_signal_handler(int signo);

extern "C" int main()
{
    // 对SIG_PIPE，一般采取ignore策略
    CSignalHandler::ignore_signal(SIGPIPE);

    // 阻塞SIGINT、SIG_CHLD和SIG_TERM三个信号
    CSignalHandler::block_signal(SIGCHLD);
    CSignalHandler::block_signal(SIGINT);
    CSignalHandler::block_signal(SIGTERM);

    // 创建信号处理专用线程
    pthread_t thread;
    int errcode = pthread_create(&thread, NULL, signal_thread, NULL);
    if (errcode != 0)
    {
        fprintf(stderr, "create thread failed: %s\n", strerror(errno));
        exit(1);
    }

    // 创建子进程
    for (int i=0; i<2; ++i)
    {
        pid_t child_pid = fork();
        if (-1 == child_pid)
        {
            // 创建子进程失败
            fprintf(stderr, "fork error: %m\n");
            exit(1);
        }
        if (0 == child_pid)
        {
            // 子进程
            fprintf(stdout, "go into child process\n");
            sleep(8);
            exit(i+1);
        }
        else
        {
            fprintf(stdout, "%d's child PID is %d\n", getpid(), child_pid);
        }
    }

    // 等待线程退出
    pthread_join(thread, NULL);
    fprintf(stdout, "signal thread exited now\n");

    return 0;
}

// 专门处理信号的线程
void* signal_thread(void* param)
{
    while (!g_to_exit)
    {
        CSignalHandler::handle(on_terminated, on_child_end, on_signal_handler, NULL);
    }
}

void on_terminated()
{
    fprintf(stdout, "=> SIGTERM\n");

    fprintf(stdout, "%d tell signal thread to exit\n", getpid());
    g_to_exit = true;
}

void on_child_end(pid_t child_pid, int child_exited_status)
{
    fprintf(stdout, "=> SIGCHLD: %d:%d\n", child_pid, child_exited_status);
}

void on_signal_handler(int signo)
{
    fprintf(stdout, "=> %s\n", strsignal(signo));
}
*/

////////////////////////////////////////////////////////////////////////////////
// 以下是CSignalHandler的实现
// ，将它放在示例代码之后，是为了方便看到如何使用CSignalHandler。

sigset_t CSignalHandler::_sigset;
std::vector<int> CSignalHandler::_signo_array;

inline bool CSignalHandler::ignore_signal(int signo) throw ()
{
    return signal(signo, SIG_IGN) != SIG_ERR;
}

inline bool CSignalHandler::block_signal(int signo) throw ()
{
    if (_signo_array.empty())
    {
        // 初始化_sigset
        if (-1 == sigemptyset(&_sigset))
        {
            return false;
        }
    }

    if (-1 == sigaddset(&_sigset, signo))
    {
        return false;
    }
    else
    {
        errno = pthread_sigmask(SIG_BLOCK, &_sigset, NULL);
        if (errno != 0)
        {
            return false;
        }
        else
        {
            _signo_array.push_back(signo);
        }
    }
}

inline int CSignalHandler::wait_signal() throw ()
{
    int signo; // 发生的信号
    sigset_t sigset;

    // 初始化sigset
    if (-1 == sigemptyset(&sigset))
    {
        return -1;
    }

    // 设置sigset
    for (std::vector<int>::size_type i=0; i<_signo_array.size(); ++i)
    {
        if (-1 == sigaddset(&sigset, _signo_array[i]))
        {
            return -1;
        }
    }

    // 等待信号发生
    errno = sigwait(&sigset, &signo);
    if (errno != 0)
    {
        return -1;
    }

    return signo;
}

inline void CSignalHandler::handle(
        void (*on_terminated)(),
        void (*on_child_end)(pid_t child_pid, int child_exited_status),
        void (*on_signal_handler)(int signo),
        void (*on_exception)(int errcode)) throw ()
{
    int signo = wait_signal();
    if (-1 == signo)
    {
        on_exception(errno);
        return;
    }

    if (SIGTERM == signo)
    {
        // 进程自己收到SIGTERM的回调
        on_terminated();
    }
    if (signo != SIGCHLD)
    {
        // 非SIGTERM和SIGCHLD信号处理
        on_signal_handler(signo);
    }
    else
    {
        // 子进程退出信号处理
        // 这里需要循环，以免漏掉处理，SIGCHLD是不可靠信号
        while (true)
        {
            int child_exited_status;
            pid_t child_pid = waitpid(-1, &child_exited_status, WNOHANG);

            if (0 == child_pid)
            {
                break;
            }
            else if (child_pid > 0)
            {
                // 子进程结束回调
                on_child_end(child_pid, child_exited_status);
            }
            else
            {
                if (errno != ECHILD)
                {
                    // /usr/include/asm-generic/errno-base.h:
                    // #define   ECHILD          10      /* No child processes */
                    // wait error
                    on_exception(errno);
                }

                break;
            }
        }
    }
}

SYS_NAMESPACE_END
#endif // MOOON_SYS_SIGNAL_HANDLER_H
