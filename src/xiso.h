#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

// Reads the Xbox game filesystem (XDVDFS) from a disc image: a full disc dump,
// where the game partition starts part-way into the image, or a trimmed XISO,
// where it starts at 0. Read only; the image is never modified.
namespace launcher {
namespace fs = std::filesystem;

struct XisoFile {
    std::string path;       // relative, '/'-separated, as stored on the disc
    uint64_t offset = 0;    // absolute byte offset in the image
    uint32_t size = 0;
};

class Xiso {
public:
    ~Xiso();
    // False with a reason when the file is not an Xbox game image.
    bool open(const fs::path& image, std::wstring& error);
    const std::vector<XisoFile>& files() const { return files_; }
    uint64_t total_bytes() const;
    const XisoFile* find(const std::string& path) const;  // case-insensitive
    bool read(const XisoFile& file, std::vector<unsigned char>& out, std::wstring& error) const;
    // Copies every file under target. progress(done, total) returns false to cancel.
    bool extract(const fs::path& target, const std::function<bool(uint64_t, uint64_t)>& progress, std::wstring& error) const;
    // The title ID from default.xbe's certificate; 0 when unreadable.
    uint32_t title_id() const;
private:
    bool read_at(uint64_t offset, void* data, uint32_t size) const;
    bool walk(uint32_t sector, uint32_t size, const std::string& prefix, int depth, std::wstring& error);
    void* handle_ = nullptr;
    uint64_t image_size_ = 0, base_ = 0;
    std::vector<XisoFile> files_;
    std::vector<std::string> directories_;
};

// X-Men Legends' title ID, as it appears in the game's own save paths
// (UDATA/4156001e) and default.xbe certificate.
constexpr uint32_t kXml1TitleId = 0x4156001E;

}
