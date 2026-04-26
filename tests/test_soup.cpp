#include "soup.hpp"
#include <atomic>
#include <gtest/gtest.h>
#include <set>

using replicator::Soup;

TEST(Soup, PopulationSizeIsConstant) {
    Soup s(64, 32, 1);
    s.run(500, 64);
    EXPECT_EQ(s.population(), 64u);
    for (std::size_t i = 0; i < s.population(); ++i) {
        EXPECT_EQ(s.cell(i).size(), 32u);
    }
}

TEST(Soup, SameSeedReproducible) {
    Soup a(32, 16, 42);
    Soup b(32, 16, 42);
    a.run(200, 32);
    b.run(200, 32);
    EXPECT_EQ(a.snapshot(), b.snapshot());
}

TEST(Soup, DifferentSeedDiverges) {
    Soup a(32, 16, 1);
    Soup b(32, 16, 2);
    a.run(200, 32);
    b.run(200, 32);
    EXPECT_NE(a.snapshot(), b.snapshot());
}

TEST(Soup, PickPairReturnsDistinctIndices) {
    Soup s(8, 4, 7);
    for (int i = 0; i < 1000; ++i) {
        auto [x, y] = s.pick_pair();
        ASSERT_NE(x, y);
        ASSERT_LT(x, s.population());
        ASSERT_LT(y, s.population());
    }
}

TEST(Soup, InitialPopulationHasDiversity) {
    Soup s(256, 32, 123);
    auto snap = s.snapshot();
    std::set<std::vector<std::uint8_t>> uniq(snap.begin(), snap.end());
    // With 256*32 random bytes the chance of any duplicate is astronomically small.
    EXPECT_EQ(uniq.size(), 256u);
}

TEST(Soup, RunParallelPreservesShape) {
    Soup s(64, 32, 11);
    std::atomic<bool> stop{false};
    s.run_parallel(2000, 64, 4, stop);
    EXPECT_EQ(s.population(), 64u);
    for (std::size_t i = 0; i < s.population(); ++i) {
        EXPECT_EQ(s.cell(i).size(), 32u);
    }
}

TEST(Soup, RunParallelSingleThreadMatchesRun) {
    // n_threads <= 1 must behave exactly like run().
    Soup a(32, 16, 99);
    Soup b(32, 16, 99);
    std::atomic<bool> stop{false};
    a.run(300, 32);
    b.run_parallel(300, 32, 1, stop);
    EXPECT_EQ(a.snapshot(), b.snapshot());
}
