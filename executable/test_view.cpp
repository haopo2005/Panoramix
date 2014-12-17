#include "../src/core/feature.hpp"
#include "../src/core/utilities.hpp"
#include "../src/core/algorithms.hpp"
#include "../src/core/view.hpp"
#include "../src/vis/visualize2d.hpp"
#include "../src/vis/visualize3d.hpp"

#include "test_config.hpp"

using namespace panoramix;
using namespace test;

TEST(View, SampleViews) {

    auto panorama = cv::imread(ProjectDataDirStrings::PanoramaIndoor + "/13.jpg");
    core::ResizeToMakeHeightUnder(panorama, 800);
    auto panoView = core::CreatePanoramicView(panorama);

    std::vector<core::PerspectiveCamera> cams = {
        core::PerspectiveCamera(700, 700, 300, { 0, 0, 0 }, { 1, 0, 0 }, { 0, 0, -1 }),
        core::PerspectiveCamera(700, 700, 300, { 0, 0, 0 }, { 0, 1, 0 }, { 0, 0, -1 }),
        core::PerspectiveCamera(700, 700, 300, { 0, 0, 0 }, { -1, 0, 0 }, { 0, 0, -1 }),
        core::PerspectiveCamera(700, 700, 300, { 0, 0, 0 }, { 0, -1, 0 }, { 0, 0, -1 }),
        core::PerspectiveCamera(700, 700, 300, { 0, 0, 0 }, { 0, 0, 1 }, { 1, 0, 0 }),
        core::PerspectiveCamera(700, 700, 300, { 0, 0, 0 }, { 0, 0, -1 }, { 1, 0, 0 })
    };
    
    std::vector<core::View<core::PerspectiveCamera>> views;
    core::SampleViews(panoView, cams.begin(), cams.end(), std::back_inserter(views));

    core::SaveToDisk("./cache/test_view.View.SampleViews.panorama", panorama);
    core::SaveToDisk("./cache/test_view.View.SampleViews.views", views);

}

TEST(View, LinesGraph) {

    std::vector<core::View<core::PerspectiveCamera>> views;
    core::LoadFromDisk("./cache/test_view.View.SampleViews.views", views);

    std::vector<core::Vec3> vanishingPoints;
    std::vector<core::LinesGraph> linesGraphs;
    core::EstimateVanishingPointsAndBuildLinesGraphs(views, vanishingPoints, linesGraphs, 10, 100, 5);

    for (int i = 0; i < views.size(); i++){
        vis::Visualizer2D viz(views[i].image);
        viz << vis::manip2d::SetColorTable(vis::ColorTableDescriptor::RGB)
            << vis::manip2d::SetThickness(2);
        for (auto & l : linesGraphs[i].elements<0>()){
            viz << l.data.line;
        }
        viz << vis::manip2d::SetThickness(1);
        for (auto & r : linesGraphs[i].elements<1>()){
            auto & line1 = linesGraphs[i].data(r.topo.lowers.front()).line;
            auto & line2 = linesGraphs[i].data(r.topo.lowers.back()).line;
            auto p1 = core::DistanceFromPointToLine(r.data.relationCenter, line1.component).second.position;
            auto p2 = core::DistanceFromPointToLine(r.data.relationCenter, line2.component).second.position;
            if (r.data.type == core::LineRelationData::Incidence){
                viz << vis::manip2d::SetColor(vis::ColorTag::Black);
                EXPECT_LT(core::Distance(p1, r.data.relationCenter), 100);
                EXPECT_LT(core::Distance(p2, r.data.relationCenter), 100);
            }
            else {
                viz << vis::manip2d::SetColor(vis::ColorTag::Cyan);
                EXPECT_LT(core::Distance(p1, r.data.relationCenter), 100);
                EXPECT_LT(core::Distance(p2, r.data.relationCenter), 100);
            }            
            viz << core::Line2(p1, r.data.relationCenter) << core::Line2(p2, r.data.relationCenter);
        }
        viz << vis::manip2d::Show();
    }

    core::SaveToDisk("./cache/test_view.View.LinesGraph.linesGraphs", linesGraphs);
    core::SaveToDisk("./cache/test_view.View.LinesGraph.vanishingPoints", vanishingPoints);

   


}

