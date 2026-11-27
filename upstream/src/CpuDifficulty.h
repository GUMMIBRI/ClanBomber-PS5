#pragma once
// Persist the old Normal=0 and Easy=1 values without changing their behavior.
namespace cpu_difficulty {
constexpr int normalize(int value) { return value >= 0 && value <= 2 ? value : 0; }
constexpr int from_menu(int index) { return 2 - normalize(index); }
constexpr int to_menu(int value) { return 2 - normalize(value); }
constexpr int speed_penalty(int value) { return value == 2 ? 45 : (value == 1 ? 15 : 0); }
constexpr float bomb_delay(int value) { return value == 2 ? 18.0f : (value == 1 ? 5.0f : 0.0f); }
constexpr int enemy_attack_value(int value, int original) { return value == 2 ? 0 : original; }
}
