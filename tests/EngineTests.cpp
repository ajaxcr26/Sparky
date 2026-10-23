#include "Engine.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace spark;
static int checks = 0;
void check(bool ok, const char *name) {
    ++checks;
    if (!ok)
        throw std::runtime_error(name);
}
int main() {
    try {
        auto h = parseChords("Am F C G Am F C G");
        Spec s;
        s.end = 30720;
        auto cs = generate(h, s);
        check(cs[0].notes == generate(h, s)[0].notes, "determinism");
        check(cs[1].notes.size() < cs[0].notes.size(), "space candidate");
        for (auto &c : cs)
            for (auto &n : c.notes) {
                check(n.pitch >= 72 && n.pitch <= 84, "range");
                check(n.start >= 0 && n.start + n.duration <= s.end, "region");
                auto &ch = h[n.start / 3840];
                int pc = (n.pitch - ch.root + 120) % 12;
                check(pc == 0 || pc == (ch.minor ? 3 : 4) || pc == 7, "harmony");
            }
        Spec region = s;
        region.begin = 26880;
        region.seed = 7;
        auto ending = regenerate(cs[0], h, region);
        for (auto &n : cs[0].notes)
            if (n.start < region.begin)
                check(std::find(ending.notes.begin(), ending.notes.end(), n) != ending.notes.end(),
                      "opening preserved");
        region.locks = {true, true, true, true, true};
        check(regenerate(cs[0], h, region).notes == cs[0].notes, "all note locks");
        for (int lock = 0; lock < 4; ++lock) {
            Spec x = s;
            x.seed = 900;
            x.locks = {lock == 0, lock == 1, lock == 2, lock == 3, false};
            auto r = regenerate(cs[0], h, x);
            for (auto &n : cs[0].notes) {
                auto it = std::find_if(r.notes.begin(), r.notes.end(),
                                       [&](auto &a) { return a.id == n.id; });
                check(it != r.notes.end(), "locked identity");
                check(lock == 0   ? it->pitch == n.pitch
                      : lock == 1 ? it->start == n.start
                      : lock == 2 ? it->duration == n.duration
                                  : it->velocity == n.velocity,
                      "individual lock");
            }
        }
        auto bytes = midi(cs[0].notes, 150);
        check(bytes.size() > 30 && bytes[12] == 3 && bytes[13] == 192, "MIDI ppq");
        auto str = save(cs, h, 2, 150, 1);
        std::array<Candidate, 3> restored;
        std::vector<Chord> hh;
        int sel = 0, patch = 0;
        double bpm = 120;
        restore(str, restored, hh, sel, bpm, patch);
        check(save(restored, hh, sel, bpm, patch) == str, "exact state round trip");
        for (auto bad : {"", "SPARK 99", "SPARK 1 -1 150 0 0"}) {
            bool thrown = false;
            try {
                restore(bad, restored, hh, sel, bpm, patch);
            } catch (...) {
                thrown = true;
            }
            check(thrown, "invalid state rejected");
        }
        bool rejected = false;
        try {
            parseChords("Cmaj7");
        } catch (...) {
            rejected = true;
        }
        check(rejected, "unsupported chord explicit");
        std::cout << checks << " checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