TEST(View, RegionsGraph) {

    std::vector<core::View<core::PerspectiveCamera>> views;
    std::vector<core::LinesGraph> linesGraphs;
    core::LoadFromDisk("./cache/test_view.View.SampleViews.views", views);
    core::LoadFromDisk("./cache/test_view.View.LinesGraph.linesGraphs", linesGraphs);

    core::SegmentationExtractor seg;

    std::vector<core::Imagei> segmentedRegionsArray;
    std::vector<core::RegionsGraph> regionsGraphs;

    for (int i = 0; i < views.size(); i++){
        auto & v = views[i];
        std::vector<core::Line2> lines;
        for (auto & ld : linesGraphs[i].elements<0>()){
            auto line = ld.data.line.component;
            line.first -= core::normalize(line.direction()) * 100.0;
            line.second += core::normalize(line.direction()) * 100.0;
            lines.push_back(line);
        }
        auto segmentedRegions = seg(v.image, lines).first;
        int samplePointsOnBoundariesSum = 0;
        auto regions = core::CreateRegionsGraph(segmentedRegions, 10, 3);
        for (auto & r : regions.elements<1>()){
            for (auto & ps : r.data.sampledPoints){
                samplePointsOnBoundariesSum += ps.size();
            }
        }
        regionsGraphs.push_back(std::move(regions));
        segmentedRegionsArray.push_back(segmentedRegions);
    }

    core::SaveToDisk("./cache/test_view.View.RegionsGraph.regionsGraphs", regionsGraphs);
    core::SaveToDisk("./cache/test_view.View.RegionsGraph.segmentedRegionsArray", segmentedRegionsArray);
}


TEST(View, RegionLineConnections){

    std::vector<core::View<core::PerspectiveCamera>> views;
    std::vector<core::Imagei> segmentedRegionsArray;
    std::vector<core::RegionsGraph> regionsGraphs;
    std::vector<core::Vec3> vanishingPoints;
    std::vector<core::LinesGraph> linesGraphs;

    core::LoadFromDisk("./cache/test_view.View.SampleViews.views", views);
    core::LoadFromDisk("./cache/test_view.View.RegionsGraph.segmentedRegionsArray", segmentedRegionsArray);
    core::LoadFromDisk("./cache/test_view.View.RegionsGraph.regionsGraphs", regionsGraphs);
    core::LoadFromDisk("./cache/test_view.View.LinesGraph.linesGraphs", linesGraphs);
    core::LoadFromDisk("./cache/test_view.View.LinesGraph.vanishingPoints", vanishingPoints);

    std::vector<std::map<std::pair<core::RegionHandle, core::LineHandle>, std::vector<core::Point2>>> 
        regionLineConnectionsArray(views.size());
    for (int i = 0; i < views.size(); i++){
        
        vis::Visualizer2D vis(views[i].image);

        // visualize connections between regions and lines
        std::unordered_map<int, vis::Visualizer2D, core::HandleHasher<core::AtLevel<0>>> vizs;

        //vis::Visualizer2D viz(vd.data.regionNet->image);
        int height = views[i].image.rows;
        int width = views[i].image.cols;

        core::ImageWithType<core::Vec3b> coloredOutput(segmentedRegionsArray[i].size());
        vis::ColorTable colors = vis::CreateRandomColorTableWithSize(regionsGraphs[i].internalElements<0>().size());
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                coloredOutput(cv::Point(x, y)) =
                    vis::ToVec3b(colors[segmentedRegionsArray[i](cv::Point(x, y))]);
            }
        }
        vizs[i].setImage(views[i].image);
        vizs[i].params.alphaForNewImage = 0.5;
        vizs[i] << coloredOutput;
        vizs[i] << vis::manip2d::SetColorTable(vis::ColorTableDescriptor::RGB);

        regionLineConnectionsArray[i] = core::RecognizeRegionLineConnections(segmentedRegionsArray[i], linesGraphs[i], 10);

        for (auto & l : regionLineConnectionsArray[i]) {
            auto & rh = l.first.first;
            auto & lh = l.first.second;
            auto & cline2 = linesGraphs[i].data(lh).line;
            auto & cam = views[i].camera;
            auto & viz = vizs[i];

            viz << vis::manip2d::SetColorTable(vis::ColorTableDescriptor::RGB) 
                << vis::manip2d::SetThickness(1) 
                << cline2;
            viz << vis::manip2d::SetColor(vis::ColorTag::Black)
                << vis::manip2d::SetThickness(1);
            auto & regionCenter = regionsGraphs[i].data(rh).center;
            for (auto & p : l.second) {
                viz << core::Line2(regionCenter, p);
            }
        }

        for (auto & viz : vizs) {
            viz.second << vis::manip2d::Show();
        }
    }

    core::SaveToDisk("./cache/test_view.View.RegionLineConnections.regionLineConnectionsArray", regionLineConnectionsArray);
}


