#include <iostream>
#include <climits>
#include <unordered_map>

using namespace std;

class Redis {
    Redis() {

    }
    static unordered_map<string, string> umap;
};

int main() {
    Redis redis = new Redis();

    return 0;
}