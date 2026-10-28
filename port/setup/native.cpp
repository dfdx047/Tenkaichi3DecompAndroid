// The setup's own installer, for a release folder (Tenkaichi3Decomp, Tenkaichi3Decomp.dat and Tenkaichi3Decomp-setup together, no build tools):
// reads the user's disc image directly, checks it, unpacks the game data and runs the self-test. No Python, no 7z.
// It reports through the same event lines as install.py (@step, @note, @ok, @skip, @fail, @stopped, @done), so the
// window (setup.cpp) shows both the same way.
//
//   disc image   ISO 9660: the volume descriptor at sector 16, directory records from the root
//   checks       SHA-1 of BIN/DBZP.BIN and of SLUS_216.78 laid out as a flat image from 0x100000 (the values of
//                config/DBZP.yaml and config/SLUS_216.78.yaml)
//   game data    gamedata/disc/<loose files>, and every DATA/*.AFS archive split into gamedata/<name>/00000.bin ...
//                (the layout of port/tools/extract_disc.py)
//   European     optional, a second image (SLES-54945): 410 entries of DATA/PZS3EU1.AFS for translated menus/HUD,
//                plus entry 1 of DATA/PZS3EU0.AFS for the native language selector and ZS3EUOP.ADX for the
//                EU opening soundtrack (as port/tools/extract_eu.py).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <SDL3/SDL.h>
#include "native.h"

namespace {

const char *kRomSha1 = "caee6c2269bba89fc51eea9e7beac3adbec9dc52";
const char *kDbzpSha1 = "4f910969e05d9b25c83af7642b60da9949a4348b";
const Uint32 kRomBase = 0x100000, kRomEnd = 0x2FF180;
#ifdef _WIN32
const char *kGame = "Tenkaichi3Decomp.exe";
#else
const char *kGame = "Tenkaichi3Decomp";
#endif

std::mutex sLock;
std::vector<std::string> sLines;
std::thread sThread;
std::atomic<bool> sRunning(false), sCancel(false);

void say(const std::string &line) {
    std::lock_guard<std::mutex> lock(sLock);
    sLines.push_back(line);
}

struct Stop { std::string why; };

// ------------------------------------------------------------------------------------------------------- SHA-1
struct Sha1 {
    Uint32 h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    Uint8 block[64];
    Uint64 total = 0;
    size_t fill = 0;