TEST(View, ConstraintsAcrossViews){

    std::vector<core::View<core::PerspectiveCamera>> views;
    std::vector<core::Imagei> segmentedRegionsArray;
    std::vector<core::RegionsGraph> regionsGraphs;
    std::vector<core::Vec3> vanishingPoints;
    std::vector<core::LinesGraph> linesGraphs;

    core::LoadFromDisk("./cache/test_view.View.SampleViews.views", views);
    core::LoadFromDisk("./cache/test_view.View.RegionsGraph.segmentedRegionsArray", segmentedRegionsArray);
    core::LoadFromDisk("./cache/test_view.View.RegionsGraph.regionsGraphs", regionsGraphs);
    core::LoadFromDisk("./cache/test_view.View.LinesGraph.linesGraphs", linesGraphs);
    core::LoadFromDisk("./cache/test_view.View.LinesGraph.vanishingPoints", vanishingPoints);

    auto regionOverlappingsAcrossViews =
        core::RecognizeRegionOverlappingsAcrossViews(views, regionsGraphs);
    auto lineIncidencesAcrossViews = 
        core::RecognizeLineIncidencesAcrossViews(views, linesGraphs, M_PI / 4, 20.0 / 300.0);

    core::SaveToDisk("./cache/test_view.View.ConstraintsAcrossViews.regionOverlappingsAcrossViews", regionOverlappingsAcrossViews);
    core::SaveToDisk("./cache/test_view.View.ConstraintsAcrossViews.lineIncidencesAcrossViews", lineIncidencesAcrossViews);

}

void VisualizeMixedGraph(const core::Image & panorama, 
    const core::MixedGraph & mg,
    const std::vector<core::MGPatch> & patches, 
    const std::vector<core::Vec3> & vps,
    bool doModal){
    
    vis::Visualizer3D viz;
    viz << vis::manip3d::SetDefaultColorTable(vis::ColorTableDescriptor::RGB);
    
    std::vector<vis::SpatialProjectedPolygon> spps;
    std::vector<core::Classified<core::Line3>> lines;

    for (auto & patch : patches){
        for (auto & uhv : patch.uhs){
            auto uh = uhv.first;
            auto & v = mg.data(uh);
            if (v.type == core::MGUnary::Region){
                auto & region = v;
                vis::SpatialProjectedPolygon spp;
                // filter corners
                core::ForeachCompatibleWithLastElement(region.normalizedCorners.begin(), region.normalizedCorners.end(), 
                    std::back_inserter(spp.corners),
                    [](const core::Vec3 & a, const core::Vec3 & b) -> bool {
                    return core::AngleBetweenDirections(a, b) > M_PI / 100.0;
                });
                if (spp.corners.size() < 3)
                    continue;

                spp.projectionCenter = core::Point3(0, 0, 0);
                spp.plane = core::PlaneOfMGUnary(region, vps, uhv.second);
                spps.push_back(std::move(spp));
            }
            else if (v.type == core::MGUnary::Line){
                auto & line = v;
                lines.push_back(core::ClassifyAs(core::LineOfMGUnary(line, vps, uhv.second), uhv.second.claz));
            }
        }
    }

    viz << vis::manip3d::Begin(spps) << vis::manip3d::SetTexture(panorama) << vis::manip3d::End;
    viz << vis::manip3d::SetDefaultLineWidth(5.0) << lines;
    viz << vis::manip3d::SetDefaultForegroundColor(vis::ColorTag::Black);

    std::vector<core::Line3> connectionLines;
    for (auto & patch : patches){
        for (auto & bhv : patch.bhs){
            auto bh = bhv.first;
            auto & v = bhv.second;
            auto & samples = mg.data(bh).normalizedAnchors;
            for (int i = 0; i < samples.size(); i++){
                connectionLines.emplace_back(normalize(samples[i]) * v.sampleDepthsOnRelatedUnaries.front()[i],
                    normalize(samples[i]) * v.sampleDepthsOnRelatedUnaries.back()[i]);
            }
        }
    }

    viz << vis::manip3d::SetDefaultForegroundColor(vis::ColorTag::DarkGray) 
        << vis::manip3d::SetDefaultLineWidth(1.0)
        << connectionLines;
    viz << vis::manip3d::Show(doModal);

}


