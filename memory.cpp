#include <crow/app.h>
#include <crow/json.h>

#include <unordered_map>
#include <chrono>
#include <shared_mutex>
#include <thread>
#include <atomic>
#include <iostream>

class Redis {
public:
    Redis() {
        values.reserve(10000);
        expiry.reserve(10000);
        stop_cleanup = false;
        cleanup_thread = std::thread([this]() { this->cleanupExpiredKeys(); });
    }

    ~Redis() {
        stop_cleanup = true;
        if (cleanup_thread.joinable()) cleanup_thread.join();
    }

    std::string get(const std::string& key) {
        using Clock = std::chrono::steady_clock;
        auto now = Clock::now();

        {
            std::shared_lock lock_values(mtx_values);
            auto it_val = values.find(key);
            if (it_val == values.end()) return "";
            // Check expiry
            std::shared_lock lock_expiry(mtx_expiry);
            auto it_exp = expiry.find(key);
            if (it_exp == expiry.end() || it_exp->second == Clock::time_point{} || it_exp->second > now) {
                return it_val->second;
            }
        }
        // expired: erase outside shared lock
        {
            std::unique_lock lock_values(mtx_values);
            std::unique_lock lock_expiry(mtx_expiry);
            values.erase(key);
            expiry.erase(key);
        }
        return "";
    }

    std::string set(const std::string& key, const std::string& value, long ttl_seconds) {
        using Clock = std::chrono::steady_clock;
        auto expiry_time = (ttl_seconds > 0) ? Clock::now() + std::chrono::seconds(ttl_seconds) : Clock::time_point{};

        {
            std::unique_lock lock_values(mtx_values);
            values[key] = value;
        }
        {
            std::unique_lock lock_expiry(mtx_expiry);
            expiry[key] = expiry_time;
        }

        return "OK";
    }

private:
    std::unordered_map<std::string, std::string> values;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> expiry;
    std::shared_mutex mtx_values;
    std::shared_mutex mtx_expiry;

    std::atomic<bool> stop_cleanup;
    std::thread cleanup_thread;

    void cleanupExpiredKeys() {
        using Clock = std::chrono::steady_clock;
        while (!stop_cleanup.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(30)); // less frequent cleanup

            auto now = Clock::now();

            std::unique_lock lock_values(mtx_values);
            std::unique_lock lock_expiry(mtx_expiry);

            for (auto it = expiry.begin(); it != expiry.end(); ) {
                if (it->second != Clock::time_point{} && it->second <= now) {
                    values.erase(it->first);
                    it = expiry.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
};


int main() {
    Redis redis;
    crow::SimpleApp app;

    // GET endpoint: /get?key=foo
    CROW_ROUTE(app, "/get")
    .methods("GET"_method)
    ([&redis](const crow::request& req) {
        auto key = req.url_params.get("key");
        if (!key) return crow::response(400, "Missing key parameter");
        std::string value = redis.get(key);
        if (value.empty()) return crow::response(404, "Key not found or expired");
        return crow::response(200, value);
    });

    // POST endpoint: /set (JSON body: { "key": "...", "value": "...", "ttl": 5 })
    CROW_ROUTE(app, "/set")
    .methods("POST"_method)
    ([&redis](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("key") || !body.has("value")) {
            return crow::response(400, "Invalid JSON body");
        }
        std::string key = body["key"].s();
        std::string value = body["value"].s();
        long ttl = body.has("ttl") ? body["ttl"].i() : 0;
        redis.set(key, value, ttl);
        return crow::response(200, "OK");
    });

    app.port(18080).multithreaded().run();
}
