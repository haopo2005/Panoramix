#include "core/basic_types.hpp"
#include "core/mixed_graph.hpp"
#include "vis/visualize2d.hpp"

using namespace panoramix;

int main(int argc, char ** argv) {
    std::string filename;
    if (argc < 2){
        std::cout << "no input" << std::endl;
        filename = PROJECT_TEST_DATA_DIR_STR"/panorama/indoor/13.jpg";
        //filename = PROJECT_TEST_DATA_DIR_STR"/normal/room.png";
    }
    else{
        filename = argv[1];
    }
    std::cout << "filename: " << filename << std::endl;

    core::Image image = cv::imread(filename);
    core::ResizeToMakeHeightUnder(image, 600);

    bool isPanorama = core::MayBeAPanorama(image);
    std::cout << "is panorama: " << (isPanorama ? "yes" : "no") << std::endl;

    if (isPanorama){
        core::View<core::PanoramicCamera> view;

        std::vector<core::PerspectiveCamera> cams;
        std::vector<std::vector<core::Classified<core::Line2>>> lines;
        std::vector<core::Vec3> vps;

        std::vector<core::PerspectiveCamera> hCams; // horizontal cams
        std::vector<core::GeometricContextEstimator::Feature> gcs;

        core::Imagei segmentedImage;

        core::MixedGraph mg;
        core::MixedGraphPropertyTable props;


        if (1){
            view = core::CreatePanoramicView(image);

            // collect lines in each view
            cams = core::CreateCubicFacedCameras(view.camera);
            lines.resize(cams.size());
            for (int i = 0; i < cams.size(); i++){
                auto pim = view.sampled(cams[i]).image;
                core::LineSegmentExtractor lineExtractor;
                lineExtractor.params().algorithm = core::LineSegmentExtractor::LSD;
                auto ls = lineExtractor(pim);
                lines[i].reserve(ls.size());
                for (auto & l : ls){
                    lines[i].push_back(core::ClassifyAs(l, -1));
                }
            }

            // estimate vp
            vps = core::EstimateVanishingPointsAndClassifyLines(cams, lines);

            // collect gcs in each horizontal view
            hCams = core::CreateHorizontalPerspectiveCameras(view.camera, 8);
            gcs.resize(hCams.size());
            for (int i = 0; i < hCams.size(); i++){
                auto pim = view.sampled(hCams[i]).image;
                core::GeometricContextEstimator gce;
                gcs[i] = gce(pim, core::SceneClass::Indoor);
            }


            // append lines
            for (int i = 0; i < cams.size(); i++){
                core::AppendLines(mg, lines[i], cams[i], vps);
            }

            // append regions
            // make 3d lines
            std::vector<core::Line3> line3ds;
            for (int i = 0; i < cams.size(); i++){
                for (auto & l : lines[i]){
                    line3ds.emplace_back(normalize(cams[i].spatialDirection(l.component.first)),
                        normalize(cams[i].spatialDirection(l.component.second)));
                }
            }
            core::SegmentationExtractor segmenter;
            segmenter.params().algorithm = core::SegmentationExtractor::GraphCut;
            segmenter.params().c = 100.0;
            segmentedImage = segmenter(view.image, line3ds, view.camera).first;
            //vis::Visualizer2D(segmentedImage) << vis::manip2d::Show();
            //int segmentsNum = core::MinMaxValOfImage(segmentedImage).second + 1;

            core::AppendRegions(mg, segmentedImage, view.camera, 0.01, 0.02, 3, 2);

            core::SaveToDisk("./cache/all", view, cams, lines, vps, hCams, gcs, segmentedImage, mg, props);
        }
        else{
            core::LoadFromDisk("./cache/all", view, cams, lines, vps, hCams, gcs, segmentedImage, mg, props);
        }

        // optimize
        if (1){
            props = core::MakeMixedGraphPropertyTable(mg, vps, gcs, hCams);
            core::InitializeVariables(mg, props);
            core::Visualize(view, mg, props);

            double scoreBefore = core::ComputeScore(mg, props);
            std::cout << "score before = " << scoreBefore << std::endl;

            core::SolveVariablesUsingInversedDepths(mg, props);

            core::SaveToDisk("./cache/mgp", mg, props);
        }
        else{
            core::LoadFromDisk("./cache/mgp", mg, props);
        }

        core::NormalizeVariables(mg, props);

        double scoreAfter = core::ComputeScore(mg, props);
        std::cout << "score after = " << scoreAfter << std::endl;

        core::Visualize(view, mg, props);

    }
    else {
        std::vector<core::Classified<core::Line2>> lines;
        std::vector<core::Vec3> vps;
        double focal;
        core::View<core::PerspectiveCamera> view;
        core::MixedGraph mg;
        std::vector<core::Line2> pureLines;
        core::Imagei segmentedImage;
        core::MixedGraphPropertyTable props;

        core::VanishingPointsDetector::Params vpdParams;
        vpdParams.algorithm = core::VanishingPointsDetector::Naive;
        view = core::CreatePerspectiveView(image, core::Point3(0, 0, 0), core::Point3(1, 0, 0), core::Point3(0, 0, -1),
            core::LineSegmentExtractor(), core::VanishingPointsDetector(vpdParams), nullptr, &lines, &vps, &focal);
        vis::Visualizer2D(view.image) << vis::manip2d::SetColorTable(vis::ColorTable(vis::ColorTableDescriptor::RGB).appendRandomizedColors(vps.size() - 3))
            << vis::manip2d::SetThickness(2.0)
            << lines << vis::manip2d::Show();       

        // append lines
        core::AppendLines(mg, lines, view.camera, vps);

        // append regions
        core::SegmentationExtractor segmenter;
        segmenter.params().algorithm = core::SegmentationExtractor::GraphCut;
        segmenter.params().c = 100.0;
        pureLines.resize(lines.size());
        for (int i = 0; i < lines.size(); i++)
            pureLines[i] = lines[i].component;
        int segmentsNum = 0;
        std::tie(segmentedImage, segmentsNum) = segmenter(image, pureLines);    
        vis::Visualizer2D::Params vParams;
        vParams.colorTable = vis::CreateRandomColorTableWithSize(segmentsNum);
        vis::Visualizer2D(segmentedImage, vParams)
            << vis::manip2d::Show();

        core::AppendRegions(mg, segmentedImage, view.camera, 0.001, 0.001, 3, 1);

        // optimize
        props = core::MakeMixedGraphPropertyTable(mg, vps);
        core::InitializeVariables(mg, props);
        core::NormalizeVariables(mg, props);
        std::cout << "score = " << core::ComputeScore(mg, props) << std::endl;
        core::Visualize(view, mg, props);       

        core::SolveVariablesUsingInversedDepths(mg, props);
        core::NormalizeVariables(mg, props);
        std::cout << "score = " << core::ComputeScore(mg, props) << std::endl;
        core::Visualize(view, mg, props);

        //core::SolveVariablesUsingNormalDepths(mg, props);
        //core::NormalizeVariables(mg, props);
        //std::cout << "score = " << core::ComputeScore(mg, props) << std::endl;
        //core::Visualize(view, mg, props);

        core::SaveToDisk("./cache/all", lines, vps, focal, view, mg, pureLines, segmentedImage, props);
        core::NormalizeVariables(mg, props);
        core::Visualize(view, mg, props);
    }

    return 0;
}