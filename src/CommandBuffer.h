/*
 * Copyright 2017 WolkAbout Technology s.r.o.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COMMAND_BUFFER_H
#define COMMAND_BUFFER_H

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

namespace wolkabout
{
template <class Command> class CommandBuffer
{
public:
    CommandBuffer() = default;
    virtual ~CommandBuffer() = default;

    void pushCommand(std::shared_ptr<Command> command);

    std::shared_ptr<Command> popCommand();

    bool empty();

    void switchBuffers();

    void processCommands();

    void notify();

private:
    std::queue<std::shared_ptr<Command>> m_pushCommandQueue;
    std::queue<std::shared_ptr<Command>> m_popCommandQueue;

    std::mutex m_lock;
    std::condition_variable m_condition;
};

template <class Command> void CommandBuffer<Command>::pushCommand(std::shared_ptr<Command> command)
{
    std::unique_lock<std::mutex> unique_lock(m_lock);

    m_pushCommandQueue.push(command);

    m_condition.notify_one();
}

template <class Command> std::shared_ptr<Command> CommandBuffer<Command>::popCommand()
{
    if (m_popCommandQueue.empty())
    {
        return nullptr;
    }

    std::shared_ptr<Command> command = m_popCommandQueue.front();
    m_popCommandQueue.pop();
    return command;
}

template <class Command> void CommandBuffer<Command>::switchBuffers()
{
    std::unique_lock<std::mutex> unique_lock(m_lock);

    if (m_pushCommandQueue.empty())
    {
        m_condition.wait(unique_lock);
    }

    std::swap(m_pushCommandQueue, m_popCommandQueue);
}

template <class Command> bool CommandBuffer<Command>::empty()
{
    std::unique_lock<std::mutex> unique_lock(m_lock);

    return m_pushCommandQueue.empty();
}

template <class Command> void CommandBuffer<Command>::notify()
{
    std::unique_lock<std::mutex> unique_lock(m_lock);

    m_condition.notify_one();
}
}

#endif
