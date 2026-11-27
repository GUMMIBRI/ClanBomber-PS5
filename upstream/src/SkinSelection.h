#pragma once
namespace skin_selection {
struct Choice { int skin; bool random; };
constexpr bool legacy_random(int saved_skin, int slot) { return saved_skin == slot; }
constexpr Choice advance(int skin, bool random, bool cpu, int count) {
    return cpu && random ? Choice{0, false} :
           cpu && skin == count - 1 ? Choice{skin, true} :
           Choice{(skin + 1) % count, false};
}
constexpr int resolve(int skin, bool random, bool cpu, unsigned long draw, int count) {
    return cpu && random ? static_cast<int>(draw % count) : skin;
}
}
