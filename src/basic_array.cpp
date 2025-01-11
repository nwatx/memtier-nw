#include <vector>
#include <cstdlib>

int main() {
    int v_size = static_cast<int>(1e9);
    std::vector<int> v(v_size);
    for (int i = 0; i < v_size; ++i) {
        v[i] = std::rand();
    }
    return 0;
}
