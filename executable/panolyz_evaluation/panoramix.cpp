#include <chrono>

#include "panoramix.hpp"

template <class T>
double ElapsedInMS(const T & start) {
    return std::chrono::duration_cast<std::chrono::duration<double, std::milli>>
        (std::chrono::system_clock::now() - start).count();
}


const std::string PanoramixOptions::parseOption(bool b) {
    return b ? "_on" : "_off";
}

std::string PanoramixOptions::algorithmOptionsTag() const {
    std::stringstream ss;
    ss << "_LayoutVersion" << LayoutVersion
        << parseOption(useWallPrior)
        << parseOption(usePrincipleDirectionPrior)
        << parseOption(useGeometricContextPrior)
        << parseOption(useGTOcclusions)
        << parseOption(looseLinesSecondTime)
        << parseOption(looseSegsSecondTime)
        << parseOption(restrictSegsSecondTime);
    return ss.str();
}

std::string PanoramixOptions::identityOfImage(const std::string & impath) const {
    return impath + algorithmOptionsTag();
}


void PanoramixOptions::print() const {
    std::cout << "##############################" << std::endl;
    std::cout << " useWallPrior = " << useWallPrior << std::endl;
    std::cout << " usePrincipleDirectionPrior = " << usePrincipleDirectionPrior << std::endl;
    std::cout << " useGeometricContextPrior = " << useGeometricContextPrior << std::endl;
    std::cout << " useGTOcclusions = " << useGTOcclusions << std::endl;
    std::cout << " looseLinesSecondTime = " << looseLinesSecondTime << std::endl;
    std::cout << " looseSegsSecondTime = " << looseSegsSecondTime << std::endl;
    std::cout << " restrictSegsSecondTime = " << restrictSegsSecondTime << std::endl;
    std::cout << "------------------------------" << std::endl;
    std::cout << " refresh_preparation = " << refresh_preparation << std::endl;
    std::cout << " refresh_mg_init = " << refresh_mg_init << std::endl;
    std::cout << " refresh_line2leftRightSegs = " << refresh_line2leftRightSegs << std::endl;
    std::cout << " refresh_mg_oriented = " << refresh_mg_oriented << std::endl;
    std::cout << " refresh_lsw = " << refresh_lsw << std::endl;
    std::cout << " refresh_mg_occdetected = " << refresh_mg_occdetected << std::endl;
    std::cout << " refresh_mg_reconstructed = " << refresh_mg_reconstructed << std::endl;
    std::cout << "##############################" << std::endl;
}



PanoramixReport::PanoramixReport() {
    time_preparation = -1;
    time_mg_init = -1;
    time_line2leftRightSegs = -1;
    time_mg_oriented = -1;
    time_lsw = -1;
    time_mg_occdetected = -1;
    time_mg_reconstructed = -1;
    succeeded = false;
}





