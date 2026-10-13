// SPDX-License-Identifier: GPL-3.0-or-later
// Shared by the native renderer tests: a minimal read-only BIG archive index and a W3D file factory over it.
// Include after the WWLib headers (ffactory.h, ramfile.h) and the standard headers used below.
#pragma once
namespace {
// Minimal read-only BIG archive index (later archives override earlier entries by base name).
class BigFiles
{
public:
    bool Add(const std::string &path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;
        unsigned char header[16];
        file.read(reinterpret_cast<char *>(header), 16);
        if (!file || std::memcmp(header, "BIGF", 4)) return false;
        const unsigned count = (header[8] << 24) | (header[9] << 16) | (header[10] << 8) | header[11];
        const size_t archive = paths_.size();
        paths_.push_back(path);
        for (unsigned i = 0; i < count; ++i) {
            unsigned char e[8];
            file.read(reinterpret_cast<char *>(e), 8);
            std::string name;
            for (char c; file.get(c) && c;) name.push_back(c);
            if (!file) return false;
            const unsigned offset = (e[0] << 24) | (e[1] << 16) | (e[2] << 8) | e[3];
            const unsigned size = (e[4] << 24) | (e[5] << 16) | (e[6] << 8) | e[7];
            files_[Base(name)] = Entry{archive, offset, size};
        }
        return true;
    }
    bool Read(const std::string &name, std::vector<unsigned char> &out) const
    {
        const auto found = files_.find(Base(name));
        if (found == files_.end()) return false;
        std::ifstream file(paths_[found->second.archive], std::ios::binary);
        file.seekg(found->second.offset);
        out.resize(found->second.size);
        file.read(reinterpret_cast<char *>(out.data()), static_cast<std::streamsize>(out.size()));
        return static_cast<bool>(file);
    }
    std::vector<std::string> Names(const char *suffix) const
    {
        std::vector<std::string> names;
        const size_t length = std::strlen(suffix);
        for (const auto &entry : files_)
            if (entry.first.size() >= length && !entry.first.compare(entry.first.size() - length, length, suffix))
                names.push_back(entry.first);
        std::sort(names.begin(), names.end());
        return names;
    }
private:
    struct Entry { size_t archive; unsigned offset, size; };
    static std::string Base(const std::string &name)
    {
        const size_t slash = name.find_last_of("\\/");
        std::string base = slash == std::string::npos ? name : name.substr(slash + 1);
        for (char &c : base) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return base;
    }
    std::vector<std::string> paths_;
    std::unordered_map<std::string, Entry> files_;
};

class BigFileFactory : public FileFactoryClass
{
public:
    explicit BigFileFactory(const BigFiles &files) : files_(files) {}
    FileClass *Get_File(char const *filename) override
    {
        std::vector<unsigned char> bytes;
        if (!filename || !files_.Read(filename, bytes) || bytes.empty()) return nullptr;
        auto owned = std::make_unique<std::vector<unsigned char>>(std::move(bytes));
        FileClass *file = new RAMFileClass(owned->data(), static_cast<int>(owned->size()));
        storage_[file] = std::move(owned);
        return file;
    }
    void Return_File(FileClass *file) override
    {
        storage_.erase(file);
        delete file;
    }
private:
    const BigFiles &files_;
    std::unordered_map<FileClass *, std::unique_ptr<std::vector<unsigned char>>> storage_;
};

bool EndsWith(const std::string &s, const char *suffix)
{
    const size_t n = std::strlen(suffix);
    return s.size() >= n && !s.compare(s.size() - n, n, suffix);
}
}

