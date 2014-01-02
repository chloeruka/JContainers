#pragma once

#include "collections.h"

#include <thread>
#include <mutex>
#include <chrono>
#include <algorithm>

namespace collections {
    using namespace std;

    class autorelease_queue {

        std::thread _thread;
        bshared_mutex& _mutex;

        typedef lock_guard<decltype(_mutex)> lock;
        typedef unsigned int time_point;
        //typedef chrono::time_point<chrono::system_clock> time_point;
        typedef std::vector<std::pair<HandleT, time_point> > queue;
        queue _queue;
        time_point _timeNow;
        bool _run;

    public:

        void _clear() {
            _queue.clear();
        }

        template<class Archive>
        void serialize(Archive & ar, const unsigned int version) {
            ar & _timeNow;
            ar & _queue;
        }

        enum {
            obj_lifetime = 10, // seconds
            sleep_duration = 2, // seconds
        };

        autorelease_queue(bshared_mutex &mt) 
            : _thread()
            , _timeNow(0)
            , _run(false)
            , _mutex(mt)
        {
            start();
        }

        void push(HandleT handle) {
            write_lock g(_mutex);
            _queue.push_back(std::make_pair(handle, _timeNow));
        }

        void start() {
            write_lock g(_mutex);
            if (!_run) {
                _run = true;
                _thread = std::thread(&autorelease_queue::run, std::ref(*this));
            }
        }

        size_t count() {
            read_lock hg(_mutex);
            return _queue.size();
        }

        void stop() {
            {
                write_lock g(_mutex);
                _run = false;
            }

            //assert(_thread.joinable());
            if (_thread.joinable()) {
                _thread.join();
            }
        }

        ~autorelease_queue() {
            stop();
        }

        static autorelease_queue& instance();

    private:
        void cycleIncr() {
            if (_timeNow < (std::numeric_limits<time_point>::max)()) {
                ++_timeNow;
            } else {
                _timeNow = 0;
            }
        }

        time_point lifetimeDiff(time_point time) const {
            if (_timeNow >= time) {
                return _timeNow - time;
            } else {
                return ((std::numeric_limits<time_point>::max)() - time) + _timeNow;
            }
        }

        enum {
            obj_lifeInTicks = obj_lifetime / sleep_duration, // ticks
        };

        static void run(autorelease_queue &self) {
            chrono::seconds sleepTime(sleep_duration);
            vector<HandleT> toRelease;

            while (true) {
                {
                    write_lock g(self._mutex);

                    if (!self._run) {
                        break;
                    }

                    self._queue.erase(
                        remove_if(self._queue.begin(), self._queue.end(), [&](const queue::value_type& val) {
                            auto diff = self.lifetimeDiff(val.second);
                            bool release = diff >= obj_lifeInTicks;

                            if (release)
                                toRelease.push_back(val.first);
                            return release;
                        }),
                            self._queue.end());

                    self.cycleIncr();
                }

                for(auto val : toRelease) {
                    printf("handle %u released\n", val);
                    auto obj = collection_registry::getObject(val);
                    if (obj)
                        obj->release();
                }
                toRelease.clear();

                printf("autorelease_queue alive\n");
                std::this_thread::sleep_for(sleepTime);
            }

            printf("autorelease_queue finish\n");
        }
    };


    inline object_base * object_base::autorelease() {
        autorelease_queue::instance().push(id);
        return this;
    }
}
