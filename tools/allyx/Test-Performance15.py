"""Exercise the renderer's kept-cache rebuild decision with real staging files."""
import argparse
from pathlib import Path
import subprocess
from tempfile import TemporaryDirectory

parser = argparse.ArgumentParser()
parser.add_argument("--compiler", required=True)
parser.add_argument("--out", required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
out = Path(args.out).resolve()
out.mkdir(parents=True, exist_ok=True)
source = (root / "src/video_core/renderer_vulkan/vk_pipeline_cache.cpp").read_text()
def method(name):
    start = source.index("bool PipelineCache::" + name + "(")
    return source[start:source.index("\n}", start) + 2]

fixture = r'''
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#define LOG_INFO(...)
#define LOG_WARNING(...)
#undef assert
#define assert(test) do { if (!(test)) { std::cerr << "FAIL: " #test << '\n'; std::exit(1); } } while (false)
namespace fs = std::filesystem;
using u64 = std::uint64_t;
constexpr int CACHE_VERSION = 1;
namespace PrecompiledKept {
struct Index : std::ifstream {
    using std::ifstream::ifstream;
    static std::shared_ptr<Index> Load(const fs::path& file, const fs::path&,
                                       const std::string&, int, std::string&) {
        auto reader = std::make_shared<Index>(file, std::ios::binary);
        return *reader && reader->peek() != '!' ? reader : nullptr;
    }
};
}
struct PipelineCache {
    std::shared_ptr<PrecompiledKept::Index> kept_index;
    std::mutex kept_mutex;
    std::atomic<bool> kept_available{};
    fs::path kept_source, kept_file;
    std::string published_kept_stamp;
    std::weak_ptr<PrecompiledKept::Index> kept_part_reader;
    bool BeginKeptRebuild(const fs::path&, const fs::path&, const std::string&);
    bool PublishKeptFile(const fs::path&, const fs::path&, const std::string&, bool,
                         const fs::path& = {});
    bool sample_valid = true;
    bool CheckKeptSample(const PrecompiledKept::Index&, const std::string&) { return sample_valid; }
    void Publish(const fs::path& file, const fs::path& source) {
        assert(PublishKeptFile(file, source, "valid", true));
    }
};
'''
checks = r'''
int main() try {
    const auto source = fs::absolute("recipe");
    const auto final = fs::absolute("cache.bin");
    const auto part = fs::absolute("cache.bin.part");
    std::ofstream(source) << 's';
    std::ofstream(final) << 'a';

    PipelineCache cache;
    assert(cache.BeginKeptRebuild(source, part, "valid"));
    assert(!cache.kept_available && !cache.kept_index);
    cache.Publish(final, source);
    auto reader = cache.kept_index;
    assert(cache.BeginKeptRebuild(source, part, "valid"));
    std::ofstream(part) << 'b'; // an interrupted rebuild may leave staging behind
    assert(cache.kept_available && cache.kept_index == reader && reader->peek() == 'a');
    assert(cache.BeginKeptRebuild(source, part, "valid"));
    assert(cache.kept_index == reader);
    const auto replacement = fs::absolute("completed.bin");
    assert(!cache.PublishKeptFile(replacement, source, "valid", false));
    assert(cache.kept_available && cache.kept_index == reader);
    const auto corrupt = fs::absolute("bad-header.part");
    std::ofstream(corrupt) << '!';
    assert(!cache.PublishKeptFile(corrupt, source, "valid", false, final));
    assert(std::ifstream(final).peek() == 'a' && cache.kept_index == reader);
    const auto bad = fs::absolute("rejected.bin");
    std::ofstream(bad) << 'x';
    cache.sample_valid = false;
    assert(!cache.PublishKeptFile(bad, source, "valid", true, final));
    assert(!fs::exists(bad) && cache.kept_index == reader);
    cache.sample_valid = true;
    assert(cache.PublishKeptFile(part, source, "valid", false, replacement));
    assert(reader->peek() == 'a' && cache.kept_index->peek() == 'b');
    assert(cache.BeginKeptRebuild(source, part, "different settings"));
    assert(!cache.kept_available && !cache.kept_index);
    cache.Publish(final, source);
    assert(cache.BeginKeptRebuild(fs::absolute("other recipe"), part, "valid"));
    assert(!cache.kept_available && !cache.kept_index);

    std::ofstream(part) << 'p';
    PipelineCache partial;
    partial.Publish(part, source);
    auto stage_reader = partial.kept_index;
    assert(!partial.BeginKeptRebuild(source, part, "valid"));
    assert(partial.kept_available && partial.kept_index == stage_reader);
    assert(!partial.BeginKeptRebuild(source, part, "changed"));
    assert(!partial.kept_available && !partial.kept_index);
    assert(!partial.BeginKeptRebuild(source, part, "changed"));
    assert(stage_reader->peek() == 'p');
    stage_reader.reset();
    assert(partial.BeginKeptRebuild(source, part, "changed"));

    const auto alias = fs::absolute("hardlinked.part");
    std::error_code link_error;
    fs::create_hard_link(final, alias, link_error);
    const auto equivalent_path = link_error ? final.parent_path() / "." / final.filename() : alias;
    assert(equivalent_path != final && fs::equivalent(equivalent_path, final));
    PipelineCache linked;
    linked.Publish(final, source);
    assert(!linked.BeginKeptRebuild(source, equivalent_path, "valid"));
    assert(linked.kept_available && linked.kept_index->peek() == 'a');
    std::cout << "PASS: valid cache stays readable during rebuild and interruption; "
                 "source/settings mismatches withdraw it; staging and file aliases protect readers\n";
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
'''
(out / "kept-rebuild.cpp").write_text(fixture + method("BeginKeptRebuild") + "\n" +
                                      method("PublishKeptFile") + checks)
exe = out / "kept-rebuild.exe"
subprocess.run([args.compiler, "/nologo", "/std:c++20", "/EHsc", "/O2", "/MD",
                "/UNDEBUG", "kept-rebuild.cpp", "/Fe:" + str(exe), "/Fo:kept-rebuild.obj"],
               cwd=out, check=True)
with TemporaryDirectory(dir=out) as directory:
    subprocess.run([str(exe)], cwd=directory, check=True)
