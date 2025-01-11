#include <vector>

int main() {
    const size_t v_size = static_cast<int>(1e8);
    std::vector<int> v(v_size);
    for (int i = 0; i < v_size; ++i) {
        v[i] = std::rand();
    }
    return 0;
}