void RunPanoramix(
    const PILayoutAnnotation & anno,
    const PanoramixOptions & options,
    misc::Matlab & matlab,
    bool showGUI) {

    PanoramixReport report;
#define START_TIME_RECORD(name) auto start_##name = std::chrono::system_clock::now()

#define STOP_TIME_RECORD(name) \
    report.time_##name = ElapsedInMS(start_##name); \
    std::cout << "refresh_"#name" time cost: " << report.time_##name << "ms" << std::endl





    options.print();
    const auto identity = options.identityOfImage(anno.impath);
    misc::SaveCache(identity, "options", options);
    misc::SaveCache(identity, "report", report);

    auto image = anno.rectifiedImage.clone();
    ResizeToHeight(image, 700);

    /// prepare things!
    View<PanoramicCamera, Image3ub> view;
    std::vector<PerspectiveCamera> cams;
    std::vector<std::vector<Classified<Line2>>> rawLine2s;
    std::vector<Classified<Line3>> line3s;
    std::vector<Vec3> vps;
    int vertVPId;
    Imagei segs;
    int nsegs;


    if (options.refresh_preparation || !misc::LoadCache(identity, "preparation", view, cams, rawLine2s, line3s, vps, vertVPId, segs, nsegs)) {
        START_TIME_RECORD(preparation);

        view = CreatePanoramicView(image);

        // collect lines in each view
        cams = CreateCubicFacedCameras(view.camera, image.rows, image.rows, image.rows * 0.4);
        std::vector<Line3> rawLine3s;
        rawLine2s.resize(cams.size());
        for (int i = 0; i < cams.size(); i++) {
            auto pim = view.sampled(cams[i]).image;
            LineSegmentExtractor lineExtractor;
            lineExtractor.params().algorithm = LineSegmentExtractor::LSD;
            auto ls = lineExtractor(pim); // use pyramid
            rawLine2s[i] = ClassifyEachAs(ls, -1);
            for (auto & l : ls) {
                rawLine3s.emplace_back(normalize(cams[i].toSpace(l.first)),
                    normalize(cams[i].toSpace(l.second)));
            }
        }
        rawLine3s = MergeLines(rawLine3s, DegreesToRadians(3), DegreesToRadians(5));

        // estimate vp
        line3s = ClassifyEachAs(rawLine3s, -1);
        vps = EstimateVanishingPointsAndClassifyLines(line3s, nullptr, true);
        vertVPId = NearestDirectionId(vps, Vec3(0, 0, 1));


        if (showGUI) {
            gui::ColorTable ctable = gui::RGBGreys;
            for (int i = 0; i < cams.size(); i++) {
                auto & cam = cams[i];
                std::vector<Classified<Line2>> lines;
                for (auto & l3 : line3s) {
                    if (!cam.isVisibleOnScreen(l3.component.first) || !cam.isVisibleOnScreen(l3.component.second)) {
                        continue;
                    }
                    auto p1 = cam.toScreen(l3.component.first);
                    auto p2 = cam.toScreen(l3.component.second);
                    lines.push_back(ClassifyAs(Line2(p1, p2), l3.claz));
                }
                auto pim = view.sampled(cams[i]).image;
                gui::AsCanvas(pim).thickness(3).colorTable(ctable).add(lines).show();
            }
        }


        // estimate segs
        nsegs = SegmentationForPIGraph(view, line3s, segs, DegreesToRadians(1));
        RemoveThinRegionInSegmentation(segs, 1, true);
        RemoveEmbededRegionsInSegmentation(segs, true);
        nsegs = DensifySegmentation(segs, true);
        assert(IsDenseSegmentation(segs));

        if (showGUI) {
            auto ctable = gui::CreateGreyColorTableWithSize(nsegs);
            ctable.randomize();
            gui::ColorTable rgb = gui::RGBGreys;
            auto canvas = gui::MakeCanvas(view.image).alpha(0.9).add(ctable(segs));
            for (auto & l : line3s) {
                static const double sampleAngle = M_PI / 100.0;
                auto & line = l.component;
                double spanAngle = AngleBetweenDirections(line.first, line.second);
                std::vector<Point2> ps; ps.reserve(spanAngle / sampleAngle);
                for (double angle = 0.0; angle <= spanAngle; angle += sampleAngle) {
                    Vec3 dir = RotateDirection(line.first, line.second, angle);
                    ps.push_back(view.camera.toScreen(dir));
                }
                for (int i = 1; i < ps.size(); i++) {
                    auto & p1 = ps[i - 1];
                    auto & p2 = ps[i];
                    if (Distance(p1, p2) >= view.image.cols / 2) {
                        continue;
                    }
                    canvas.thickness(2);
                    canvas.colorTable(rgb).add(gui::ClassifyAs(Line2(p1, p2), l.claz));
                }
            }
            canvas.show();
        }

      
        STOP_TIME_RECORD(preparation);

        // save
        misc::SaveCache(identity, "preparation", view, cams, rawLine2s, line3s, vps, vertVPId, segs, nsegs);
    }


    // gc !!!!
    std::vector<PerspectiveCamera> hcams;
    std::vector<Weighted<View<PerspectiveCamera, Image5d>>> gcs;
    Image5d gc;
    static const int hcamNum = 16;
    static const Sizei hcamScreenSize(500, 500);
    //static const Sizei hcamScreenSize(500, 700);
    static const int hcamFocal = 200;
    std::string hcamsgcsFileName;
    {
        std::stringstream ss;
        ss << "hcamsgcs_" << hcamNum << "_" << hcamScreenSize.width << "_" << hcamScreenSize.height << "_" << hcamFocal;
        hcamsgcsFileName = ss.str();
    }
    if (0 || !misc::LoadCache(anno.impath, hcamsgcsFileName, hcams, gcs)) {
        // extract gcs
        hcams = CreateHorizontalPerspectiveCameras(view.camera, hcamNum, hcamScreenSize.width, hcamScreenSize.height, hcamFocal);
        gcs.resize(hcams.size());
        for (int i = 0; i < hcams.size(); i++) {
            auto pim = view.sampled(hcams[i]);
            auto pgc = ComputeGeometricContext(matlab, pim.image, false, true);
            gcs[i].component.camera = hcams[i];
            gcs[i].component.image = pgc;
            gcs[i].score = abs(1.0 - normalize(hcams[i].forward()).dot(normalize(view.camera.up())));
        }
        misc::SaveCache(anno.impath, hcamsgcsFileName, hcams, gcs);
    }
    std::string gcmergedFileName;
    {
        std::stringstream ss;
        ss << "gc_" << hcamNum << "_" << hcamScreenSize.width << "_" << hcamScreenSize.height << "_" << hcamFocal;
        gcmergedFileName = ss.str();
    }
    if (0 || !misc::LoadCache(anno.impath, gcmergedFileName, gc)) {
        gc = Combine(view.camera, gcs).image;
        misc::SaveCache(anno.impath, gcmergedFileName, gc);
    }


    if (0) {
        std::vector<Imaged> gcChannels;
        cv::split(gc, gcChannels);
        gui::AsCanvas(ConvertToImage3d(gc)).show(1, "gc");
    }



    // build pigraph!
    PIGraph mg;
    if (options.refresh_mg_init || !misc::LoadCache(identity, "mg_init", mg)) {
        std::cout << "########## refreshing mg init ###########" << std::endl;
        START_TIME_RECORD(mg_init);
        mg = BuildPIGraph(view, vps, vertVPId, segs, line3s,
            DegreesToRadians(1), DegreesToRadians(1), DegreesToRadians(1),
            0.04, DegreesToRadians(15), DegreesToRadians(2));
        STOP_TIME_RECORD(mg_init);
        misc::SaveCache(identity, "mg_init", mg);
    }

    std::vector<std::array<std::set<int>, 2>> line2leftRightSegs;
    static const double angleDistForSegLineNeighborhood = DegreesToRadians(5);
    if (options.refresh_line2leftRightSegs || !misc::LoadCache(identity, "line2leftRightSegs", line2leftRightSegs)) {
        std::cout << "########## refreshing line2leftRightSegs ###########" << std::endl;
        START_TIME_RECORD(line2leftRightSegs);
        line2leftRightSegs = CollectSegsNearLines(mg, angleDistForSegLineNeighborhood * 2);
        STOP_TIME_RECORD(line2leftRightSegs);
        misc::SaveCache(identity, "line2leftRightSegs", line2leftRightSegs);
    }


    const auto printLines = [&mg, &anno](int delay, const std::string & saveAs) {
        static gui::ColorTable rgbColors = gui::RGBGreys;
        rgbColors.exceptionalColor() = gui::Gray;
        Image3ub image = mg.view.image.clone();
        auto canvas = gui::AsCanvas(image);
        for (auto & l : mg.lines) {
            static const double sampleAngle = M_PI / 100.0;
            auto & line = l.component;
            int claz = l.claz;
            if (claz >= mg.vps.size()) {
                claz = -1;
            }
            double spanAngle = AngleBetweenDirections(line.first, line.second);
            std::vector<Point2> ps; ps.reserve(spanAngle / sampleAngle);
            for (double angle = 0.0; angle <= spanAngle; angle += sampleAngle) {
                Vec3 dir = RotateDirection(line.first, line.second, angle);
                ps.push_back(mg.view.camera.toScreen(dir));
            }
            for (int i = 1; i < ps.size(); i++) {
                auto p1 = (ps[i - 1]);
                auto p2 = (ps[i]);
                if (Distance(p1, p2) >= mg.view.image.cols / 2) {
                    continue;
                }
                canvas.thickness(3);
                canvas.colorTable(rgbColors).add(gui::ClassifyAs(Line2(p1, p2), claz));
            }
        }
        canvas.show(delay, "lines");
        if (saveAs != "") {
            cv::imwrite(saveAs, canvas.image());
        }
    };

    const auto printOrientedSegs = [&mg, &identity](int delay, const std::string & saveAs) {
        static const gui::ColorTable randColors = gui::CreateRandomColorTableWithSize(mg.nsegs);
        static gui::ColorTable rgbColors = gui::RGBGreys;
        rgbColors.exceptionalColor() = gui::Gray;
        auto pim = Print2(mg,
            [&mg](int seg, Pixel pos) -> gui::Color {
            static const gui::ColorTable ctable = gui::ColorTableDescriptor::RGBGreys;
            auto & c = mg.seg2control[seg];
            if (!c.used) {
                return gui::Black;
            }
            if (c.orientationClaz != -1) {
                return ctable[c.orientationClaz].blendWith(gui::White, 0.3);
            }
            if (c.orientationNotClaz != -1) {
                static const int w = 10;
                if (IsBetween((pos.x + pos.y) % w, 0, w / 2 - 1)) {
                    return ctable[c.orientationNotClaz].blendWith(gui::White, 0.3);
                } else {
                    return gui::White;
                }
            }
            return gui::White;
        },
            [&mg](int lp) {
            return gui::Transparent;
        },
            [&mg](int bp) -> gui::Color {
            return gui::Gray;
        }, 1, 0);
        auto canvas = gui::AsCanvas(pim);
        canvas.show(delay, "oriented segs");
        if (saveAs != "") {
            cv::imwrite(saveAs, Image3ub(canvas.image() * 255));
        }
        return canvas.image();
    };

    const auto printPIGraph = [&mg, &identity](int delay, const std::string & saveAs) {
        static const gui::ColorTable randColors = gui::CreateRandomColorTableWithSize(mg.nsegs);
        static gui::ColorTable rgbColors = gui::RGBGreys;
        rgbColors.exceptionalColor() = gui::Gray;
        auto pim = Print2(mg,
            [&mg](int seg, Pixel pos) -> gui::Color {
            static const gui::ColorTable ctable = gui::ColorTableDescriptor::RGBGreys;
            auto & c = mg.seg2control[seg];
            if (!c.used) {
                return gui::Black;
            }
            //if (c.orientationClaz == 0) {
            //    return gui::Red;
            //}
            if (c.orientationClaz != -1) {
                return ctable[c.orientationClaz].blendWith(gui::White, 0.3);
            }
            if (c.orientationNotClaz != -1) {
                static const int w = 10;
                if (IsBetween((pos.x + pos.y) % w, 0, w / 2 - 1)) {
                    return ctable[c.orientationNotClaz].blendWith(gui::White, 0.3);
                } else {
                    return gui::White;
                }
            }
            return gui::White;
        },
            [&mg](int lp) {
            return gui::Transparent;
        },
            [&mg](int bp) -> gui::Color {
            return gui::Gray;
        }, 1, 0);
        auto canvas = gui::AsCanvas(pim);
        for (auto & l : mg.lines) {
            static const double sampleAngle = M_PI / 100.0;
            auto & line = l.component;
            int claz = l.claz;
            if (claz >= mg.vps.size()) {
                claz = -1;
            }
            double spanAngle = AngleBetweenDirections(line.first, line.second);
            std::vector<Point2> ps; ps.reserve(spanAngle / sampleAngle);
            for (double angle = 0.0; angle <= spanAngle; angle += sampleAngle) {
                Vec3 dir = RotateDirection(line.first, line.second, angle);
                ps.push_back(mg.view.camera.toScreen(dir));
            }
            for (int i = 1; i < ps.size(); i++) {
                auto p1 = ToPixel(ps[i - 1]);
                auto p2 = ToPixel(ps[i]);
                if (Distance(p1, p2) >= mg.view.image.cols / 2) {
                    continue;
                }
                gui::Color color = rgbColors[claz];
                cv::clipLine(cv::Rect(0, 0, canvas.image().cols, canvas.image().rows), p1, p2);
                cv::line(canvas.image(), p1, p2, (cv::Scalar)color / 255.0, 2);
            }
        }
        canvas.show(delay, "pi graph");
        if (saveAs != "") {
            cv::imwrite(saveAs, Image3ub(canvas.image() * 255));
        }
        return canvas.image();
    };


    if (false) {
        Image3ub lsim = printPIGraph(0, "initial_lines_segs") * 255;
        ReverseRows(lsim);
        gui::SceneBuilder sb;
        gui::ResourceStore::set("tex", lsim);
        Sphere3 sphere;
        sphere.center = Origin();
        sphere.radius = 1.0;
        sb.begin(sphere).shaderSource(gui::OpenGLShaderSourceDescriptor::XPanorama).resource("tex").end();
        sb.show(true, true);
    }



    // attach orientation constraints
    if (options.refresh_mg_oriented || !misc::LoadCache(identity, "mg_oriented", mg)) {
        std::cout << "########## refreshing mg oriented ###########" << std::endl;
        START_TIME_RECORD(mg_oriented);
        if (options.usePrincipleDirectionPrior) {
            AttachPrincipleDirectionConstraints(mg);
        }
        if (options.useWallPrior) {
            AttachWallConstraints(mg, M_PI / 60.0);
        }
        if (options.useGeometricContextPrior) {
            AttachGCConstraints(mg, gc, 0.7, 0.7, true);
        }
        STOP_TIME_RECORD(mg_oriented);
        misc::SaveCache(identity, "mg_oriented", mg);
    }

    
    const std::string folder = "F:\\GitHub\\write-papers\\papers\\a\\figure\\experiments\\";
    printLines(0, folder + identity + ".oriented_lines.png");
    printOrientedSegs(0, folder + identity + ".oriented_segs.png");
    //printPIGraph(0, "oriented_lines_segs");

    // detect occlusions
    std::vector<LineSidingWeight> lsw;
    if (options.refresh_lsw || !misc::LoadCache(identity, "lsw", lsw)) {
        std::cout << "########## refreshing lsw ###########" << std::endl;
        START_TIME_RECORD(lsw);
        if (!options.useGTOcclusions) {
            lsw = ComputeLinesSidingWeights(mg, DegreesToRadians(3), 0.2, 0.1,
                angleDistForSegLineNeighborhood);
        } else {
            lsw = ComputeLinesSidingWeightsFromAnnotation(mg, anno, DegreesToRadians(0.5), DegreesToRadians(8), 0.6);
        }
        STOP_TIME_RECORD(lsw);
        misc::SaveCache(identity, "lsw", lsw);
    }


    if (showGUI) {
        auto pim = Print(mg, ConstantFunctor<gui::Color>(gui::White),
            ConstantFunctor<gui::Color>(gui::Transparent),
            ConstantFunctor<gui::Color>(gui::Gray), 2, 0);
        auto drawLine = [&mg, &pim](const Line3 & line, const std::string & text, const gui::Color & color, bool withTeeth) {
            double angle = AngleBetweenDirections(line.first, line.second);
            std::vector<Pixel> ps;
            for (double a = 0.0; a <= angle; a += 0.04) {
                ps.push_back(ToPixel(mg.view.camera.toScreen(RotateDirection(line.first, line.second, a))));
            }
            for (int i = 1; i < ps.size(); i++) {
                auto p1 = ps[i - 1];
                auto p2 = ps[i];
                if (Distance(p1, p2) >= pim.cols / 2) {
                    continue;
                }
                cv::clipLine(cv::Rect(0, 0, pim.cols, pim.rows), p1, p2);
                cv::line(pim, p1, p2, (cv::Scalar)color / 255.0, 1);
                if (withTeeth) {
                    auto teethp = ToPixel(RightPerpendicularDirectiion(ecast<double>(p2 - p1))) + p1;
                  /*  auto tp1 = p1, tp2 = teethp;
                    cv::clipLine(cv::Rect(0, 0, pim.cols, pim.rows), tp1, tp2);
                    cv::line(pim, tp1, tp2, (cv::Scalar)color / 255.0, 1);*/
                    std::vector<Pixel> triangle = { p1, teethp, p2 };
                    cv::fillConvexPoly(pim, triangle, (cv::Scalar)color / 255.0);
                }
            }
            //cv::circle(pim, ps.back(), 2.0, (cv::Scalar)color / 255.0, 2);
            if (!text.empty()) {
                //cv::putText(pim, text, ps.back() + Pixel(5, 0), 1, 0.7, color);
            }
        };
        for (int line = 0; line < mg.nlines(); line++) {
            auto & ws = lsw[line];
            auto l = mg.lines[line].component;
            if (!ws.isOcclusion()) {
                drawLine(l, "", gui::Black, false);
            } else if (ws.onlyConnectLeft()) {
                drawLine(l, std::to_string(line), gui::Blue, true);
            } else if (ws.onlyConnectRight()) {
                drawLine(l.reversed(), std::to_string(line), gui::Blue, true);
            } else {
                drawLine(l, std::to_string(line), gui::Red, false);
            }
        }
        gui::AsCanvas(pim).show(0, "line labels");
    }


    if (options.refresh_mg_occdetected || !misc::LoadCache(identity, "mg_occdetected", mg)) {
        std::cout << "########## refreshing mg occdetected ###########" << std::endl;
        START_TIME_RECORD(mg_occdetected);
        ApplyLinesSidingWeights(mg, lsw, line2leftRightSegs, true);
        if (anno.extendedOnTop && !anno.topIsPlane) {
            DisableTopSeg(mg);
        }
        if (anno.extendedOnBottom && !anno.bottomIsPlane) {
            DisableBottomSeg(mg);
        }
        STOP_TIME_RECORD(mg_occdetected);
        misc::SaveCache(identity, "mg_occdetected", mg);
    }


    PIConstraintGraph cg;
    PICGDeterminablePart dp;
    if (options.refresh_mg_reconstructed || !misc::LoadCache(identity, "mg_reconstructed", mg, cg, dp)) {
        std::cout << "########## refreshing mg reconstructed ###########" << std::endl;
        START_TIME_RECORD(mg_reconstructed);
        cg = BuildPIConstraintGraph(mg, DegreesToRadians(1), 0.01);

        bool hasSecondTime = options.looseLinesSecondTime || options.looseSegsSecondTime || options.restrictSegsSecondTime;
        for (int i = 0; i < (hasSecondTime ? 2 : 1); i++) {
            dp = LocateDeterminablePart(cg, DegreesToRadians(3));
            
            auto start = std::chrono::system_clock::now();
            double energy = Solve(dp, cg, matlab, 5, 1e6);
            report.time_solve_lp = ElapsedInMS(start);

            if (IsInfOrNaN(energy)) {
                std::cout << "solve failed" << std::endl;
                return;
            }

            if (options.looseLinesSecondTime) {
                DisorientDanglingLines3(dp, cg, mg, 0.05, 0.1);
            }
            if (options.looseSegsSecondTime) {
                DisorientDanglingSegs3(dp, cg, mg, 0.01, 0.1);
            }
            if (options.restrictSegsSecondTime) {
                OverorientSkewSegs(dp, cg, mg, DegreesToRadians(3), DegreesToRadians(50), 1.0);
            }
        }
        STOP_TIME_RECORD(mg_reconstructed);
        misc::SaveCache(identity, "mg_reconstructed", mg, cg, dp);
    }

    if (showGUI) {
        std::pair<double, double> validRange;
        auto depthMap = SurfaceDepthMap(mg.view.camera, dp, cg, mg, &validRange);
        Imagei depthMapDisc = (depthMap - validRange.first) / (validRange.second - validRange.first) * 255;
        auto jetctable = gui::CreateJetColorTableWithSize(MinMaxValOfImage(depthMapDisc).second + 1);
        gui::AsCanvas(jetctable(depthMapDisc)).show(1, "depth map");

        auto surfaceNormalMap = SurfaceNormalMap(mg.view.camera, dp, cg, mg, true);
        auto surfaceNormalMapForShow = surfaceNormalMap.clone();
        for (auto & n : surfaceNormalMapForShow) {
            auto nn = n;
            for (int i = 0; i < mg.vps.size(); i++) {
                n[i] = abs(nn.dot(vps[i]));
            }
        }
        gui::AsCanvas(surfaceNormalMapForShow).show(0, "surface normal map");

        VisualizeReconstructionCompact(dp, cg, mg, false);
        VisualizeReconstruction(dp, cg, mg, true);
    }

    report.succeeded = true;
    misc::SaveCache(identity, "report", report);
}

bool GetPanoramixResult(
    const PILayoutAnnotation & anno,
    const PanoramixOptions & options,
    PIGraph & mg,
    PIConstraintGraph & cg,
    PICGDeterminablePart & dp) {
    auto identity = options.identityOfImage(anno.impath);
    return misc::LoadCache(identity, "mg_reconstructed", mg, cg, dp);
}

