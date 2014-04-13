#include "../src/core/utilities.hpp"

#include <random>
#include <list>

#include "gtest/gtest.h"

using namespace panoramix;

TEST(UtilTest, BoundBetween) {
    for (int i = 0; i < 10000; i++){
        double x = double(rand()) / rand() + rand();
        double a = double(rand()) / rand() + rand();
        double b = a + abs(double(rand()) / rand());
        if (a == b || std::isnan(x) || std::isnan(a) || std::isnan(b) ||
            std::isinf(x) || std::isinf(a) || std::isinf(b))
            continue;
        double xx = core::BoundBetween(x, a, b);
        ASSERT_LE(a, xx);
        ASSERT_LE(xx, b);
        ASSERT_TRUE(xx == x || xx == a || xx == b);
    }
}

TEST(UtilTest, WrapBetween) {
    for (int i = 0; i < 10000; i++){
        double x = double(rand()) / rand() + rand();
        double a = double(rand()) / rand() + rand();
        double b = a + abs(double(rand())/rand());
        if (a == b || std::isnan(x) || std::isnan(a) || std::isnan(b) || 
            std::isinf(x) || std::isinf(a) || std::isinf(b))
            continue;
        double xx = core::WrapBetween(x, a, b);
        double rem = (xx - x) / (b - a) - std::round((xx - x) / (b - a));
        if (std::isnan(rem)){
            assert(0);
        }
        ASSERT_NEAR(0, rem, 1e-5);
        ASSERT_LE(a, xx);
        ASSERT_LT(xx, b);
    }

    // int test
    int x = core::WrapBetween(0, 1, 2);
    for (int i = 0; i < 10000; i++){
        int x = rand();
        int a = rand();
        int b = a + abs(rand());
        if (a == b)
            continue;

        if (core::WrapBetween(x, a, b) == b){
            std::cout << a << b << std::endl;
        }

        EXPECT_LE(a, core::WrapBetween(x, a, b));
        EXPECT_LT(core::WrapBetween(x, a, b), b);
    }
}

TEST(UtilTest, SubscriptAndIndex) {

    auto i = core::EncodeSubscriptToIndex(core::Point<int, 2>(1, 2), core::Vec<int, 2>(2, 3));
    ASSERT_EQ(5, i);

    int trueIndex = 0;
    for (int a = 0; a < 10; a++){
        for (int b = 0; b < 20; b++){
            for (int c = 0; c < 15; c++){
                for (int d = 0; d < 9; d++){                    
                    int index = core::EncodeSubscriptToIndex(core::Point<int, 4>(a, b, c, d), 
                        core::Vec<int, 4>(10, 20, 15, 9));
                    ASSERT_EQ(trueIndex, index);
                    
                    auto subs = core::DecodeIndexToSubscript(index, core::Vec<int, 4>(10, 20, 15, 9));
                    ASSERT_EQ(0, core::norm(core::Point<int, 4>(a, b, c, d) - subs));
                    
                    trueIndex++;
                }
            }
        }
    }

}


TEST(UtilTest, AngleBetweenDirections) {
    core::Vec2 v1(1, 0), v2(1, 1);
    ASSERT_DOUBLE_EQ(M_PI_4, core::AngleBetweenDirections(v1, v2));
    ASSERT_DOUBLE_EQ(M_PI_4, core::SignedAngleBetweenDirections(v1, v2, false));
    ASSERT_DOUBLE_EQ(-M_PI_4, core::SignedAngleBetweenDirections(v1, v2, true));
    ASSERT_DOUBLE_EQ(-M_PI_4, core::SignedAngleBetweenDirections(v1, v2));
    core::Vec2 v3(-1, -1);
    ASSERT_DOUBLE_EQ(-M_PI_4 * 3, core::SignedAngleBetweenDirections(v1, v3, false));
    ASSERT_DOUBLE_EQ(M_PI_4 * 3, core::SignedAngleBetweenDirections(v1, v3, true));
    ASSERT_FLOAT_EQ(M_PI, core::AngleBetweenDirections(v2, v3));
}

TEST(UtilTest, DistanceFromPointToLine) {
    core::Line3 l;
    l.first = { 1, 0, 0 };
    l.second = { -1, 0, 0 };
    for (double x = -3; x <= 3; x += 0.5) {
        core::Point3 p(x, 1, 0);
        if (x < -1){
            ASSERT_DOUBLE_EQ(core::norm(l.second - p), core::DistanceFromPointToLine(p, l).first);
        }
        else if (x > 1){
            ASSERT_DOUBLE_EQ(core::norm(l.first - p), core::DistanceFromPointToLine(p, l).first);
        }
        else{
            ASSERT_DOUBLE_EQ(1, core::DistanceFromPointToLine(p, l).first);
        }
        ASSERT_DOUBLE_EQ(1, core::norm(core::ProjectionOfPointOnLine(p, l).position, p));
    }
}

