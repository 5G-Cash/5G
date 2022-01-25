/**
* @file       ParallelTasks.h
*
* @brief      ParallelTasks class for the Zerocoin library.
*
* @author     Peter Shugalev
* @date       Nov 2017
*
* @copyright  Copyright 2017 Peter Shugalev
* @license    This project is released under the MIT license.
**/

#ifndef PARALLELTASKS_H
#define PARALLELTASKS_H

/**
 * Implementation of thread pool for parallelizing spend creation and verification
 */ 

#include <vector>
#include <functional>

#define BOOST_THREAD_PROVIDES_FUTURE


#include <boost/thread/future.hpp>
#include <boost/thread.hpp>

namespace libzerocoin {

class ParallelTasks {
private:
    vector<boost::future<void>> tasks;

public:
    ParallelTasks(int n=0);

    // add new task
    void Add(std::function<void()> task);

    // wait for everything added so far
    void Wait();

    // clear all the tasks from the waiting list
    void Reset();

    // helper class to put thread interruption on pause
    class DoNotDisturb {
    private:
        boost::this_thread::disable_interruption dnd;
    public:
        DoNotDisturb() {}
    };
};

}

#endif // PARALLELTASKS_H