    static Uint32 rol(Uint32 v, int n) { return (v << n) | (v >> (32 - n)); }
    void compress() {
        Uint32 w[80], a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 16; i++) {
            w[i] = (Uint32)block[i * 4] << 24 | (Uint32)block[i * 4 + 1] << 16 | (Uint32)block[i * 4 + 2] << 8 | block[i * 4 + 3];
        }
        for (int i = 16; i < 80; i++) {
            w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }
        for (int i = 0; i < 80; i++) {
            Uint32 f = i < 20 ? (b & c) | (~b & d) : i < 40 ? b ^ c ^ d : i < 60 ? (b & c) | (b & d) | (c & d) : b ^ c ^ d;
            Uint32 k = i < 20 ? 0x5A827999 : i < 40 ? 0x6ED9EBA1 : i < 60 ? 0x8F1BBCDC : 0xCA62C1D6;
            Uint32 t = rol(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rol(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    void add(const Uint8 *p, size_t n) {
        total += n;
        while (n > 0) {
            size_t take = 64 - fill < n ? 64 - fill : n;
            memcpy(block + fill, p, take);
            fill += take; p += take; n -= take;
            if (fill == 64) {
                compress();
                fill = 0;
            }
        }
    }
    std::string hex() {
        Uint64 bits = total * 8;
        Uint8 pad = 0x80, zero = 0, len[8];
        char out[41];
        add(&pad, 1);
        while (fill != 56) {
            add(&zero, 1);
        }
        for (int i = 0; i < 8; i++) {
            len[i] = (Uint8)(bits >> (56 - i * 8));
        }
        add(len, 8);
        for (int i = 0; i < 5; i++) {
            snprintf(out + i * 8, 9, "%08x", h[i]);
        }
        return out;
    }
};

// ------------------------------------------------------------------------------------------------- the disc image
struct IsoFile { std::string path; Uint64 offset, size; };

struct Iso {
    SDL_IOStream *io = NULL;
    std::vector<IsoFile> files;

    void read(Uint64 offset, void *buf, size_t n) {
        if (SDL_SeekIO(io, (Sint64)offset, SDL_IO_SEEK_SET) < 0 || SDL_ReadIO(io, buf, n) != n) {
            throw Stop{"The disc image could not be read to its end. It may be incomplete."};
        }
    }
    void walk(Uint32 lba, Uint32 size, const std::string &dir, int depth) {
        std::vector<Uint8> d(size);
        read((Uint64)lba * 2048, d.data(), size);
        for (size_t pos = 0; pos + 34 <= size;) {
            Uint8 len = d[pos];
            if (len == 0) { // records do not cross sectors: the rest of this one is empty
                pos = (pos / 2048 + 1) * 2048;
                continue;
            }
            if (pos + len > size) {
                break;
            }
            Uint32 ext, bytes;
            memcpy(&ext, &d[pos + 2], 4);
            memcpy(&bytes, &d[pos + 10], 4);
            Uint8 flags = d[pos + 25], nameLen = d[pos + 32];
            std::string name((const char *)&d[pos + 33], nameLen);
            pos += len;
            if (nameLen == 1 && (name[0] == 0 || name[0] == 1)) {
                continue; // this directory and its parent
            }
            size_t semi = name.find(';');
            if (semi != std::string::npos) {
                name.resize(semi);
            }
            if (!name.empty() && name.back() == '.') {
                name.pop_back();
            }
            if (flags & 2) {
                if (depth < 8) {
                    walk(ext, bytes, dir + name + "/", depth + 1);
                }
            } else if (!files.empty() && files.back().path == dir + name) {
                files.back().size += bytes; // a file in several extents: they follow each other
            } else {
                files.push_back({dir + name, (Uint64)ext * 2048, bytes});
            }
        }
    }
    void open(const std::string &path) {
        Uint8 pvd[2048];
        Uint32 lba, size;
        io = SDL_IOFromFile(path.c_str(), "rb");
        if (io == NULL) {
            throw Stop{"The disc image could not be opened: " + path};
        }
        if (SDL_SeekIO(io, 16 * 2048, SDL_IO_SEEK_SET) < 0 || SDL_ReadIO(io, pvd, sizeof(pvd)) != sizeof(pvd) || pvd[0] != 1 ||
            memcmp(pvd + 1, "CD001", 5) != 0) {
            throw Stop{"This file is not a disc image (.iso). Compressed images (.chd, .cso, .gz) have to be converted to .iso first."};
        }
        memcpy(&lba, pvd + 156 + 2, 4);
        memcpy(&size, pvd + 156 + 10, 4);
        walk(lba, size, "", 0);
    }
    const IsoFile *find(const char *path) {
        for (auto &f : files) {
            if (SDL_strcasecmp(f.path.c_str(), path) == 0) {
                return &f;
            }
        }
        return NULL;
    }
    ~Iso() {
        if (io != NULL) {
            SDL_CloseIO(io);
        }
    }
};

std::string lower(std::string s) {
    for (auto &c : s) {
        c = (char)tolower((unsigned char)c);
    }
    return s;
}

std::string upper(std::string s) {
    for (auto &c : s) {
        c = (char)toupper((unsigned char)c);
    }
    return s;
}

void check_cancel() {
    if (sCancel) {
        throw Stop{"Cancelled. Install continues from where it stopped."};
    }
}

// Copies `size` bytes of the image at `offset` into a new file.
void copy_out(Iso &iso, Uint64 offset, Uint64 size, const std::string &path, std::vector<Uint8> &buf) {
    SDL_IOStream *out = SDL_IOFromFile(path.c_str(), "wb");
    if (out == NULL) {
        throw Stop{"Could not write " + path + ". Is the disk full or the folder read-only?"};
    }
    while (size > 0) {
        size_t n = size < buf.size() ? (size_t)size : buf.size();
        iso.read(offset, buf.data(), n);
        if (SDL_WriteIO(out, buf.data(), n) != n) {
            SDL_CloseIO(out);
            throw Stop{"Could not write " + path + ". Is the disk full?"};
        }
        offset += n;
        size -= n;
    }
    if (!SDL_CloseIO(out)) {
        throw Stop{"Could not write " + path + ". Is the disk full?"};
    }
}

// "@step k n Title": the steps of this run are numbered as the window lists them (setup.cpp, reset_rows).
int sStep, sSteps;
void step(const char *title) {
    char line[96];
    snprintf(line, sizeof(line), "@step %d %d %s", ++sStep, sSteps, title);
    say(line);
}

void step_disc(Iso &iso, const std::string &path) {
    step("Disc image");
    iso.open(path);
    const IsoFile *slus = iso.find("SLUS_216.78"), *dbzp = iso.find("BIN/DBZP.BIN");
    if (slus == NULL || dbzp == NULL) {
        throw Stop{"This is not a disc image of the USA release of Budokai Tenkaichi 3 (no SLUS_216.78 on it).\n"
                   "Other regions and the Wii version have different programs and are not supported."};
    }
    say("@note checking");
    std::vector<Uint8> elf((size_t)slus->size), rom(kRomEnd - kRomBase, 0), menu((size_t)dbzp->size);
    iso.read(slus->offset, elf.data(), elf.size());
    iso.read(dbzp->offset, menu.data(), menu.size());
    if (elf.size() > 0x34 && memcmp(elf.data(), "\177ELF", 4) == 0) {
        Uint32 shoff;
        Uint16 shentsize, shnum;
        memcpy(&shoff, &elf[0x20], 4);
        memcpy(&shentsize, &elf[0x2E], 2);
        memcpy(&shnum, &elf[0x30], 2);
        for (Uint32 i = 0; i < shnum && (size_t)shoff + (size_t)(i + 1) * shentsize <= elf.size(); i++) {
            Uint32 sh[6]; // name, type, flags, address, offset, size
            memcpy(sh, &elf[shoff + (size_t)i * shentsize], sizeof(sh));
            if ((sh[2] & 2) && sh[1] != 8 && sh[5] != 0 && sh[3] >= kRomBase && sh[3] + sh[5] <= kRomEnd && (size_t)sh[4] + sh[5] <= elf.size()) {
                memcpy(&rom[sh[3] - kRomBase], &elf[sh[4]], sh[5]);
            }
        }
    }
    Sha1 a, b;
    a.add(rom.data(), rom.size());
    b.add(menu.data(), menu.size());
    if (a.hex() != kRomSha1 || b.hex() != kDbzpSha1) {
        throw Stop{"The game's programs on this disc do not have the expected checksums.\n"
                   "The port needs the unmodified USA release (SLUS-21678); a patched or damaged image will not work."};
    }
    say("@ok the USA release, checksums match");
}

void step_data(Iso &iso, const std::string &root) {
    std::string data = root + "gamedata/", marker = data + ".installed";
    SDL_PathInfo info;
    step("Game data");
    if (SDL_GetPathInfo(marker.c_str(), &info)) {
        say("@skip already unpacked");
        return;
    }
    std::vector<const IsoFile *> take;
    Uint64 total = 0, done = 0;
    for (auto &f : iso.files) {
        std::string u = upper(f.path);
        if (u == "SLUS_216.78" || u.rfind("BIN/", 0) == 0 || u.rfind("DATA/", 0) == 0) {
            take.push_back(&f);
            total += f.size;
        }
    }
    std::vector<Uint8> buf(1 << 20);
    int files = 0, lastPercent = -1;
    auto progress = [&](Uint64 n) {
        done += n;
        int percent = total ? (int)(done * 100 / total) : 0;
        if (percent != lastPercent) {
            char line[64];
            snprintf(line, sizeof(line), "@note %d%%, %d files", percent, files);
            say(line);
            lastPercent = percent;
        }
        check_cancel();
    };
    for (const IsoFile *f : take) {
        std::string u = upper(f->path);
        bool afs = u.rfind("DATA/", 0) == 0 && u.size() > 4 && u.compare(u.size() - 4, 4, ".AFS") == 0;
        if (!afs) {
            std::string out = data + "disc/" + u; // upper case, as the game asks for them
            SDL_CreateDirectory(out.substr(0, out.rfind('/')).c_str());
            copy_out(iso, f->offset, f->size, out, buf);
            files++;
            progress(f->size);
            continue;
        }
        // an AFS archive: "AFS\0", the number of entries, then (offset, size) of each; one loose file per entry
        std::string stem = lower(u.substr(5, u.size() - 9)), dir = data + stem + "/";
        Uint8 head[8];
        Uint32 count;
        SDL_CreateDirectory(dir.c_str());
        iso.read(f->offset, head, 8);
        memcpy(&count, head + 4, 4);
        if (memcmp(head, "AFS\0", 4) != 0 || (Uint64)count * 8 + 8 > f->size) {
            throw Stop{"An archive on the disc (" + f->path + ") is damaged."};
        }
        std::vector<Uint32> table((size_t)count * 2);
        iso.read(f->offset + 8, table.data(), table.size() * 4);
        Uint64 inArchive = 0;
        for (Uint32 i = 0; i < count; i++) {
            char name[32];
            snprintf(name, sizeof(name), "%05u.bin", i);
            if ((Uint64)table[i * 2] + table[i * 2 + 1] > f->size) {
                throw Stop{"An archive on the disc (" + f->path + ") is damaged."};
            }
            copy_out(iso, f->offset + table[i * 2], table[i * 2 + 1], dir + name, buf);
            files++;
            inArchive += table[i * 2 + 1];
            progress(table[i * 2 + 1]);
        }
        progress(f->size > inArchive ? f->size - inArchive : 0); // the archive's table and padding
    }
    SDL_IOStream *m = SDL_IOFromFile(marker.c_str(), "wb");
    if (m != NULL) {
        SDL_CloseIO(m);
    }
    char line[64];
    snprintf(line, sizeof(line), "@ok %d files in gamedata/", files);
    say(line);
}

void step_test(const std::string &root) {
    std::string exe = root + kGame, saves = root + "gamedata/.selftest";
    const char *args[2] = {exe.c_str(), NULL};
    step("Self-test");
    say("@note the game's demo fight, no window");
    SDL_Environment *env = SDL_CreateEnvironment(true);
    SDL_SetEnvironmentVariable(env, "BT3_DEMO", "1", true);
    SDL_SetEnvironmentVariable(env, "BT3_SETTINGS", "", true);
    SDL_SetEnvironmentVariable(env, "BT3_SAVES", saves.c_str(), true);
    SDL_SetEnvironmentVariable(env, "BT3_GS", "none", true); // no window: a release program opens one by default
    for (const char *name : {"BT3_REPLAY", "BT3_PAD_PLAY", "BT3_PAD_REC", "BT3_DATA"}) {
        SDL_UnsetEnvironmentVariable(env, name);
    }
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)args);
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER, env);
    SDL_SetStringProperty(props, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING, root.c_str());
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
    SDL_Process *proc = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    SDL_DestroyEnvironment(env);
    if (proc == NULL) {
        throw Stop{std::string("The game could not be started: ") + SDL_GetError()};
    }
    size_t size = 0;
    int code = -1;
    char *out = (char *)SDL_ReadProcess(proc, &size, &code);
    std::string text = out != NULL ? std::string(out, size) : "";
    SDL_free(out);
    SDL_DestroyProcess(proc);
    // How the game ended goes into the log too: when it never got as far as printing anything (the system refused
    // to start it, or something stopped it), the exit code is all there is to go on.
    char ended[160];
    const char *why = "";
    switch ((unsigned)code) {
    case 0xC0000135u: why = " (a DLL the program needs was not found)"; break;
    case 0xC0000018u: why = " (the program could not be loaded at its fixed address)"; break;
    case 0xC0000022u: why = " (access denied: often security software blocking the program)"; break;
    case 0xC000007Bu: why = " (a DLL of the wrong kind was loaded)"; break;
    case 0xC0000005u: why = " (crashed: bad memory access)"; break;
    case 0xC0000409u: why = " (stopped by a security check)"; break;
    default: break;
    }
    snprintf(ended, sizeof(ended), "\n[setup] the game exited with code %d (0x%08X)%s\n", code, (unsigned)code, why);
    text += ended;
    {   // a crash report the game wrote itself
        size_t n = 0;
        char *crash = (char *)SDL_LoadFile((root + "bt3_crash.txt").c_str(), &n);
        if (crash != NULL) {
            text += "[setup] bt3_crash.txt:\n" + std::string(crash, n);
            SDL_free(crash);
        }
    }
    SDL_IOStream *log = SDL_IOFromFile((root + "install.log").c_str(), "wb");
    if (log != NULL) {
        SDL_WriteIO(log, text.data(), text.size());
        SDL_CloseIO(log);
    }
    if (text.find("battle finished") == std::string::npos) {
        size_t end = text.find_last_not_of("\r\n"), begin = end == std::string::npos ? std::string::npos : text.rfind('\n', end);
        std::string last = end == std::string::npos ? "no output" : text.substr(begin == std::string::npos ? 0 : begin + 1, end - (begin == std::string::npos ? 0 : begin + 1) + 1);
        throw Stop{"The game's demo fight did not finish. Its last line was:\n" + last + "\nThe full output is in install.log."};
    }
    say("@ok the demo fight ran to its end");
}

// The European release's archive entries: the battle HUD (5 to 9, one per language: English, Spanish, German,
// French, Italian) and the menu archives (USA 448 + k is 451 + k + 31 per language).
std::vector<Uint32> eu_entries() {
    std::vector<Uint32> out;
    for (Uint32 bank = 0; bank < 5; bank++) {
        out.push_back(5 + bank);
        for (Uint32 usa = 448; usa < 479; usa++) {
            out.push_back(451 + (usa - 448) + bank * 31);
        }
    }
    // Story dialogue: 50 episodes for each of the five European languages.
    for (Uint32 entry = 656; entry < 906; entry++) out.push_back(entry);
    return out;
}

void step_eu(const std::string &root, const std::string &path) {
    Iso iso;
    std::string dir = root + "gamedata/pzs3eu1/";
    step("European languages");
    iso.open(path);
    const IsoFile *sles = iso.find("SLES_549.45"), *afs = iso.find("DATA/PZS3EU1.AFS"), *selector = iso.find("DATA/PZS3EU0.AFS");
    const IsoFile *openingAudio = iso.find("DATA/ZS3EUOP.ADX");
    if (sles == NULL || afs == NULL || selector == NULL || openingAudio == NULL) {
        throw Stop{"This is not a disc image of the European release of Budokai Tenkaichi 3 (no SLES_549.45 on it).\n"
                   "The languages come from the European release (SLES-54945); the game itself is set up from the USA one."};
    }
    Uint8 head[8];
    Uint32 count;
    iso.read(afs->offset, head, 8);
    memcpy(&count, head + 4, 4);
    if (memcmp(head, "AFS\0", 4) != 0 || count == 0 || (Uint64)count * 8 + 8 > afs->size) {
        throw Stop{"An archive on the European disc image (DATA/PZS3EU1.AFS) is damaged."};
    }
    std::vector<Uint32> table((size_t)count * 2);
    iso.read(afs->offset + 8, table.data(), table.size() * 4);
    std::vector<Uint8> buf(1 << 20);
    std::vector<Uint32> want = eu_entries();
    SDL_CreateDirectory(dir.c_str());
    int done = 0;
    for (Uint32 e : want) {
        char name[32], line[64];
        if (e >= count || (Uint64)table[e * 2] + table[e * 2 + 1] > afs->size) {
            throw Stop{"An archive on the European disc image (DATA/PZS3EU1.AFS) is damaged or not the European release's."};
        }
        snprintf(name, sizeof(name), "%05u", e);
        copy_out(iso, afs->offset + table[e * 2], table[e * 2 + 1], dir + name + ".part", buf);
        if (!SDL_RenamePath((dir + name + ".part").c_str(), (dir + name + ".bin").c_str())) {
            throw Stop{"Could not write " + dir + name + ".bin."};
        }
        snprintf(line, sizeof(line), "@note %d of %d", ++done, (int)want.size());
        say(line);
        check_cancel();
    }
    iso.read(selector->offset, head, 8);
    memcpy(&count, head + 4, 4);
    if (memcmp(head, "AFS\0", 4) != 0 || count < 2 || selector->size < 24) {
        throw Stop{"The language selector archive on the European disc image (DATA/PZS3EU0.AFS) is damaged."};
    }
    Uint32 selectorEntry[2];
    iso.read(selector->offset + 16, selectorEntry, sizeof(selectorEntry));
    if (selectorEntry[1] == 0 || selectorEntry[1] > (1u << 20) ||
        (Uint64)selectorEntry[0] + selectorEntry[1] > selector->size) {
        throw Stop{"The language selector archive on the European disc image (DATA/PZS3EU0.AFS) is damaged."};
    }
    std::string selectorDir = root + "gamedata/pzs3eu0/";
    SDL_CreateDirectory(selectorDir.c_str());
    copy_out(iso, selector->offset + selectorEntry[0], selectorEntry[1], selectorDir + "00001.part", buf);
    if (!SDL_RenamePath((selectorDir + "00001.part").c_str(), (selectorDir + "00001.bin").c_str())) {
        throw Stop{"Could not write " + selectorDir + "00001.bin."};
    }
    std::string movieDir = root + "gamedata/disc/DATA/";
    SDL_CreateDirectory((root + "gamedata/disc/").c_str());
    SDL_CreateDirectory(movieDir.c_str());
    Uint8 magic[2];
    if (openingAudio->size < 2 || openingAudio->size > (16u << 20)) {
        throw Stop{"The European opening soundtrack has an invalid size."};
    }
    iso.read(openingAudio->offset, magic, sizeof(magic));
    if (magic[0] != 0x80 || magic[1] != 0) {
        throw Stop{"The European opening soundtrack has an invalid header."};
    }
    std::string dest = movieDir + "ZS3EUOP.ADX";
    copy_out(iso, openingAudio->offset, openingAudio->size, dest + ".part", buf);
    if (!SDL_RenamePath((dest + ".part").c_str(), dest.c_str())) {
        throw Stop{"Could not write " + dest + "."};
    }
    say("@ok European languages and opening soundtrack: the game makes the languages at its next start");
}

void work(std::string root, std::string path, std::string eu) {
    try {
        sStep = 0;
        sSteps = (path.empty() ? 0 : 3) + (eu.empty() ? 0 : 1);
        if (!path.empty()) {
            Iso iso;
            step_disc(iso, path);
            check_cancel();
            step_data(iso, root);
            check_cancel();
        }
        if (!eu.empty()) {
            step_eu(root, eu);
            check_cancel();
        }
        if (!path.empty()) {
            step_test(root);
        }
        say("@done " + root + kGame);
    } catch (const Stop &s) {
        std::string why = s.why;
        for (size_t i = 0; (i = why.find('\n', i)) != std::string::npos;) {
            why.replace(i, 1, "\\n");
        }
        say("@fail");
        say("@stopped " + why);
    }
    sRunning = false;
}

} // namespace