TEST(UtilTest, DistanceBetweenTwoLines) {
    core::Line3 l1 = { { 1, 0, 0 }, { -1, 0, 0 } };
    core::Line3 l2 = { { 0, 1, 1 }, { 0, -1, 1 } };
    core::Line3 l3 = { { 0, 2, 1 }, { 0, 3, 1 } };
    ASSERT_DOUBLE_EQ(1, core::DistanceBetweenTwoLines(l1, l2).first);
    ASSERT_DOUBLE_EQ(core::norm(l3.first - l1.center()), core::DistanceBetweenTwoLines(l1, l3).first);

    core::Line3 l4 = { { 1, 1, 0 }, { -1, 1, 0 } };
    ASSERT_DOUBLE_EQ(1, core::DistanceBetweenTwoLines(l1, l4).first);

    core::Line3 l5 = { { 2, 1, 0 }, { 3, 1, 0 } };
    ASSERT_DOUBLE_EQ(core::norm(l1.first, l5.first), core::DistanceBetweenTwoLines(l1, l5).first);

    core::Line3 l6 = { { 2, 1, 0 }, { -2, 1, 0 } };
    ASSERT_DOUBLE_EQ(1, core::DistanceBetweenTwoLines(l1, l6).first);
}


TEST(UtilTest, CreateLinearSequence) {
    size_t n = 500;
    size_t low = -150, high = 100;
    std::vector<size_t> vs(n);
    core::CreateLinearSequence(vs.begin(), vs.end(), low, high);
    for (size_t i = 0; i < n; i++){
        EXPECT_EQ(low + i * (high - low) / n, vs[i]);
    }
}


TEST(UtilTest, NaiveMergeNear) {
    std::list<double> arr1;
    arr1.resize(1000);
    std::generate(arr1.begin(), arr1.end(), std::rand);
    std::vector<double> arr2(arr1.begin(), arr1.end());

    double thres = 10;
    auto gBegins1 = core::NaiveMergeNear(std::begin(arr1), std::end(arr1), std::false_type(), thres);
    auto gBegins2 = core::NaiveMergeNear(std::begin(arr2), std::end(arr2), std::true_type(), thres);
    ASSERT_EQ(gBegins1.size(), gBegins2.size());
    auto i = gBegins1.begin();
    auto j = gBegins2.begin();
    for (; i != gBegins1.end(); ++i, ++j){
        EXPECT_EQ(**i, **j);
    }
    for (auto i = gBegins2.begin(); i != gBegins2.end(); ++i){
        auto inext = std::next(i);
        auto begin = *i;
        auto end = inext == gBegins2.end() ? std::end(arr2) : *inext;
        auto beginVal = *begin;
        for (auto j = begin; j != end; ++j){
            EXPECT_NEAR(*j, beginVal, thres);
        }
    }
}

TEST(UtilTest, MinimumSpanningTree) {
    std::vector<int> verts = { 0, 1, 2, 3, 4, 5};
    std::vector<int> edges = { 0, 1, 2, 3, 4, 5, 6, 7 };
    struct EdgeProperty { int fromv, tov; double w; };
    std::vector<EdgeProperty> edgeProperties = {
        {0, 1, 0.1},
        {1, 2, 0.2},
        {0, 2, 0.5},
        {0, 5, 0.2},
        {5, 4, 0.7},
        {2, 4, 0.3},
        {3, 4, 0.1},
        {2, 3, 0.5}
    };
    
    std::vector<int> MST;
    MST.reserve(5);
    core::MinimumSpanningTree(
        verts.begin(), verts.end(), edges.begin(), edges.end(),
        std::back_inserter(MST),
        [&edgeProperties](int e){ // get vertices of edge
            return std::make_pair(edgeProperties[e].fromv, edgeProperties[e].tov); 
        },
        [&edgeProperties](int e1, int e2){ // compare weights of edges
            return edgeProperties[e1].w < edgeProperties[e2].w; 
        }
    );
    
    std::vector<int> correctMST = { 0, 1, 3, 5, 6 };
    EXPECT_TRUE(std::is_permutation(MST.begin(), MST.end(), correctMST.begin()));
}


int main(int argc, char * argv[], char * envp[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}