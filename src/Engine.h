#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace spark {
constexpr int ppq = 960;
struct Note {
    int id = 0, start = 0, duration = 480, pitch = 72, velocity = 90, channel = 1;
    bool operator==(const Note &n) const {
        return id == n.id && start == n.start && duration == n.duration && pitch == n.pitch &&
               velocity == n.velocity && channel == n.channel;
    }
};
struct Chord {
    int start = 0, duration = 3840, root = 0;
    bool minor = false;
    std::vector<int> voicing;
};
struct Locks {
    bool pitch = false, timing = false, duration = false, velocity = false, sound = false;
};
struct Spec {
    int begin = 0, end = 15360, low = 72, high = 84, density = 1, complexity = 1;
    uint32_t seed = 42;
    Locks locks;
};
struct Candidate {
    std::vector<Note> notes;
    Spec spec;
    std::string name;
};
std::array<Candidate, 3> generate(const std::vector<Chord> &, const Spec &);
Candidate regenerate(const Candidate &, const std::vector<Chord> &, Spec);
std::vector<Chord> parseChords(const std::string &, int barTicks = 3840);
std::vector<uint8_t> midi(const std::vector<Note> &, double bpm, int numerator = 4,
                          int denominator = 4, int offset = 0);
std::string save(const std::array<Candidate, 3> &, const std::vector<Chord> &, int selected,
                 double bpm, int patch);
void restore(const std::string &, std::array<Candidate, 3> &, std::vector<Chord> &, int &, double &,
             int &);
} // namespace spark
