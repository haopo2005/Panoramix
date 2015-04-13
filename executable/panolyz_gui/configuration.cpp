#include <QtOpenGL>

#include "../../src/core/algorithms.hpp"
#include "../../src/gui/qt_glue.hpp"
#include "../../src/gui/visualizers.hpp"
#include "../../src/experimental/tools.hpp"

#include "configuration.hpp"
#include "widgets.hpp"

using namespace panoramix;
using namespace core;
using namespace experimental;


using PanoView = View<PanoramicCamera>;
using PerspView = View<PerspectiveCamera>;


StepWidgetInterface * CreateBindingWidgetAndActions(DataOfType<PanoView> & pv,
    QList<QAction*> & actions, QWidget * parent) {

    return CreateImageViewer(&pv, [](DataOfType<PanoView>& pv){
        pv.lockForRead();
        QImage im = gui::MakeQImage(pv.content.image);
        pv.unlock();
        return im;
    }, parent);
}






StepWidgetInterface * CreateBindingWidgetAndActions(DataOfType<Segmentation> & segs,
    QList<QAction*> & actions, QWidget * parent) {
    
    return CreateImageViewer(&segs, [](DataOfType<Segmentation> & segs){        
        segs.lockForRead();
        auto colorTable = gui::CreateRandomColorTableWithSize(segs.content.segmentsNum);
        Image3ub im = colorTable(segs.content.segmentation);
        segs.unlock();
        return gui::MakeQImage(im);    
    }, parent);
}







StepWidgetInterface * CreateBindingWidgetAndActions(
    DataOfType<LinesAndVPs> & linesAndVPs,
    QList<QAction*> & actions, QWidget * parent){
    return CreateImageViewer(&linesAndVPs, [](DataOfType<LinesAndVPs> & linesAndVPs){
        linesAndVPs.lockForRead();
        auto & pviews = linesAndVPs.content.perspectiveViews;
        std::vector<Image> ims(pviews.size());
        gui::ColorTable ctable = gui::ColorTableDescriptor::RGBGreys;
        for (int i = 0; i < pviews.size(); i++){
            const std::vector<Classified<Line2>> & lines = linesAndVPs.content.lines[i];
            ims[i] = pviews[i].image.clone();
            for (auto & line : lines){
                cv::line(ims[i], ToPixelLoc(line.component.first), ToPixelLoc(line.component.second), ctable[line.claz]);
            }
        }
        Image concated;
        cv::hconcat(ims, concated);
        linesAndVPs.unlock();
        return gui::MakeQImage(concated);
    }, parent);
}