TEST(MixedGraph, Build) {

    std::vector<core::View<core::PerspectiveCamera>> views;
    std::vector<core::RegionsGraph> regionsGraphs;
    std::vector<core::LinesGraph> linesGraphs;
    std::vector<core::Vec3> vanishingPoints;
    core::Image panorama;

    core::ComponentIndexHashMap<std::pair<core::RegionIndex, core::RegionIndex>, double> regionOverlappingsAcrossViews;
    core::ComponentIndexHashMap<std::pair<core::LineIndex, core::LineIndex>, core::Vec3> lineIncidencesAcrossViews;
    std::vector<std::map<std::pair<core::RegionHandle, core::LineHandle>, std::vector<core::Point2>>>
        regionLineConnectionsArray;

    core::LoadFromDisk("./cache/test_view.View.SampleViews.panorama", panorama);
    core::LoadFromDisk("./cache/test_view.View.SampleViews.views", views);
    core::LoadFromDisk("./cache/test_view.View.RegionsGraph.regionsGraphs", regionsGraphs);
    core::LoadFromDisk("./cache/test_view.View.LinesGraph.linesGraphs", linesGraphs);
    core::LoadFromDisk("./cache/test_view.View.LinesGraph.vanishingPoints", vanishingPoints);

    core::LoadFromDisk("./cache/test_view.View.ConstraintsAcrossViews.regionOverlappingsAcrossViews", regionOverlappingsAcrossViews);
    core::LoadFromDisk("./cache/test_view.View.ConstraintsAcrossViews.lineIncidencesAcrossViews", lineIncidencesAcrossViews);
    core::LoadFromDisk("./cache/test_view.View.RegionLineConnections.regionLineConnectionsArray", regionLineConnectionsArray);

    core::MGUnaryVarTable unaryVars;
    core::MGBinaryVarTable binaryVars;

    core::MixedGraph mg = core::BuildMixedGraph(views, regionsGraphs, linesGraphs, 
        regionOverlappingsAcrossViews, lineIncidencesAcrossViews, regionLineConnectionsArray, vanishingPoints, 
        unaryVars, binaryVars, 1.0);

    core::SaveToDisk("./cache/test_view.MixedGraph.Build.mg", mg);
    core::SaveToDisk("./cache/test_view.MixedGraph.Build.unaryVars", unaryVars);
    core::SaveToDisk("./cache/test_view.MixedGraph.Build.binaryVars", binaryVars);  

}


