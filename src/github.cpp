#include "github.hpp"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <cctype>
#include <exception>
#include <utility>

namespace
{

std::string url_encode(const std::string& value)
{
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(value.size() * 3);
    for (unsigned char c : value)
    {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
        {
            out += static_cast<char>(c);
        }
        else
        {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0x0F];
        }
    }
    return out;
}

size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* body = static_cast<std::string*>(userdata);
    body->append(ptr, size * nmemb);
    return size * nmemb;
}

bool fetch_repos(const std::string& query, std::vector<repository>& out, std::string& err)
{
    CURL* curl = curl_easy_init();
    if (!curl)
    {
        err = "failed to initialize libcurl";
        return false;
    }

    const std::string url =
        "https://api.github.com/search/repositories?q=" + url_encode(query) + "&per_page=50";
    std::string body;
    const std::string user_agent = "repo-search/1.0";

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

    CURLcode rc = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK)
    {
        err = curl_easy_strerror(rc);
        return false;
    }

    try
    {
        auto json = nlohmann::json::parse(body);
        if (json.contains("items") && json["items"].is_array())
        {
            for (const auto& item : json["items"])
            {
                repository repo;
                repo.full_name = item.value("full_name", std::string());
                repo.stars = item.value("stargazers_count", 0LL);
                if (!repo.full_name.empty())
                {
                    out.push_back(std::move(repo));
                }
            }
            return true;
        }

        std::string message = json.value("message", "unexpected response from GitHub");
        if (http_code == 403 || http_code == 429)
        {
            message += " (rate limited, try again later)";
        }
        else if (http_code != 0)
        {
            message += " (HTTP " + std::to_string(http_code) + ")";
        }
        err = message;
        return false;
    }
    catch (const std::exception& e)
    {
        err = std::string("could not parse GitHub response: ") + e.what();
        return false;
    }
}

}  // namespace

void github_global_init()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void github_global_cleanup()
{
    curl_global_cleanup();
}

github_searcher::github_searcher() : worker(&github_searcher::run, this)
{
}

github_searcher::~github_searcher()
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        stop = true;
    }
    wake.notify_all();
    if (worker.joinable())
    {
        worker.join();
    }
}

void github_searcher::request(const std::string& query)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        pending = query;
        has_job = true;
        ++latest_gen;
        results.clear();
        error.clear();
        searched_query.clear();
        busy = true;
        ready = false;
    }
    wake.notify_one();
}

github_searcher::state_snapshot github_searcher::snapshot()
{
    std::lock_guard<std::mutex> lock(mutex);
    return state_snapshot{results, error, searched_query, busy, latest_gen};
}

bool github_searcher::take_ready()
{
    std::lock_guard<std::mutex> lock(mutex);
    bool was_ready = ready;
    ready = false;
    return was_ready;
}

void github_searcher::run()
{
    while (true)
    {
        std::string query;
        unsigned long long gen = 0;
        {
            std::unique_lock<std::mutex> lock(mutex);
            wake.wait(lock, [this]
            {
                return stop || has_job;
            });
            if (stop)
            {
                return;
            }
            query = pending;
            gen = latest_gen;
            has_job = false;
        }

        std::vector<repository> repos;
        std::string err;
        bool ok = fetch_repos(query, repos, err);

        std::lock_guard<std::mutex> lock(mutex);
        if (gen == latest_gen)
        {
            results = std::move(repos);
            error = std::move(err);
            searched_query = query;
            busy = false;
            ready = ok;
        }
    }
}