template <class CameraT>
StepWidgetInterface * CreateBindingWidgetAndActionsTemplated(DataOfType<ReconstructionSetup<CameraT>> & rec,
    QList<QAction*> & actions, QWidget * parent){

    class Widget : public ImageViewer<ReconstructionSetup<CameraT>> {

        void setNoPen() { _strokePen = QPen(Qt::transparent); _applyStroke = nullptr; }
        void setPen(const QColor & color, int strokeSize, 
            bool regionPropUsed, int regionPropOrientationClaz, int regionPropOrientationNotClaz) {
            _strokePen = QPen(color, strokeSize);
            _applyStroke = [=]() -> bool {
                bool changed = false;
                auto & recSetup = data();
                recSetup.lockForWrite();
                for (QPointF & p : _stroke){
                    int sz = strokeSize;
                    for (int dx = -sz; dx <= sz; dx++){
                        for (int dy = -sz; dy <= sz; dy++){
                            PixelLoc pp(p.x() + dx, p.y() + dy);
                            if (!Contains(recSetup.content.segmentation, pp))
                                continue;
                            int segId = recSetup.content.segmentation(pp);
                            RegionHandle rh(segId);
                            auto & prop = recSetup.content.controls[rh];
                            if (prop.used == regionPropUsed && 
                                prop.orientationClaz == regionPropOrientationClaz && 
                                prop.orientationNotClaz == regionPropOrientationNotClaz)
                                continue;
                            changed = true;
                            prop.used = regionPropUsed;
                            prop.orientationClaz = regionPropOrientationClaz;
                            prop.orientationNotClaz = regionPropOrientationNotClaz;
                        }
                    }
                }   
                if (changed){
                    //ResetVariables(recSetup.content.mg, recSetup.content.controls);
                }
                recSetup.unlock();
                return changed;
            };            
        }

    public:
        explicit Widget(DataOfType<ReconstructionSetup<CameraT>> * dd, QWidget * p) : ImageViewer<ReconstructionSetup<CameraT>>(dd, p) {

            setContextMenuPolicy(Qt::ActionsContextMenu);
            QActionGroup * bas = new QActionGroup(this);
            
            QAction * defaultAction = nullptr;
            connect(defaultAction = bas->addAction(tr("No Brush")), &QAction::triggered, [this](){setNoPen(); });
            {
                QAction * sep = new QAction(bas);
                sep->setSeparator(true);
                bas->addAction(sep);
            }
            connect(bas->addAction(tr("Paint Void")), &QAction::triggered, [this](){ setPen(Qt::black, 3, false, -1, -1); });
            connect(bas->addAction(tr("Paint Free Plane")), &QAction::triggered, [this](){ setPen(Qt::gray, 3, true, -1, -1); });
            connect(bas->addAction(tr("Paint Toward Direction 1")), &QAction::triggered, [this](){ setPen(Qt::red, 3, true, 0, -1); });
            connect(bas->addAction(tr("Paint Toward Direction 2")), &QAction::triggered, [this](){ setPen(Qt::red, 3, true, 1, -1);  });
            connect(bas->addAction(tr("Paint Toward Direction 3")), &QAction::triggered, [this](){ setPen(Qt::red, 3, true, 2, -1); });
            connect(bas->addAction(tr("Paint Along Direction 1")), &QAction::triggered, [this](){ setPen(Qt::red, 3, true, -1, 0); });
            connect(bas->addAction(tr("Paint Along Direction 2")), &QAction::triggered, [this](){ setPen(Qt::red, 3, true, -1, 1); });
            connect(bas->addAction(tr("Paint Along Direction 3")), &QAction::triggered, [this](){ setPen(Qt::red, 3, true, -1, 2); });
            {
                QAction * sep = new QAction(bas);
                sep->setSeparator(true);
                bas->addAction(sep);
            }
            connect(bas->addAction(tr("Draw Occlusion")), &QAction::triggered, [this](){}); 
            connect(bas->addAction(tr("Remove Occlusion")), &QAction::triggered, [this](){});

            for (auto a : bas->actions()) a->setCheckable(true);

            bas->setExclusive(true);
            defaultAction->setChecked(true);

            addActions(bas->actions());
        }
        
    public:
        virtual void refreshDataAsync() {
            if (noData())
                return;

            auto & rec = data();
            rec.lockForRead();
            QImage transparentBg(rec.content.segmentation.cols, rec.content.segmentation.rows, QImage::Format::Format_RGB888);
            {
                transparentBg.fill(Qt::transparent);
                QPainter painter(&transparentBg);
                painter.fillRect(transparentBg.rect(), Qt::BrushStyle::HorPattern);
            }

            // render
            Image3ub rendered = Image3ub::zeros(rec.content.segmentation.size());
            gui::ColorTable rgb = { gui::ColorTag::Red, gui::ColorTag::Green, gui::ColorTag::Blue };
            gui::ColorTable ymc = { gui::ColorTag::Yellow, gui::ColorTag::Magenta, gui::ColorTag::Cyan };
            double alpha = 0.5;
            for (auto it = rendered.begin(); it != rendered.end(); ++it){
                panoramix::experimental::RegionHandle rh = rec.content.segId2RegionHandles[rec.content.segmentation(it.pos())];
                if (rh.invalid())
                    continue;
                auto & prop = rec.content.controls[rh];
                if (!prop.used){
                    QRgb transPixel = transparentBg.pixel(it.pos().x, it.pos().y);
                    *it = Vec3b(qRed(transPixel), qGreen(transPixel), qBlue(transPixel));
                    continue;
                }
                gui::Color imColor = gui::ColorFromImage(rec.content.view.image, it.pos());
                if (prop.orientationClaz >= 0 && prop.orientationNotClaz == -1){
                    *it = imColor.blendWith(rgb[prop.orientationClaz], alpha);
                }
                else if (prop.orientationClaz == -1 && prop.orientationNotClaz >= 0){
                    *it = imColor.blendWith(ymc[prop.orientationNotClaz], alpha);
                }
                else{
                    *it = imColor;
                }
            }
            // render disconnected boundaries
            for (auto & b : rec.content.mg.constraints<RegionBoundaryData>()){
                for (auto & e : b.data.normalizedEdges){
                    if (e.size() <= 1) continue;
                    for (int i = 1; i < e.size(); i++){
                        auto p1 = ToPixelLoc(rec.content.view.camera.screenProjection(e[i - 1]));
                        auto p2 = ToPixelLoc(rec.content.view.camera.screenProjection(e[i]));
                        cv::clipLine(cv::Rect(0, 0, rendered.cols, rendered.rows), p1, p2);
                        cv::line(rendered, p1, p2, gui::Color(rec.content.controls[b.topo.hd].used ? gui::White : gui::Black), 1);
                    }
                }
            }            
            
            rec.unlock();
            _imageLock.lockForWrite();
            _image = gui::MakeQImage(rendered);
            _imageLock.unlock();
        }

    protected:
        virtual void mousePressEvent(QMouseEvent * e) override {
            if (_applyStroke && e->buttons() & Qt::LeftButton){
                _stroke << positionOnImage(e->pos());
                setCursor(Qt::CursorShape::OpenHandCursor);
                update();
            }
            else{
                ImageViewer<ReconstructionSetup<CameraT>>::mousePressEvent(e);
            }
        }

        virtual void mouseMoveEvent(QMouseEvent * e) override {
            if (_applyStroke && e->buttons() & Qt::LeftButton){
                auto pos = positionOnImage(e->pos());
                setCursor(Qt::CursorShape::ClosedHandCursor);
                if (_stroke.isEmpty() || (_stroke.last() - pos).manhattanLength() > 3){
                    _stroke << pos;
                    update();
                }
            }
            else{
                ImageViewer<ReconstructionSetup<CameraT>>::mouseMoveEvent(e);
            }
        }

        virtual void mouseReleaseEvent(QMouseEvent * e) override {
            if (_applyStroke && _applyStroke()){
                refreshDataAsync();
                refreshData();
                data().lockForWrite();
                data().setModified();
                data().unlock();
            }
            _stroke.clear();
            update();
            ImageViewer<ReconstructionSetup<CameraT>>::mouseReleaseEvent(e);
        }

    private:
        std::function<bool(void)> _applyStroke;
    };

    return new Widget(&rec, parent);
}

