#include <gtest/gtest.h>
#include "Utils/Math/Math.h"
#include "../../../SEOwnedDE/SEOwnedDE/include/nlohmann/json.hpp"
#include "../helpers/SourceContractAssertions.h"

TEST(NativeHitscanGeometry, MatchesCapturedClientDllIntersections) {
    const auto path = testhelpers::FindRepoRoot()
        / "docs/verification/data/2026-11-20-native-hitscan-geometry.json";
    const auto data = nlohmann::json::parse(testhelpers::ReadTextFile(path));
    ASSERT_EQ(data.at("client_sha256").get<std::string>(),
        "b6f45c74805edacf0cf61d965f658158fc22960f66d583ef3a6549eb56696b95");
    ASSERT_EQ(data.at("fixture_count").get<int>(), 324);
    ASSERT_EQ(data.at("fixtures").size(), 324u);
    const auto vector = [](const nlohmann::json& value) {
        return Vec3(value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>());
    };
    for (const auto& fixture : data.at("fixtures")) {
        SCOPED_TRACE(fixture.at("name").get<std::string>());
        matrix3x4_t bone{};
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 4; ++col)
                bone[row][col] = fixture.at("bone").at(row).at(col).get<float>();
        EXPECT_EQ(Math::RayToHitbox(vector(fixture.at("origin")), vector(fixture.at("direction")),
            vector(fixture.at("mins")), vector(fixture.at("maxs")), bone,
            fixture.at("inset").get<float>(), fixture.at("max_distance").get<float>()),
            fixture.at("native_hit").get<bool>());
    }
}
