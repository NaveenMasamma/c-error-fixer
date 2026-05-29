#include <gtest/gtest.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <string>
#include "code_fixer.h"
#include "FixCoordinator.h"
#include "compiler_error_parser.h"
#include "utils.h"

static std::string findExecutable(const std::string& name) {
    std::filesystem::path path = std::filesystem::current_path() / name;
    if (std::filesystem::exists(path)) return path.string();
    path = std::filesystem::current_path() / ".." / name;
    if (std::filesystem::exists(path)) return path.lexically_normal().string();
    return name;
}

static std::string runCommandCaptureOutput(const std::string& command) {
    std::string result;
    std::array<char, 256> buffer;
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return result;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

class NegativeCaseTest : public ::testing::Test {
protected:
    std::filesystem::path temp_dir;

    void SetUp() override {
        temp_dir = std::filesystem::temp_directory_path() / "c_error_fixer_test";
        std::filesystem::create_directories(temp_dir);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::permissions(temp_dir, std::filesystem::perms::owner_all, std::filesystem::perm_options::add, ec);
        std::filesystem::remove_all(temp_dir, ec);
    }

    std::filesystem::path makeFile(const std::string& name, const std::string& contents) {
        auto path = temp_dir / name;
        std::ofstream file(path);
        file << contents;
        file.close();
        return path;
    }
};

TEST_F(NegativeCaseTest, ReadOnlySourceFileDoesNotCrash) {
    auto source_path = makeFile("readonly.c", "int main() { return 0; }");
    std::filesystem::permissions(source_path, std::filesystem::perms::owner_read, std::filesystem::perm_options::replace);

    CodeFixer fixer;
    FixSuggestion suggestion;
    suggestion.is_safe = true;
    CodeDelta delta;
    delta.line_number = 1;
    delta.old_content = "int main() { return 0; }";
    delta.new_content = "int main() { return 0; }";
    suggestion.deltas.push_back(delta);

    EXPECT_FALSE(fixer.applyAllFixes(source_path.string(), {suggestion}));
}

TEST_F(NegativeCaseTest, ReadOnlyBackupFileDoesNotCrash) {
    auto source_path = makeFile("source.c", "int main() { return 0; }");
    auto backup_path = source_path.string() + ".backup";
    std::ofstream backup(backup_path);
    backup << "backup";
    backup.close();
    std::filesystem::permissions(backup_path, std::filesystem::perms::owner_read, std::filesystem::perm_options::replace);

    CodeFixer fixer;
    FixSuggestion suggestion;
    suggestion.is_safe = true;
    CodeDelta delta;
    delta.line_number = 1;
    delta.old_content = "int main() { return 0; }";
    delta.new_content = "int main() { return 0; }";
    suggestion.deltas.push_back(delta);

    EXPECT_TRUE(fixer.applyAllFixes(source_path.string(), {suggestion}));
}

TEST_F(NegativeCaseTest, MissingFileHandledGracefully) {
    auto missing_path = temp_dir / "does_not_exist.c";
    auto lines = Utils::readFile(missing_path.string());
    EXPECT_TRUE(lines.empty());

    CodeFixer fixer;
    FixSuggestion suggestion;
    suggestion.is_safe = true;
    CodeDelta delta;
    delta.line_number = 1;
    delta.old_content = "";
    delta.new_content = "";
    suggestion.deltas.push_back(delta);

    EXPECT_FALSE(fixer.applyAllFixes(missing_path.string(), {suggestion}));
}

TEST_F(NegativeCaseTest, InvalidEmptyFilePathWritesFail) {
    std::vector<std::string> lines = {"int main() { return 0; }"};
    EXPECT_FALSE(Utils::writeFile("", lines));
}

TEST_F(NegativeCaseTest, DirectoryInsteadOfFileIsRejected) {
    auto dir_path = temp_dir / "a_directory";
    std::filesystem::create_directory(dir_path);
    auto lines = Utils::readFile(dir_path.string());
    EXPECT_TRUE(lines.empty());
}

TEST_F(NegativeCaseTest, UnsupportedCompilerErrorGeneratesNoSuggestions) {
    FixCoordinator coordinator;
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    CompilerError error;
    error.error_code = "unknown-error";
    error.error_message = "this error pattern is not supported";
    error.line_number = 1;
    error.file_path = "test.c";

    std::vector<std::string> lines = {"int main() {}"};
    auto suggestions = coordinator.getBestSuggestions(error, lines, analyzer, db);
    EXPECT_TRUE(suggestions.empty());
}

TEST_F(NegativeCaseTest, HelpOptionShowsUsageInformation) {
    std::string executable = findExecutable("c_error_fixer");
    std::string output = runCommandCaptureOutput(executable + " --help 2>&1");
    EXPECT_NE(output.find("Usage:"), std::string::npos);
    EXPECT_NE(output.find("--dry-run"), std::string::npos);
    EXPECT_NE(output.find("Examples:"), std::string::npos);
}

TEST_F(NegativeCaseTest, BackupPreservedWhenExistingBackupIsReadOnly) {
    auto source_path = makeFile("source.c", "int main() { return 0; }");
    auto backup_base = source_path.string() + ".backup";
    std::ofstream backup(backup_base);
    backup << "old backup";
    backup.close();

    EXPECT_TRUE(Utils::backupFile(source_path.string()));

    int backup_count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(temp_dir)) {
        auto filename = entry.path().filename().string();
        if (filename.rfind(source_path.filename().string() + ".backup", 0) == 0) {
            backup_count++;
        }
    }
    EXPECT_GE(backup_count, 2);
}