StepWidgetInterface * CreateBindingWidgetAndActions(
    DataOfType<ReconstructionSetup<PanoramicCamera>> & rec,
    QList<QAction*> & actions, QWidget * parent){
    return CreateBindingWidgetAndActionsTemplated(rec, actions, parent);
}

StepWidgetInterface * CreateBindingWidgetAndActions(
    DataOfType<ReconstructionSetup<PerspectiveCamera>> & rec,
    QList<QAction*> & actions, QWidget * parent){
    return CreateBindingWidgetAndActionsTemplated(rec, actions, parent);
}


Image ImageForTexture(const PanoramicView & view) { return view.image; }
Image ImageForTexture(const PerspectiveView & view) { 
    return view.sampled(PanoramicCamera(2000, Point3(0, 0, 0), Point3(1, 0, 0), Vec3(0, 0, 1))).image; 
}

template <class CameraT>
StepWidgetInterface * CreateBindingWidgetAndActionsTemplated(DataOfType<Reconstruction<CameraT>> & rec,
    QList<QAction*> & actions, QWidget * parent) {

    return CreateSceneViewer(&rec, [](DataOfType<Reconstruction<CameraT>> & data, panoramix::gui::Visualizer & viz){ 

        struct ComponentID {
            int handleID;
            bool isRegion;
        };

        gui::ColorTable rgbctable = gui::ColorTableDescriptor::RGBGreys;

        viz.installingOptions.discretizeOptions.colorTable = gui::ColorTableDescriptor::RGB;
        std::vector<std::pair<ComponentID, gui::Colored<gui::SpatialProjectedPolygon>>> spps;
        std::vector<gui::Colored<Line3>> lines;

        for (auto & g : data.content.gs){
            auto & mg = g.mg;
            auto & controls = g.controls;
            auto & vars = g.vars;

            data.lockForRead();
            for (auto & c : mg.components<RegionData>()){
                if (!controls[c.topo.hd].used)
                    continue;
                auto uh = c.topo.hd;
                auto & region = c.data;
                gui::SpatialProjectedPolygon spp;
                // filter corners
                ForeachCompatibleWithLastElement(c.data.normalizedContours.front().begin(), c.data.normalizedContours.front().end(),
                    std::back_inserter(spp.corners),
                    [](const Vec3 & a, const Vec3 & b) -> bool {
                    return AngleBetweenDirections(a, b) > 0.0;
                });
                if (spp.corners.size() < 3)
                    continue;

                spp.projectionCenter = Point3(0, 0, 0);
                spp.plane = Instance(mg, controls, vars, uh);
                if (HasValue(spp.plane, IsInfOrNaN<double>)){
                    continue;
                }
                assert(!HasValue(spp.plane, IsInfOrNaN<double>));
                spps.emplace_back(ComponentID{ uh.id, true }, std::move(gui::ColorAs(spp, gui::ColorTag::Black)));
            }

            for (auto & c : mg.components<LineData>()){
                if (!controls[c.topo.hd].used)
                    continue;
                auto uh = c.topo.hd;
                auto & line = c.data;
                if (HasValue(line.line, IsInfOrNaN<double>)){
                    continue;
                }
                lines.push_back(gui::ColorAs(Instance(mg, controls, vars, uh), gui::ColorTag::Black));
            }

            assert(controls.vanishingPoints.size() >= 3);
            data.unlock();
        }

        data.lockForRead();
        gui::ResourceStore::set("texture", ImageForTexture(data.content.view));
        data.unlock();

        qDebug() << "loading texture";
        viz.begin(spps).shaderSource(gui::OpenGLShaderSourceDescriptor::XPanorama).resource("texture").end();
        qDebug() << "texture loaded";

        viz.installingOptions.discretizeOptions.color = gui::ColorTag::DarkGray;
        viz.installingOptions.lineWidth = 5.0;
        viz.add(lines);

        viz.installingOptions.discretizeOptions.color = gui::ColorTag::Black;
        viz.installingOptions.lineWidth = 1.0;

        viz.renderOptions.renderMode = gui::RenderModeFlag::Triangles | gui::RenderModeFlag::Lines;
        viz.renderOptions.backgroundColor = gui::ColorTag::White;
        viz.renderOptions.bwColor = 0.0;
        viz.renderOptions.bwTexColor = 1.0;
        viz.renderOptions.cullBackFace = false;
        viz.renderOptions.cullFrontFace = false;
        viz.camera(PerspectiveCamera(1000, 800, Point2(500, 400),
            800, Point3(-1, 1, 1), Point3(0, 0, 0)));

    }, parent);

}



