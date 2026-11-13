#pragma once

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct repository
{
    std::string full_name;
    long long stars = 0;
};

void github_global_init();
void github_global_cleanup();

class github_searcher
{
public:
    struct state_snapshot
    {
        std::vector<repository> results;
        std::string error;
        std::string searched_query;
        bool busy = false;
        unsigned long long gen = 0;
    };

    github_searcher();
    ~github_searcher();

    github_searcher(const github_searcher&) = delete;
    github_searcher& operator=(const github_searcher&) = delete;

    void request(const std::string& query);
    state_snapshot snapshot();
    bool take_ready();

private:
    void run();

    std::thread worker;
    std::mutex mutex;
    std::condition_variable wake;
    bool stop = false;
    bool has_job = false;
    std::string pending;
    unsigned long long latest_gen = 0;
    std::vector<repository> results;
    std::string error;
    std::string searched_query;
    bool busy = false;
    bool ready = false;
};
