#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "FixCoordinator.h"
#include "MockFixEngine.h"

using ::testing::Return;
using ::testing::_;

TEST(FixCoordinatorTest, RanksByConfidence) {
    FixCoordinator coordinator;
    auto engine1 = std::make_unique<MockFixEngine>();
    auto engine2 = std::make_unique<MockFixEngine>();

    FixSuggestion lowConf; lowConf.confidence = 0.5f;
    FixSuggestion highConf; highConf.confidence = 0.9f;

    EXPECT_CALL(*engine1, canHandle("test-err")).WillRepeatedly(Return(true));
    EXPECT_CALL(*engine1, generateSuggestions(_, _, _, _))
        .WillOnce(Return(std::vector<FixSuggestion>{lowConf}));

    EXPECT_CALL(*engine2, canHandle("test-err")).WillRepeatedly(Return(true));
    EXPECT_CALL(*engine2, generateSuggestions(_, _, _, _))
        .WillOnce(Return(std::vector<FixSuggestion>{highConf}));

    coordinator.registerEngine(std::move(engine1));
    coordinator.registerEngine(std::move(engine2));

    CompilerError err;
    err.error_code = "test-err";
    
    auto results = coordinator.getBestSuggestions(err, {}, *(new CodeAnalyzer()), *(new ErrorPatternDB()));

    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].confidence, 0.9f); // Higher confidence first
    EXPECT_EQ(results[1].confidence, 0.5f);
}

TEST(FixCoordinatorTest, SkipsEnginesThatCannotHandle) {
    FixCoordinator coordinator;
    auto engine = std::make_unique<MockFixEngine>();

    EXPECT_CALL(*engine, canHandle("unhandled")).WillOnce(Return(false));
    EXPECT_CALL(*engine, generateSuggestions(_, _, _, _)).Times(0);

    coordinator.registerEngine(std::move(engine));

    CompilerError err;
    err.error_code = "unhandled";
    auto results = coordinator.getBestSuggestions(err, {}, *(new CodeAnalyzer()), *(new ErrorPatternDB()));
    EXPECT_TRUE(results.empty());
}