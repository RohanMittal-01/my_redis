#include <iostream>
#include <unordered_map>
#include <string>
#include <ctime>
#include <chrono>
#include <thread>

using namespace std;

class Redis {
public:
    string get(string key) {
        std::time_t currentTime = std::time(nullptr);
        if (values.find(key) != values.end()) {
            if (expiry[key] == 0 || expiry[key] > currentTime) {
                return values[key];
            } else {
                values.erase(key);
                expiry.erase(key);
                return "";
            }
        }
        return "";
    }

    string set(string key, string value, long ttl) {
        values[key] = value;
        if (ttl > 0) {
            expiry[key] = std::time(nullptr) + ttl;
        } else {
            expiry[key] = 0;
        }
        return "OK";
    }

private:
    static unordered_map<string, string> values;
    static unordered_map<string, long> expiry;
};

unordered_map<string, string> Redis::values;
unordered_map<string, long> Redis::expiry;

int main() {
    Redis redis;
    cout << redis.set("foo", "bar", 3) << endl;
    cout << "Immediately: " << redis.get("foo") << endl;

    std::this_thread::sleep_for(std::chrono::seconds(4));

    cout << "After 4 seconds: " << redis.get("foo") << endl;

    return 0;
}