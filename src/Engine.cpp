#include "Engine.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <sstream>
#include <stdexcept>
namespace spark {
std::vector<Chord> parseChords(const std::string &text, int barTicks) {
    std::istringstream in(text);
    std::string token;
    std::vector<Chord> result;
    while (in >> token) {
        if (token.empty())
            continue;
        auto pos = std::string("C D EF G A B").find(token[0]);
        if (pos == std::string::npos)
            throw std::runtime_error(
                "Use chord names such as Am F C G; major and minor triads are supported.");
        int root = (int)pos;
        size_t i = 1;
        if (i < token.size() && (token[i] == '#' || token[i] == 'b'))
            root += (token[i++] == '#' ? 1 : -1);
        bool minor = i < token.size() && token[i] == 'm';
        if (minor)
            ++i;
        if (i != token.size())
            throw std::runtime_error("This prototype supports major and minor triads only.");
        root = (root + 12) % 12;
        result.push_back({(int)result.size() * barTicks,
                          barTicks,
                          root,
                          minor,
                          {48 + root, 48 + root + (minor ? 3 : 4), 55 + root}});
        if (result.size() > 64)
            throw std::runtime_error("Maximum 64 chords.");
    }
    if (result.empty())
        throw std::runtime_error("Enter at least one chord.");
    return result;
}
std::array<Candidate, 3> generate(const std::vector<Chord> &chords, const Spec &s) {
    if (chords.empty() || s.low < 0 || s.high > 127 || s.low > s.high || s.begin < 0 ||
        s.end <= s.begin || s.end - s.begin > ppq * 256)
        throw std::runtime_error("Invalid harmony, region, or pitch range.");
    std::array<Candidate, 3> out;
    const char *names[] = {"Repeating motif", "More space", "Alternate contour"};
    for (int k = 0; k < 3; ++k) {
        auto &c = out[k];
        c.spec = s;
        c.name = names[k];
        std::mt19937 rng(s.seed + k * 7919);
        int count = std::max(1, (s.end - s.begin) * (6 + std::clamp(s.density, 0, 4) * 8) / 15360);
        if (k == 1)
            count = std::max(1, count * 2 / 3);
        int grid = s.complexity > 1 ? 240 : 480;
        int last = -1;
        for (int i = 0; i < count; ++i) {
            int t = s.begin + ((int64_t)i * (s.end - s.begin) / count / grid) * grid;
            if (t == last)
                continue;
            last = t;
            const Chord *ch = &chords.front();
            for (auto &h : chords)
                if (h.start <= t)
                    ch = &h;
            std::vector<int> pitches;
            for (int p = s.low; p <= s.high; ++p) {
                int pc = (p - ch->root + 120) % 12;
                if (pc == 0 || pc == (ch->minor ? 3 : 4) || pc == 7)
                    pitches.push_back(p);
            }
            if (pitches.empty())
                throw std::runtime_error("No chord tones in the selected range; widen the range.");
            int ix = k == 2 ? (int)pitches.size() - 1 - (i % (int)pitches.size())
                            : ((i % 4) + (int)(rng() % 2)) % (int)pitches.size();
            c.notes.push_back({i + 1, t, std::min(grid + (int)(rng() % 2) * grid, s.end - t),
                               pitches[ix], 80 + (int)(rng() % 24), 1});
        }
    }
    return out;
}
Candidate regenerate(const Candidate &old, const std::vector<Chord> &harmony, Spec s) {
    auto fresh = generate(harmony, s)[0];
    Candidate out = old;
    if (s.begin < old.spec.begin || s.end > old.spec.end)
        throw std::runtime_error("Regeneration region extends beyond the phrase.");
    out.spec.seed = s.seed;
    out.spec.locks = s.locks;
    std::vector<size_t> eligible;
    for (size_t i = 0; i < old.notes.size(); ++i)
        if (old.notes[i].start >= s.begin && old.notes[i].start < s.end)
            eligible.push_back(i);
    bool locked = s.locks.pitch || s.locks.timing || s.locks.duration || s.locks.velocity;
    if (locked) {
        for (size_t j = 0; j < eligible.size(); ++j) {
            auto &n = out.notes[eligible[j]];
            auto replacement = fresh.notes[j % fresh.notes.size()];
            if (!s.locks.pitch)
                n.pitch = replacement.pitch;
            if (!s.locks.timing)
                n.start = s.locks.duration ? std::min(replacement.start, old.spec.end - n.duration)
                                           : replacement.start;
            if (!s.locks.duration)
                n.duration = std::min(replacement.duration, s.end - n.start);
            if (!s.locks.velocity)
                n.velocity = replacement.velocity;
        }
    } else {
        out.notes.erase(
            std::remove_if(out.notes.begin(), out.notes.end(),
                           [&](auto &n) { return n.start >= s.begin && n.start < s.end; }),
            out.notes.end());
        int id = 1;
        for (auto &n : out.notes)
            id = std::max(id, n.id + 1);
        for (auto n : fresh.notes) {
            n.id = id++;
            out.notes.push_back(n);
        }
    }
    std::stable_sort(out.notes.begin(), out.notes.end(),
                     [](auto &a, auto &b) { return a.start < b.start; });
    return out;
}
static void be(std::vector<uint8_t> &b, uint32_t n, int bytes) {
    for (int i = bytes - 1; i >= 0; --i)
        b.push_back((n >> (8 * i)) & 255);
}
static void vl(std::vector<uint8_t> &b, uint32_t v) {
    uint8_t buf[5];
    int n = 0;
    buf[n++] = v & 127;
    while ((v >>= 7))
        buf[n++] = (v & 127) | 128;
    while (n)
        b.push_back(buf[--n]);
}
std::vector<uint8_t> midi(const std::vector<Note> &notes, double bpm, int num, int den,
                          int offset) {
    if (!std::isfinite(bpm) || bpm < 20 || bpm > 300 || num < 1 || num > 32 ||
        (den != 2 && den != 4 && den != 8 && den != 16))
        throw std::runtime_error("Invalid tempo or meter.");
    struct Event {
        int t;
        std::vector<uint8_t> b;
    };
    std::vector<Event> events;
    for (auto &n : notes) {
        if (n.start < offset || n.duration <= 0 || n.pitch < 0 || n.pitch > 127 || n.velocity < 1 ||
            n.velocity > 127 || n.channel < 1 || n.channel > 16)
            throw std::runtime_error("Invalid note for MIDI export.");
        events.push_back(
            {n.start - offset,
             {(uint8_t)(0x90 + n.channel - 1), (uint8_t)n.pitch, (uint8_t)n.velocity}});
        events.push_back({n.start - offset + n.duration,
                          {(uint8_t)(0x80 + n.channel - 1), (uint8_t)n.pitch, 0}});
    }
    std::stable_sort(events.begin(), events.end(),
                     [](auto &a, auto &b) { return a.t != b.t ? a.t < b.t : a.b[0] < b.b[0]; });
    std::vector<uint8_t> tr = {0, 255, 81, 3};
    be(tr, (uint32_t)std::lround(60000000 / bpm), 3);
    tr.insert(tr.end(), {0, 255, 88, 4, (uint8_t)num,
                         (uint8_t)(den == 2   ? 1
                                   : den == 4 ? 2
                                   : den == 8 ? 3
                                              : 4),
                         24, 8});
    int prev = 0;
    for (auto &e : events) {
        vl(tr, e.t - prev);
        tr.insert(tr.end(), e.b.begin(), e.b.end());
        prev = e.t;
    }
    tr.insert(tr.end(), {0, 255, 47, 0});
    std::vector<uint8_t> b = {'M', 'T', 'h', 'd', 0,   0,   0,   6,   0,
                              0,   0,   1,   3,   192, 'M', 'T', 'r', 'k'};
    be(b, (uint32_t)tr.size(), 4);
    b.insert(b.end(), tr.begin(), tr.end());
    return b;
}
std::string save(const std::array<Candidate, 3> &cs, const std::vector<Chord> &h, int selected,
                 double bpm, int patch) {
    std::ostringstream o;
    o << "SPARK 1 " << selected << ' ' << bpm << ' ' << patch << ' ' << h.size() << '\n';
    for (auto &c : h) {
        o << c.start << ' ' << c.duration << ' ' << c.root << ' ' << c.minor << ' '
          << c.voicing.size();
        for (int p : c.voicing)
            o << ' ' << p;
        o << '\n';
    }
    for (auto &c : cs) {
        auto &s = c.spec;
        o << s.begin << ' ' << s.end << ' ' << s.low << ' ' << s.high << ' ' << s.density << ' '
          << s.complexity << ' ' << s.seed << ' ' << s.locks.pitch << ' ' << s.locks.timing << ' '
          << s.locks.duration << ' ' << s.locks.velocity << ' ' << s.locks.sound << ' '
          << c.notes.size() << '\n';
        for (auto &n : c.notes)
            o << n.id << ' ' << n.start << ' ' << n.duration << ' ' << n.pitch << ' ' << n.velocity
              << ' ' << n.channel << '\n';
    }
    return o.str();
}
void restore(const std::string &str, std::array<Candidate, 3> &cs, std::vector<Chord> &h,
             int &selected, double &bpm, int &patch) {
    std::istringstream in(str);
    std::string magic;
    int v, sel, pat;
    double tempo;
    size_t count;
    std::array<Candidate, 3> temp;
    std::vector<Chord> harmony;
    if (!(in >> magic >> v >> sel >> tempo >> pat >> count) || magic != "SPARK" || v != 1 ||
        sel < 0 || sel > 2 || tempo < 20 || tempo > 300 || pat < 0 || pat > 3 || count > 64)
        throw std::runtime_error("Unsupported or corrupt SPARK session.");
    for (size_t i = 0; i < count; ++i) {
        Chord c;
        size_t n;
        if (!(in >> c.start >> c.duration >> c.root >> c.minor >> n) || n > 128 || c.start < 0 ||
            c.duration < 1 || c.start > ppq * 256 || c.duration > ppq * 256 - c.start ||
            c.root < 0 || c.root > 11)
            throw std::runtime_error("Invalid chord state.");
        for (size_t j = 0; j < n; ++j) {
            int p;
            if (!(in >> p) || p < 0 || p > 127)
                throw std::runtime_error("Invalid voicing.");
            c.voicing.push_back(p);
        }
        harmony.push_back(c);
    }
    for (int k = 0; k < 3; ++k) {
        auto &c = temp[k];
        auto &s = c.spec;
        if (!(in >> s.begin >> s.end >> s.low >> s.high >> s.density >> s.complexity >> s.seed >>
              s.locks.pitch >> s.locks.timing >> s.locks.duration >> s.locks.velocity >>
              s.locks.sound >> count) ||
            count > 10000 || s.begin < 0 || s.end <= s.begin || s.end > ppq * 256 || s.low < 0 ||
            s.high > 127 || s.low > s.high)
            throw std::runtime_error("Invalid candidate state.");
        for (size_t j = 0; j < count; ++j) {
            Note n;
            if (!(in >> n.id >> n.start >> n.duration >> n.pitch >> n.velocity >> n.channel) ||
                n.start < 0 || n.duration < 1 || n.start >= s.end || n.duration > s.end - n.start ||
                n.pitch < 0 || n.pitch > 127 || n.velocity < 1 || n.velocity > 127 ||
                n.channel < 1 || n.channel > 16)
                throw std::runtime_error("Invalid note state.");
            c.notes.push_back(n);
        }
        c.name = k == 0 ? "Repeating motif" : k == 1 ? "More space" : "Alternate contour";
    }
    cs = temp;
    h = harmony;
    selected = sel;
    bpm = tempo;
    patch = pat;
}
} // namespace spark
