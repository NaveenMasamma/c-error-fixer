#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "utils.h"

namespace fs = std::filesystem;

class FileOpsTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir = fs::temp_directory_path() / "CEFixerTests";
        fs::create_directory(testDir);
    }
    void TearDown() override { fs::remove_all(testDir); }
    fs::path testDir;
};

TEST_F(FileOpsTest, HandlesMissingFile) {
    auto lines = Utils::readFile((testDir / "non_existent.c").string());
    EXPECT_TRUE(lines.empty());
}

TEST_F(FileOpsTest, DetectsReadOnlyFiles) {
    fs::path readOnlyFile = testDir / "readonly.c";
    std::ofstream ofs(readOnlyFile);
    ofs << "content";
    ofs.close();

    // Set to read-only
    fs::permissions(readOnlyFile, fs::perms::owner_read | fs::perms::group_read | fs::perms::others_read);

    bool success = Utils::writeFile(readOnlyFile.string(), {"new content"});
    EXPECT_FALSE(success);
}

TEST_F(FileOpsTest, BackupOverwritesExisting) {
    fs::path src = testDir / "src.c";
    Utils::writeFile(src.string(), {"line1"});
    
    // Create an old backup
    fs::path bkp = testDir / "src.c.backup";
    Utils::writeFile(bkp.string(), {"old backup"});

    bool success = Utils::backupFile(src.string());
    EXPECT_TRUE(success);
    auto lines = Utils::readFile(bkp.string());
    EXPECT_EQ(lines[0], "line1");
}