void Native_Start(const std::string &root, const std::string &iso, const std::string &eu) {
    if (sThread.joinable()) {
        sThread.join();
    }
    sCancel = false;
    sRunning = true;
    sThread = std::thread(work, root, iso, eu);
}

bool Native_Poll(std::string &line) {
    std::lock_guard<std::mutex> lock(sLock);
    if (sLines.empty()) {
        return false;
    }
    line = sLines.front();
    sLines.erase(sLines.begin());
    return true;
}

bool Native_Running() { return sRunning; }

void Native_Cancel() { sCancel = true; }

void Native_Join() {
    sCancel = true;
    if (sThread.joinable()) {
        sThread.join();
    }
}

bool Native_Installed(const std::string &root) {
    SDL_PathInfo info;
    return SDL_GetPathInfo((root + "gamedata/.installed").c_str(), &info);
}

bool Native_EuInstalled(const std::string &root) {
    SDL_PathInfo info;
    if (!SDL_GetPathInfo((root + "gamedata/pzs3eu0/00001.bin").c_str(), &info)) return false;
    if (!SDL_GetPathInfo((root + "gamedata/disc/DATA/ZS3EUOP.ADX").c_str(), &info)) return false;
    for (Uint32 e : eu_entries()) {
        char name[48];
        snprintf(name, sizeof(name), "gamedata/pzs3eu1/%05u.bin", e);
        if (!SDL_GetPathInfo((root + name).c_str(), &info)) {
            return false;
        }
    }
    return true;
}
