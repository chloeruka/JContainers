#pragma once

namespace collections {

    class object_registry;

    class autorelease_queue {

        std::thread _thread;
        bshared_mutex& _mutex;

        object_registry& _registry;

        typedef std::lock_guard<decltype(_mutex)> lock;
        typedef unsigned int time_point;
        //typedef chrono::time_point<chrono::system_clock> time_point;
        typedef std::vector<std::pair<HandleT, time_point> > queue;
        queue _queue;
        time_point _timeNow;
        bool _run;
        bool _paused;

    public:

        void u_clear() {
            _queue.clear();
        }

        template<class Archive>
        void serialize(Archive & ar, const unsigned int version) {
            ar & _timeNow;
            ar & _queue;
        }

        explicit autorelease_queue(object_registry& registry, bshared_mutex &mt) 
            : _thread()
            , _timeNow(0)
            , _run(false)
            , _paused(false)
            , _mutex(mt)
            , _registry(registry)
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
                _paused = false;
                _thread = std::thread(&autorelease_queue::run, std::ref(*this));
            }
        }

        size_t count() {
            read_lock hg(_mutex);
            return _queue.size();
        }

        void setPaused(bool paused) {
            write_lock g(_mutex);
            _paused = paused;
        }

        void stop() {
            {
                write_lock g(_mutex);
                _run = false;
                _paused = false;
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

    public:

        enum {
            obj_lifetime = 10, // seconds
            sleep_duration = 2, // seconds
        };

        enum {
            obj_lifeInTicks = obj_lifetime / sleep_duration, // ticks
        };

    private:

        static void run(autorelease_queue &self) {
            using namespace std;

            chrono::seconds sleepTime(sleep_duration);
            vector<HandleT> toRelease;

            while (true) {
                {
                    write_lock g(self._mutex);

                    if (!self._run) {
                        break;
                    }

                    if (!self._paused) {
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
                }

                for(auto& val : toRelease) {
                    auto obj = self._registry.getObject(val);
                    if (obj) {
                        bool deleted = obj->_deleteOrRelease(nullptr);
                        printf("handle %u %s\n", val, (deleted ? "deleted" : "released"));
                    }
                }
                toRelease.clear();

                printf("autorelease_queue time - %u\n", self._timeNow);
                std::this_thread::sleep_for(sleepTime);
            }

            printf("autorelease_queue finish\n");
        }
    };


}

//BOOST_CLASS_EXPORT_GUID (collections::autorelease_queue, "kJAutoreleaseQueue")