TEST(MixedGraph, BasicOptimization) {

    core::Image panorama;
    std::vector<core::Vec3> vanishingPoints;

    core::MixedGraph mg;
    core::MGUnaryVarTable unaryVars;
    core::MGBinaryVarTable binaryVars;

    core::LoadFromDisk("./cache/test_view.View.SampleViews.panorama", panorama);
    core::LoadFromDisk("./cache/test_view.View.LinesGraph.vanishingPoints", vanishingPoints);

    core::LoadFromDisk("./cache/test_view.MixedGraph.Build.mg", mg);
    core::LoadFromDisk("./cache/test_view.MixedGraph.Build.unaryVars", unaryVars);
    core::LoadFromDisk("./cache/test_view.MixedGraph.Build.binaryVars", binaryVars);

    core::InitializeUnaryVarDepths(unaryVars, 1.0);
    {
        core::MGBinaryHandle bh(74);
        core::MGPatch patch = core::MakePatchOnBinary(mg, bh, unaryVars, binaryVars);

        core::MGPatchDepthsOptimizer pdo(mg, patch, vanishingPoints, false);

        double errBefore = core::AverageBinaryDistanceOfPatch(patch);// / core::AverageDepthOfPatch(starPatch);
        bool feasible = pdo.optimize();
        double errAfter = core::AverageBinaryDistanceOfPatch(patch);// / core::AverageDepthOfPatch(starPatch);

        EXPECT_TRUE(feasible);
        EXPECT_GE(errBefore, errAfter);
        if (errBefore < errAfter){
            auto t = mg.data(bh).type;
        }
        if (!feasible){
            auto t = mg.data(bh).type;
        }
    }

}


TEST(MixedGraph, OptimizateBinaryPatch) {

    core::Image panorama;
    std::vector<core::Vec3> vanishingPoints;

    core::MixedGraph mg;
    core::MGUnaryVarTable unaryVars;
    core::MGBinaryVarTable binaryVars;

    core::LoadFromDisk("./cache/test_view.View.SampleViews.panorama", panorama);
    core::LoadFromDisk("./cache/test_view.View.LinesGraph.vanishingPoints", vanishingPoints);

    core::LoadFromDisk("./cache/test_view.MixedGraph.Build.mg", mg);
    core::LoadFromDisk("./cache/test_view.MixedGraph.Build.unaryVars", unaryVars);
    core::LoadFromDisk("./cache/test_view.MixedGraph.Build.binaryVars", binaryVars);

    
    // test small patches
    // mosek
    for (auto & b : mg.elements<1>()){
        core::InitializeUnaryVarDepths(unaryVars, 1.0);
        core::MGPatch patch = core::MakePatchOnBinary(mg, b.topo.hd, unaryVars, binaryVars);
        auto oldPatch = patch;        
        core::MGPatchDepthsOptimizer pdo(mg, patch, vanishingPoints, false);

        auto & uh1 = patch.uhs.begin()->first;
        auto & uh2 = std::next(patch.uhs.begin())->first;
        auto & ud1 = mg.data(uh1);
        auto & ud2 = mg.data(uh2);
        auto & uvar1 = unaryVars[uh1];
        auto & uvar2 = unaryVars[uh2];

        auto & bd = mg.data(patch.bhs.begin()->first);

        double errBefore = core::AverageBinaryDistanceOfPatch(patch);// / core::AverageDepthOfPatch(starPatch);
        bool feasible = pdo.optimize();
        double errAfter = core::AverageBinaryDistanceOfPatch(patch);// / core::AverageDepthOfPatch(starPatch);

        if (!feasible){
            EXPECT_GT(bd.normalizedAnchors.size(), 1);
            if (bd.type == core::MGBinary::RegionRegionConnection || 
                bd.type == core::MGBinary::RegionRegionOverlapping){
                ASSERT_NE(uvar1.claz, uvar2.claz);
            }
        }
        else{
            if (bd.normalizedAnchors.size() > 1){
                if (bd.type == core::MGBinary::RegionRegionConnection || core::MGBinary::RegionRegionOverlapping)
                    ASSERT_EQ(uvar1.claz, uvar2.claz);
                else if (bd.type == core::MGBinary::RegionLineConnection)
                    ASSERT_NE(uvar1.claz, uvar2.claz);
            }
            EXPECT_GE(errBefore, errAfter);
        }
    }

}


