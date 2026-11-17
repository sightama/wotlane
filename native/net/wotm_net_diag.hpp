// ABOUTME: Diagnostics reports (the DIAG lines a page sends) and their per-match log files: a strict parser that keeps only short,
// ABOUTME: machine-readable fields and a note, and an archive that rotates by age and size. No game code, no sockets.
#pragma once

#include "wotm_net_proto.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace wotm_net::diag
{
    constexpr size_t kMaxFields = 48;      // fields kept from one report
    constexpr size_t kMaxKeyBytes = 16;    // a key: a lower-case letter, then a-z, 0-9 and _
    constexpr size_t kMaxValueBytes = 48;  // a value: A-Z, a-z, 0-9 and . _ : , / + -
    constexpr size_t kMaxNoteBytes = 200;  // the player's own words of a "Send report": the one free text (percent-encoded on the wire)

    // DIAG <key=value>... [note=<percent-encoded text>]: numbers and short tokens only (nothing that could hold a name, address or chat), so
    // a line can be logged as it is. Anything that doesn't fit is dropped, the rest of the report stays.
    struct Report
    {
        std::vector<std::pair<std::string, std::string>> fields;   // in the order sent
        std::string note;                                          // decoded, printable, valid UTF-8, at most kMaxNoteBytes
        std::string get(const std::string &key) const
        {
            for (const auto &f : fields)
                if (f.first == key)
                    return f.second;
            return {};
        }
    };

    namespace detail
    {
        inline bool validKey(const std::string &k)
        {
            if (k.empty() || k.size() > kMaxKeyBytes || !(k[0] >= 'a' && k[0] <= 'z'))
                return false;
            return k.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") == std::string::npos;
        }

        inline bool validValue(const std::string &v)
        {
            return !v.empty() && v.size() <= kMaxValueBytes &&
                   v.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._:,/+-") == std::string::npos;
        }

        inline int hexDigit(char c)
        {
            return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        }

        // percent-decoded, control characters removed, valid UTF-8 or nothing, cut to kMaxNoteBytes on a character boundary
        inline std::string cleanNote(const std::string &encoded)
        {
            std::string text;
            for (size_t i = 0; i < encoded.size(); ++i)
            {
                char c = encoded[i];
                if (c == '%' && i + 2 < encoded.size() && hexDigit(encoded[i + 1]) >= 0 && hexDigit(encoded[i + 2]) >= 0)
                {
                    c = static_cast<char>(hexDigit(encoded[i + 1]) * 16 + hexDigit(encoded[i + 2]));
                    i += 2;
                }
                if (static_cast<unsigned char>(c) >= 0x20 && static_cast<unsigned char>(c) != 0x7f)
                    text += c;
            }
            if (!proto::validUtf8(text))
                return {};
            if (text.size() > kMaxNoteBytes)
            {
                size_t cut = kMaxNoteBytes;
                while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80)
                    --cut;
                text.resize(cut);
            }
            const size_t first = text.find_first_not_of(' '), last = text.find_last_not_of(' ');
            return first == std::string::npos ? std::string{} : text.substr(first, last - first + 1);
        }
    }

    // `words`: the line's words, "DIAG" first. False when nothing usable is in it
    inline bool parse(const std::vector<std::string> &words, Report &out)
    {
        out = Report{};
        for (size_t i = 1; i < words.size(); ++i)
        {
            const std::string &w = words[i];
            const size_t eq = w.find('=');
            if (eq == std::string::npos)
                continue;
            const std::string key = w.substr(0, eq), value = w.substr(eq + 1);
            if (key == "note")
            {
                if (out.note.empty())
                    out.note = detail::cleanNote(value);
                continue;
            }
            if (!detail::validKey(key) || !detail::validValue(value) || out.fields.size() >= kMaxFields || !out.get(key).empty())
                continue;
            out.fields.emplace_back(key, value);
        }
        return !out.fields.empty() || !out.note.empty();
    }

    // the report as the log line's tail: key=value ... note="text" (quotes and backslashes in the note escaped)
    inline std::string format(const Report &r)
    {
        std::string out;
        for (const auto &f : r.fields)
            out += (out.empty() ? "" : " ") + f.first + "=" + f.second;
        if (!r.note.empty())
        {
            std::string quoted;
            for (char c : r.note)
            {
                if (c == '"' || c == '\\')
                    quoted += '\\';
                quoted += c;
            }
            out += (out.empty() ? "" : " ") + std::string("note=\"") + quoted + "\"";
        }
        return out;
    }

    // UTC time as YYYY-MM-DDTHH:MM:SSZ (`compact`: YYYYMMDD-HHMMSS, for file names)
    inline std::string utcNow(bool compact = false)
    {
        const std::time_t t = std::time(nullptr);
        std::tm tm{};
#ifdef _WIN32
        gmtime_s(&tm, &t);
#else
        gmtime_r(&t, &tm);
#endif
        char b[32];
        std::strftime(b, sizeof b, compact ? "%Y%m%d-%H%M%S" : "%Y-%m-%dT%H:%M:%SZ", &tm);
        return b;
    }

    struct Limits
    {
        int keepDays = 7;                           // files untouched for longer than this are deleted
        uint64_t maxTotalBytes = 64ull << 20;       // above this the oldest files are deleted (the newest stays)
        uint64_t maxFileBytes = 2ull << 20;         // a file stops growing here (one "truncated" line says so)
    };

    // The files of one folder: one per match (and one per lobby before its match), named <UTC time>-<code>-<kind>.log, plus a loose
    // reports file per day. With no folder it does nothing (a server run on a PC to play with friends writes no files).
    class Archive
    {
    public:
        Archive(std::string dir, Limits limits) : dir_(std::move(dir)), limits_(limits)
        {
            if (!dir_.empty())
            {
                std::error_code ec;
                std::filesystem::create_directories(dir_, ec);
            }
        }

        bool enabled() const { return !dir_.empty(); }

        // a new file for `code` (kind: "lobby" or "match"); it becomes the code's latest. Returns the file's name ("" when off)
        std::string begin(const std::string &code, const std::string &kind)
        {
            if (!enabled())
                return {};
            const std::string stem = utcNow(true) + "-" + code + "-" + kind;
            std::string name = stem + ".log";
            for (int n = 2; std::filesystem::exists(std::filesystem::path(dir_) / name); ++n)
                name = stem + "-" + std::to_string(n) + ".log";
            if (latest_.size() > kMaxCodes)   // a server up for months sees many codes; a reply to a report on a lobby this old isn't worth the memory
                latest_.clear();
            latest_[code] = name;
            return name;
        }

        // the code's newest file this process began ("" when it began none)
        std::string latest(const std::string &code) const
        {
            const auto it = latest_.find(code);
            return it == latest_.end() ? std::string{} : it->second;
        }

        // the loose file for reports that name a lobby but can't be tied to one: one per UTC day
        std::string reportsFile() const { return "reports-" + utcNow(true).substr(0, 8) + ".log"; }

        // One line (a newline is added) to `file`, created when missing. Past the file's cap lines are dropped after one "truncated"
        // line. Lines that come from players (`fromServer` false) only get three quarters of the cap, so a player's reports can't
        // crowd out the server's own lines.
        void append(const std::string &file, const std::string &line, bool fromServer = true)
        {
            if (!enabled() || file.empty())
                return;
            const std::filesystem::path path = std::filesystem::path(dir_) / file;
            auto it = files_.find(file);
            if (it == files_.end())
            {
                std::error_code ec;
                const auto size = std::filesystem::exists(path, ec) ? std::filesystem::file_size(path, ec) : 0u;
                it = files_.emplace(file, FileState{ec ? 0u : static_cast<uint64_t>(size), false, false}).first;
            }
            FileState &st = it->second;
            bool &closed = fromServer ? st.serverClosed : st.clientClosed;
            const uint64_t cap = fromServer ? limits_.maxFileBytes : limits_.maxFileBytes - limits_.maxFileBytes / 4;
            if (closed)
                return;
            std::string text = line + "\n";
            if (st.size + text.size() > cap)
            {
                text = "(truncated: this file reached its size cap)\n";
                closed = true;
            }
            std::ofstream out(path, std::ios::binary | std::ios::app);
            if (!out)
                return;
            out << text;
            st.size += text.size();
        }

        // deletes the files older than keepDays, then the oldest while the folder is over maxTotalBytes (the newest file stays)
        void prune()
        {
            if (!enabled())
                return;
            namespace fs = std::filesystem;
            struct Entry
            {
                fs::path path;
                fs::file_time_type time;
                uint64_t size;
            };
            std::vector<Entry> files;
            std::error_code ec;
            for (fs::directory_iterator it(dir_, ec), end; !ec && it != end; it.increment(ec))
            {
                std::error_code e;
                if (!it->is_regular_file(e) || it->path().extension() != ".log")
                    continue;
                const auto time = fs::last_write_time(it->path(), e);
                const auto size = fs::file_size(it->path(), e);
                if (!e)
                    files.push_back({it->path(), time, static_cast<uint64_t>(size)});
            }
            std::sort(files.begin(), files.end(), [](const Entry &a, const Entry &b) { return a.time < b.time; });
            const auto oldest = fs::file_time_type::clock::now() - std::chrono::hours(24 * limits_.keepDays);
            uint64_t total = 0;
            for (const Entry &f : files)
                total += f.size;
            for (size_t i = 0; i < files.size(); ++i)
            {
                const bool stale = files[i].time < oldest, over = total > limits_.maxTotalBytes && i + 1 < files.size();
                if (!stale && !over)
                    continue;
                std::error_code e;
                fs::remove(files[i].path, e);
                if (!e)
                {
                    total -= files[i].size;
                    files_.erase(files[i].path.filename().string());
                }
            }
        }

    private:
        static constexpr size_t kMaxCodes = 1024;
        std::string dir_;
        Limits limits_;
        std::map<std::string, std::string> latest_;   // lobby code -> its newest file
        struct FileState
        {
            uint64_t size = 0;
            bool serverClosed = false, clientClosed = false;   // the "truncated" line was written for that kind of line
        };
        std::map<std::string, FileState> files_;      // every file this process has appended to
    };
}
