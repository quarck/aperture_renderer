#pragma once
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>

#include <thread>
#include <vector>
#include <list>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <atomic>
#include <functional>
#include <iostream>



class ThreadGrid
{
    int numThreads;

    std::vector<std::thread> threads;
    std::vector<std::mutex> threadIsActive;

    std::atomic_bool terminate{ false };

    std::function<void(int, int)> task;
    std::mutex taskLock;
    std::vector<bool> hasTask;
    int numActiveThreads;
    std::condition_variable taskAwailableCond;
    std::condition_variable taskDoneCond;

public:
    ThreadGrid(int n)
        : numThreads(n)
        , threads(n)
        , threadIsActive(n)
        , hasTask(n)
		, numActiveThreads{0}
    {
        for (int i = 0; i < n; ++i)
        {
            threads[i] = std::thread(&ThreadGrid::Thread, this, i);
        }
    }

    ~ThreadGrid()
    {
        terminate = true;
        {
            std::lock_guard<std::mutex> m(taskLock);
            taskAwailableCond.notify_all();
        }

        for (auto& thread : threads)
        {
            if (thread.joinable())
                thread.join();
        }
    }

    void GridRun(std::function<void(int, int)>&& item) noexcept
    {
		try 
		{
			std::unique_lock<std::mutex> m(taskLock);

			std::fill(std::begin(hasTask), std::end(hasTask), true);
			numActiveThreads = numThreads;

			task = std::move(item);

			// this will wake waiting threads, but only when we unlcok the taskLock -
			// i.e. when we do wait ourselves below
			taskAwailableCond.notify_all();

			// Wait for the theads to finish
			taskDoneCond.wait(m, [&] {return numActiveThreads == 0; });

			// Finally - ensure we clean up the task closure
			task = std::function<void(int, int)>();
		}
		catch (...)
		{
			std::cerr << "unhandled exception in GridRun" << std::endl;
			std::terminate();
		}
    }

private:
    void Thread(int threadIdx)
    {
        //_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
        //_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

        while (!terminate)
        {
            std::function<void(int, int)> item;

            {
                std::unique_lock<std::mutex> m(taskLock);
                taskAwailableCond.wait(m, [&] {return hasTask[threadIdx] || terminate; });
                if (!hasTask[threadIdx])
                    continue;
                item = task;
            }

            // we have the task - run it
            item(threadIdx, numThreads);

            // Mark ourselves as done, and if we are the last thread - notify the waitinig "GridRun"
            std::unique_lock<std::mutex> m(taskLock);
            hasTask[threadIdx] = false;
            if (--numActiveThreads == 0)
                taskDoneCond.notify_all();
        }
    }
};