TEST(MixedGraph, OptimizateBinaryPatchEigen) {

    core::Image panorama;
    std::vector<core::Vec3> vanishingPoints;

    core::MixedGraph mg;
    core::MGUnaryVarTable unaryVars;
    core::MGBinaryVarTable binaryVars;

    core::LoadFromDisk("./cache/test_view.View.SampleViews.panorama", panorama);
    core::LoadFromDisk("./cache/test_view.View.LinesGraph.vanishingPoints", vanishingPoints);

    core::LoadFromDisk("./cache/test_view.MixedGraph.Build.mg", mg);
    core::LoadFromDisk("./cache/test_view.MixedGraph.Build.unaryVars", unaryVars);
    core::LoadFromDisk("./cache/test_view.MixedGraph.Build.binaryVars", binaryVars);

    // eigen
    for (auto & b : mg.elements<1>()){
        core::InitializeUnaryVarDepths(unaryVars, 1.0);
        core::MGPatch patch = core::MakePatchOnBinary(mg, b.topo.hd, unaryVars, binaryVars);
        auto oldPatch = patch;
        core::MGPatchDepthsOptimizer pdo(mg, patch, vanishingPoints, false, core::MGPatchDepthsOptimizer::Eigen);

        auto & uh1 = patch.uhs.begin()->first;
        auto & uh2 = std::next(patch.uhs.begin())->first;
        auto & ud1 = mg.data(uh1);
        auto & ud2 = mg.data(uh2);
        auto & uvar1 = unaryVars[uh1];
        auto & uvar2 = unaryVars[uh2];

        auto & bd = mg.data(patch.bhs.begin()->first);

        double errBefore = core::AverageBinaryDistanceOfPatch(patch, 2) / core::AverageDepthOfPatch(patch);
        bool feasible = pdo.optimize();
        double errAfter = core::AverageBinaryDistanceOfPatch(patch, 2) / core::AverageDepthOfPatch(patch);

        ASSERT_TRUE(feasible);
        EXPECT_GE(errBefore, errAfter);
    }

}



TEST(MixedGraph, OptimizateStarPatch){

    core::Image panorama;
    std::vector<core::Vec3> vanishingPoints;

    core::MixedGraph mg;
    core::MGUnaryVarTable unaryVars;
    core::MGBinaryVarTable binaryVars;

    core::LoadFromDisk("./cache/test_view.View.SampleViews.panorama", panorama);
    core::LoadFromDisk("./cache/test_view.View.LinesGraph.vanishingPoints", vanishingPoints);

    core::LoadFromDisk("./cache/test_view.MixedGraph.Build.mg", mg);
    core::LoadFromDisk("./cache/test_view.MixedGraph.Build.unaryVars", unaryVars);
    core::LoadFromDisk("./cache/test_view.MixedGraph.Build.binaryVars", binaryVars);

    for (auto & u : mg.elements<0>()){
        core::InitializeUnaryVarDepths(unaryVars, 1.0);
        core::MGPatch starPatch = core::MakeStarPatchAroundUnary(mg, u.topo.hd, unaryVars, binaryVars);
        core::MGPatchDepthsOptimizer pdo(mg, starPatch, vanishingPoints, false, core::MGPatchDepthsOptimizer::Eigen);

        double errBefore = core::AverageBinaryDistanceOfPatch(starPatch) / core::AverageDepthOfPatch(starPatch);
        pdo.optimize();
        double errAfter = core::AverageBinaryDistanceOfPatch(starPatch) / core::AverageDepthOfPatch(starPatch);

        EXPECT_GE(errBefore, errAfter);
    }

}