StepWidgetInterface * CreateBindingWidgetAndActions(
    DataOfType<Reconstruction<PanoramicCamera>> & rec,
    QList<QAction*> & actions, QWidget * parent){
    return CreateBindingWidgetAndActionsTemplated(rec, actions, parent);
}

StepWidgetInterface * CreateBindingWidgetAndActions(
    DataOfType<Reconstruction<PerspectiveCamera>> & rec,
    QList<QAction*> & actions, QWidget * parent){
    return CreateBindingWidgetAndActionsTemplated(rec, actions, parent);
}






StepWidgetInterface * CreateBindingWidgetAndActions(
    DataOfType<ReconstructionRefinement<PanoramicCamera>> & rec,
    QList<QAction*> & actions, QWidget * parent){
    return CreateSceneViewer(&rec, 
        [](DataOfType<ReconstructionRefinement<PanoramicCamera>> & data, panoramix::gui::Visualizer & viz){

        data.lockForRead();
        gui::ResourceStore::set("texture", ImageForTexture(data.content.view));
        viz.begin(data.content.shapes).shaderSource(gui::OpenGLShaderSourceDescriptor::XPanorama).resource("texture").end();
        data.unlock();

        viz.renderOptions.renderMode = gui::RenderModeFlag::Triangles | gui::RenderModeFlag::Lines;
        viz.renderOptions.backgroundColor = gui::ColorTag::White;
        viz.renderOptions.bwColor = 0.0;
        viz.renderOptions.bwTexColor = 1.0;
        viz.renderOptions.cullBackFace = false;
        viz.renderOptions.cullFrontFace = true;
        viz.camera(PerspectiveCamera(1000, 800, Point2(500, 400),
            800, Point3(-1, 1, 1), Point3(0, 0, 0)));

    }, parent);
}