TEST(MixedGraph, NaiveHolisticOptimization) {

    core::Image panorama;
    std::vector<core::Vec3> vanishingPoints;

    core::MixedGraph mg;
    core::MGUnaryVarTable unaryVars;
    core::MGBinaryVarTable binaryVars;

    core::LoadFromDisk("./cache/test_view.View.SampleViews.panorama", panorama);
    core::LoadFromDisk("./cache/test_view.View.LinesGraph.vanishingPoints", vanishingPoints);

    core::LoadFromDisk("./cache/test_view.MixedGraph.Build.mg", mg);
    core::LoadFromDisk("./cache/test_view.MixedGraph.Build.unaryVars", unaryVars);
    core::LoadFromDisk("./cache/test_view.MixedGraph.Build.binaryVars", binaryVars);

    core::InitializeUnaryVarDepths(unaryVars, 1.0);

    std::vector<core::MGPatch> naivePatches =
        core::SplitMixedGraphIntoPatches(mg, unaryVars, binaryVars);

    for (auto & patch : naivePatches){
        VisualizeMixedGraph(panorama, mg, { patch }, vanishingPoints, false);
        core::MGPatchDepthsOptimizer pdo(mg, patch, vanishingPoints, false, 
            core::MGPatchDepthsOptimizer::Eigen);

        double distBefore = core::AverageBinaryDistanceOfPatch(patch) / core::AverageDepthOfPatch(patch);
        pdo.optimize();
        double distAfter = core::AverageBinaryDistanceOfPatch(patch) / core::AverageDepthOfPatch(patch);

        EXPECT_GE(distBefore, distAfter);
        VisualizeMixedGraph(panorama, mg, { patch }, vanishingPoints, true);
    }

    core::SaveToDisk("./cache/test_view.MixedGraph.Build.NaiveHolisticOptimization", naivePatches);

}


TEST(MixedGraph, LinesOptimization) {

    core::Image panorama;
    std::vector<core::Vec3> vanishingPoints;

    core::MixedGraph mg;
    core::MGUnaryVarTable unaryVars;
    core::MGBinaryVarTable binaryVars;

    core::LoadFromDisk("./cache/test_view.View.SampleViews.panorama", panorama);
    core::LoadFromDisk("./cache/test_view.View.LinesGraph.vanishingPoints", vanishingPoints);

    core::LoadFromDisk("./cache/test_view.MixedGraph.Build.mg", mg);
    core::LoadFromDisk("./cache/test_view.MixedGraph.Build.unaryVars", unaryVars);
    core::LoadFromDisk("./cache/test_view.MixedGraph.Build.binaryVars", binaryVars);

    std::vector<core::MGPatch> naivePatches;
    core::LoadFromDisk("./cache/test_view.MixedGraph.Build.NaiveHolisticOptimization", naivePatches);

    std::vector<core::MGPatch> linePatches;
    std::vector<core::MGPatch> lineMSTPatches;

    for (auto & patch : naivePatches){
        std::vector<core::MGPatch> subPatches = core::SplitPatch(mg, patch, [&mg](core::MGBinaryHandle bh){
            return mg.data(bh).type == core::MGBinary::LineLineIncidence ||
                mg.data(bh).type == core::MGBinary::LineLineIntersection;
        });
        for (auto & subp : subPatches){
            if (mg.data(subp.uhs.begin()->first).type == core::MGUnary::Line){
                linePatches.push_back(std::move(subp));
            }
        }
    }

    for (auto & patch : linePatches){
        lineMSTPatches.push_back(core::MinimumSpanningTreePatch(mg, patch));
    }
    
    for (auto & patch : linePatches){
        core::MGPatchDepthsOptimizer pdo(mg, patch, vanishingPoints, false, core::MGPatchDepthsOptimizer::Eigen);
        pdo.optimize();
    }

    for (auto & patch : lineMSTPatches){
        core::MGPatchDepthsOptimizer pdo(mg, patch, vanishingPoints, false, core::MGPatchDepthsOptimizer::Eigen);
        pdo.optimize();
    }

    VisualizeMixedGraph(panorama, mg, linePatches, vanishingPoints, false);
    VisualizeMixedGraph(panorama, mg, lineMSTPatches, vanishingPoints, true);

    core::SaveToDisk("./cache/test_view.MixedGraph.Build.LinesOptimization.linePatches", linePatches);
    core::SaveToDisk("./cache/test_view.MixedGraph.Build.LinesOptimization.lineMSTPatches", lineMSTPatches);

}








int main(int argc, char * argv[], char * envp[]) {
    srand(clock());
    testing::InitGoogleTest(&argc, argv);
    testing::GTEST_FLAG(filter) = "MixedGraph.LinesOptimization";
    return RUN_ALL_TESTS();
}
