// SPDX-FileCopyrightText: 2000-2010 University College London, Alasdair Turner
// SPDX-FileCopyrightText: 2011-2012 Tasos Varoudis
// SPDX-FileCopyrightText: 2024 Petros Koutsolampros
//
// SPDX-License-Identifier: GPL-3.0-or-later

// The meta graph

#include "metagraphdm.hpp"

#include "salalib/agents/agentanalysis.hpp"
#include "salalib/alllinemap.hpp"
#include "salalib/axialmodules/axialintegration.hpp"
#include "salalib/axialmodules/axiallocal.hpp"
#include "salalib/axialmodules/axialstepdepth.hpp"
#include "salalib/importutils.hpp"
#include "salalib/isovist.hpp"
#include "salalib/isovistutils.hpp"
#include "salalib/mapconverter.hpp"
#include "salalib/metagraphreadwrite.hpp"
#include "salalib/segmmodules/segmangular.hpp"
#include "salalib/segmmodules/segmmetric.hpp"
#include "salalib/segmmodules/segmmetricpd.hpp"
#include "salalib/segmmodules/segmtopological.hpp"
#include "salalib/segmmodules/segmtopologicalpd.hpp"
#include "salalib/segmmodules/segmtulip.hpp"
#include "salalib/segmmodules/segmtulipdepth.hpp"
#include "salalib/vgamodules/vgaangular.hpp"
#include "salalib/vgamodules/vgaangulardepth.hpp"
#include "salalib/vgamodules/vgaisovist.hpp"
#include "salalib/vgamodules/vgametric.hpp"
#include "salalib/vgamodules/vgametricdepth.hpp"
#include "salalib/vgamodules/vgametricdepthlinkcost.hpp"
#include "salalib/vgamodules/vgathroughvision.hpp"
#include "salalib/vgamodules/vgavisualglobal.hpp"
#include "salalib/vgamodules/vgavisualglobaldepth.hpp"
#include "salalib/vgamodules/vgavisuallocal.hpp"

#include "salalib/genlib/comm.hpp"

#include <tuple>

MetaGraphDM::MetaGraphDM(std::string name)
    : m_state(0), m_viewClass(DX_VIEWNONE), m_showGrid(false), m_showText(false),
      currentLayer(std::nullopt) {
    m_metaGraph.name = std::move(name);
    m_metaGraph.version = -1; // <- if unsaved, file version is -1

    // whether or not showing text / grid saved with file:

    // bsp tree for making isovists:
    m_bspNodeTree = BSPNodeTree();
}

bool MetaGraphDM::setViewClass(int command) {
    if (command < 0x10) {
        throw("Use with a show command, not a view class type");
    }
    if ((command & (DX_SHOWHIDEVGA | DX_SHOWVGATOP)) && (~m_state & DX_LATTICEMAPS))
        return false;
    if ((command & (DX_SHOWHIDEAXIAL | DX_SHOWAXIALTOP)) && (~m_state & DX_SHAPEGRAPHS))
        return false;
    if ((command & (DX_SHOWHIDESHAPE | DX_SHOWSHAPETOP)) && (~m_state & DX_DATAMAPS))
        return false;
    switch (command) {
    case DX_SHOWHIDEVGA:
        if (m_viewClass & (DX_VIEWVGA | DX_VIEWBACKVGA)) {
            m_viewClass &= ~(DX_VIEWVGA | DX_VIEWBACKVGA);
            if (m_viewClass & DX_VIEWBACKAXIAL) {
                m_viewClass ^= (DX_VIEWAXIAL | DX_VIEWBACKAXIAL);
            } else if (m_viewClass & DX_VIEWBACKDATA) {
                m_viewClass ^= (DX_VIEWDATA | DX_VIEWBACKDATA);
            }
        } else if (m_viewClass & (DX_VIEWAXIAL | DX_VIEWDATA)) {
            m_viewClass &= ~(DX_VIEWBACKAXIAL | DX_VIEWBACKDATA);
            m_viewClass |= DX_VIEWBACKVGA;
        } else {
            m_viewClass |= DX_VIEWVGA;
        }
        break;
    case DX_SHOWHIDEAXIAL:
        if (m_viewClass & (DX_VIEWAXIAL | DX_VIEWBACKAXIAL)) {
            m_viewClass &= ~(DX_VIEWAXIAL | DX_VIEWBACKAXIAL);
            if (m_viewClass & DX_VIEWBACKVGA) {
                m_viewClass ^= (DX_VIEWVGA | DX_VIEWBACKVGA);
            } else if (m_viewClass & DX_VIEWBACKDATA) {
                m_viewClass ^= (DX_VIEWDATA | DX_VIEWBACKDATA);
            }
        } else if (m_viewClass & (DX_VIEWVGA | DX_VIEWDATA)) {
            m_viewClass &= ~(DX_VIEWBACKVGA | DX_VIEWBACKDATA);
            m_viewClass |= DX_VIEWBACKAXIAL;
        } else {
            m_viewClass |= DX_VIEWAXIAL;
        }
        break;
    case DX_SHOWHIDESHAPE:
        if (m_viewClass & (DX_VIEWDATA | DX_VIEWBACKDATA)) {
            m_viewClass &= ~(DX_VIEWDATA | DX_VIEWBACKDATA);
            if (m_viewClass & DX_VIEWBACKVGA) {
                m_viewClass ^= (DX_VIEWVGA | DX_VIEWBACKVGA);
            } else if (m_viewClass & DX_VIEWBACKAXIAL) {
                m_viewClass ^= (DX_VIEWAXIAL | DX_VIEWBACKAXIAL);
            }
        } else if (m_viewClass & (DX_VIEWVGA | DX_VIEWAXIAL)) {
            m_viewClass &= ~(DX_VIEWBACKVGA | DX_VIEWBACKAXIAL);
            m_viewClass |= DX_VIEWBACKDATA;
        } else {
            m_viewClass |= DX_VIEWDATA;
        }
        break;
    case DX_SHOWVGATOP:
        if (m_viewClass & DX_VIEWAXIAL) {
            m_viewClass = DX_VIEWBACKAXIAL | DX_VIEWVGA;
        } else if (m_viewClass & DX_VIEWDATA) {
            m_viewClass = DX_VIEWBACKDATA | DX_VIEWVGA;
        } else {
            m_viewClass = DX_VIEWVGA | (m_viewClass & (DX_VIEWBACKAXIAL | DX_VIEWBACKDATA));
        }
        break;
    case DX_SHOWAXIALTOP:
        if (m_viewClass & DX_VIEWVGA) {
            m_viewClass = DX_VIEWBACKVGA | DX_VIEWAXIAL;
        } else if (m_viewClass & DX_VIEWDATA) {
            m_viewClass = DX_VIEWBACKDATA | DX_VIEWAXIAL;
        } else {
            m_viewClass = DX_VIEWAXIAL | (m_viewClass & (DX_VIEWBACKVGA | DX_VIEWBACKDATA));
        }
        break;
    case DX_SHOWSHAPETOP:
        if (m_viewClass & DX_VIEWVGA) {
            m_viewClass = DX_VIEWBACKVGA | DX_VIEWDATA;
        } else if (m_viewClass & DX_VIEWAXIAL) {
            m_viewClass = DX_VIEWBACKAXIAL | DX_VIEWDATA;
        } else {
            m_viewClass = DX_VIEWDATA | (m_viewClass & (DX_VIEWBACKVGA | DX_VIEWBACKAXIAL));
        }
        break;
    }
    return true;
}

double MetaGraphDM::getLocationValue(const Point2f &point) {
    // this varies according to whether axial or vga information is displayed on
    // top
    double val = -2;

    if (viewingProcessedPoints()) {
        val = getDisplayedLatticeMap().getLocationValue(point);
    } else if (viewingProcessedLines()) {
        val = getDisplayedShapeGraph().getLocationValue(point);
    } else if (viewingProcessedShapes()) {
        val = getDisplayedDataMap().getLocationValue(point);
    }

    return val;
}

bool MetaGraphDM::setGrid(double spacing, const Point2f &offset) {
    m_state &= ~DX_LATTICEMAPS;

    getDisplayedLatticeMap().setGrid(spacing, offset);
    getDisplayedLatticeMap().setDisplayedAttribute(-2);

    m_state |= DX_LATTICEMAPS;

    // just reassert that we should be viewing this (since set grid is essentially
    // a "new point map")
    setViewClass(DX_SHOWVGATOP);

    return true;
}

// AV TV // semifilled
bool MetaGraphDM::makePoints(const Point2f &p, int fillType, Communicator *communicator) {
    //   m_state &= ~POINTS;

    try {
        std::vector<Line4f> lines = getShownDrawingFilesAsLines();
        getDisplayedLatticeMap().getInternalMap().blockLines(lines);
        getDisplayedLatticeMap().makePoints(p, fillType, communicator);
        getDisplayedLatticeMap().setDisplayedAttribute(-2);
    } catch (Communicator::CancelledException) {

        // By this stage points almost certainly exist,
        // To avoid problems, just say points exist:
        m_state |= DX_LATTICEMAPS;

        return false;
    }

    //   m_state |= POINTS;

    return true;
}

bool MetaGraphDM::clearPoints() {
    bool bReturn = getDisplayedLatticeMap().clearPoints();
    return bReturn;
}

bool MetaGraphDM::hasVisibleDrawingShapes() {
    // tries to find at least one visible shape
    for (const auto &pixelGroup : m_drawingFiles) {
        for (const auto &pixel : pixelGroup.maps) {
            if (pixel.isShown() && !pixel.getAllShapes().empty()) {
                return true;
            }
        }
    }
    return false;
}

std::vector<std::pair<std::reference_wrapper<const ShapeMapDM>, int>>
MetaGraphDM::getShownDrawingMaps() {
    std::vector<std::pair<std::reference_wrapper<const ShapeMapDM>, int>> maps;
    for (const auto &pixelGroup : m_drawingFiles) {
        int j = 0;
        for (const auto &pixel : pixelGroup.maps) {
            // chooses the first editable layer it can find:
            if (pixel.isShown()) {
                maps.push_back(std::make_pair(std::ref(pixel), j));
            }
            j++;
        }
    }
    return maps;
}

std::vector<std::pair<std::reference_wrapper<const ShapeMap>, int>> MetaGraphDM::getAsInternalMaps(
    std::vector<std::pair<std::reference_wrapper<const ShapeMapDM>, int>> maps) {
    std::vector<std::pair<std::reference_wrapper<const ShapeMap>, int>> internalMaps;
    internalMaps.reserve(maps.size());
    std::transform(maps.begin(), maps.end(), std::back_inserter(internalMaps),
                   [](std::pair<std::reference_wrapper<const ShapeMapDM>, int> &map) {
                       return std::make_pair(std::ref(map.first.get().getInternalMap()),
                                             map.second);
                   });
    return internalMaps;
}

std::vector<Line4f> MetaGraphDM::getShownDrawingFilesAsLines() {
    std::vector<Line4f> lines;
    auto shownMaps = getShownDrawingMaps();
    for (const auto &map : shownMaps) {
        std::vector<SimpleLine> newLines =
            map.first.get().getInternalMap().getAllShapesAsSimpleLines();
        for (const auto &line : newLines) {
            lines.emplace_back(line.start(), line.end());
        }
    }
    return lines;
}

std::vector<SalaShape> MetaGraphDM::getShownDrawingFilesAsShapes() {
    std::vector<SalaShape> shapes;

    auto shownMaps = getShownDrawingMaps();
    for (const auto &map : shownMaps) {
        auto refShapes = map.first.get().getInternalMap().getAllShapes();
        for (const auto &refShape : refShapes) {
            shapes.push_back(refShape.second);
        }
    }
    return shapes;
}

bool MetaGraphDM::makeGraph(Communicator *communicator, int algorithm, double maxdist) {
    // this is essentially a version tag, and remains for historical reasons:
    m_state |= DX_ANGULARGRAPH;

    bool graphMade = false;

    try {
        std::vector<Line4f> lines = getShownDrawingFilesAsLines();
        getDisplayedLatticeMap().getInternalMap().blockLines(lines);
        // algorithm is now used for boundary graph option (as a simple boolean)
        graphMade = getDisplayedLatticeMap().getInternalMap().sparkGraph2(
            communicator, (algorithm != 0), maxdist);
        getDisplayedLatticeMap().setDisplayedAttribute(LatticeMap::Column::CONNECTIVITY);
    } catch (Communicator::CancelledException) {
        graphMade = false;
    }

    if (graphMade) {
        setViewClass(DX_SHOWVGATOP);
    }

    return graphMade;
}

bool MetaGraphDM::unmakeGraph(bool removeLinks) {
    bool graphUnmade = getDisplayedLatticeMap().getInternalMap().unmake(removeLinks);

    getDisplayedLatticeMap().setDisplayedAttribute(-2);

    if (graphUnmade) {
        setViewClass(DX_SHOWVGATOP);
    }

    return graphUnmade;
}

bool MetaGraphDM::analyseGraph(Communicator *communicator, int pointDepthSelection,
                               AnalysisType outputType, int local, bool gatesOnly, int global,
                               double radius, bool simpleVersion) {
    bool analysisCompleted = false;

    if (pointDepthSelection) {
        if (m_viewClass & DX_VIEWVGA && !getDisplayedLatticeMap().isSelected()) {
            return false;
        } else if (m_viewClass & DX_VIEWAXIAL && !getDisplayedShapeGraph().hasSelectedElements()) {
            return false;
        }
    }

    try {
        analysisCompleted = true;
        if (pointDepthSelection == 1) {
            if (m_viewClass & DX_VIEWVGA) {
                auto &map = getDisplayedLatticeMap();
                std::set<PixelRef> origins;
                for (auto &sel : map.getSelSet())
                    origins.insert(sel);
                auto analysis = VGAVisualGlobalDepth(map.getInternalMap(), std::move(origins));
                auto analysisResult = analysis.run(communicator);
                analysis.copyResultToMap(analysisResult.getAttributes(),
                                         analysisResult.getAttributeData(), map.getInternalMap(),
                                         analysisResult.columnStats);
                analysisCompleted = analysisResult.completed;

                // force redisplay:
                map.setDisplayedAttribute(-2);
                map.setDisplayedAttribute(VGAVisualGlobalDepth::Column::VISUAL_STEP_DEPTH);

            } else if (m_viewClass & DX_VIEWAXIAL) {
                if (!getDisplayedShapeGraph().getInternalMap().isSegmentMap()) {
                    auto &map = getDisplayedShapeGraph();
                    analysisCompleted = AxialStepDepth(map.getSelSet())
                                            .run(communicator, map.getInternalMap(), false)
                                            .completed;
                    map.setDisplayedAttribute(-1); // <- override if it's already showing
                    map.setDisplayedAttribute(AxialStepDepth::Column::STEP_DEPTH);
                } else {
                    auto &map = getDisplayedShapeGraph();
                    analysisCompleted = SegmentTulipDepth(1024, map.getSelSet())
                                            .run(communicator, map.getInternalMap(), false)
                                            .completed;
                    map.setDisplayedAttribute(-2); // <- override if it's already showing
                    map.setDisplayedAttribute(SegmentTulipDepth::Column::ANGULAR_STEP_DEPTH);
                }
            }
            // REPLACES:
            // Graph::calculate_point_depth_matrix( communicator );
        } else if (pointDepthSelection == 2) {
            if (m_viewClass & DX_VIEWVGA) {
                auto &map = getDisplayedLatticeMap();
                std::set<PixelRef> origins;
                for (auto &sel : map.getSelSet())
                    origins.insert(sel);
                std::unique_ptr<IVGAMetric> analysis =
                    map.getAttributeTable().hasColumn(
                        VGAMetricDepthLinkCost::Column::LINK_METRIC_COST)
                        ? std::unique_ptr<IVGAMetric>(
                              new VGAMetricDepthLinkCost(map.getInternalMap(), origins))
                        : std::unique_ptr<IVGAMetric>(
                              new VGAMetricDepth(map.getInternalMap(), std::move(origins)));
                auto analysisResult = analysis->run(communicator);
                analysis->copyResultToMap(analysisResult.getAttributes(),
                                          analysisResult.getAttributeData(), map.getInternalMap(),
                                          analysisResult.columnStats);
                analysisCompleted = analysisResult.completed;
                map.setDisplayedAttribute(-2);
                map.setDisplayedAttribute(VGAMetricDepth::Column::METRIC_STEP_SHORTEST_PATH_LENGTH);
            } else if (m_viewClass & DX_VIEWAXIAL &&
                       getDisplayedShapeGraph().getInternalMap().isSegmentMap()) {

                auto &map = getDisplayedShapeGraph();
                analysisCompleted = SegmentMetricPD(map.getSelSet())
                                        .run(communicator, map.getInternalMap(), false)
                                        .completed;
                map.setDisplayedAttribute(SegmentMetricPD::Column::METRIC_STEP_DEPTH);
            }
        } else if (pointDepthSelection == 3) {
            auto &map = getDisplayedLatticeMap();
            std::set<PixelRef> origins;
            for (auto &sel : map.getSelSet()) {
                origins.insert(sel);
            }
            auto analysis = VGAAngularDepth(map.getInternalMap(), std::move(origins));
            auto analysisResult = analysis.run(communicator);
            analysis.copyResultToMap(analysisResult.getAttributes(),
                                     analysisResult.getAttributeData(), map.getInternalMap(),
                                     analysisResult.columnStats);
            analysisCompleted = analysisResult.completed;
            map.setDisplayedAttribute(-2);
            map.setDisplayedAttribute(VGAAngularDepth::Column::ANGULAR_STEP_DEPTH);
        } else if (pointDepthSelection == 4) {
            if (m_viewClass & DX_VIEWVGA) {
                auto &map = getDisplayedLatticeMap();
                map.getInternalMap().binDisplay(communicator, map.getSelSet());
            } else if (m_viewClass & DX_VIEWAXIAL &&
                       getDisplayedShapeGraph().getInternalMap().isSegmentMap()) {

                auto &map = getDisplayedShapeGraph();
                analysisCompleted = SegmentTopologicalPD(map.getSelSet())
                                        .run(communicator, map.getInternalMap(), false)
                                        .completed;
                map.setDisplayedAttribute(-2);
                map.setDisplayedAttribute(SegmentTopologicalPD::Column::TOPOLOGICAL_STEP_DEPTH);
            }
        } else if (outputType == AnalysisType::ISOVIST) {
            auto shapes = getShownDrawingFilesAsShapes();
            auto &map = getDisplayedLatticeMap();
            auto analysis = VGAIsovist(map.getInternalMap(), shapes);
            analysis.setSimpleVersion(simpleVersion);
            AnalysisResult analysisResult = analysis.run(communicator);
            analysis.copyResultToMap(analysisResult.getAttributes(),
                                     analysisResult.getAttributeData(), map.getInternalMap(),
                                     analysisResult.columnStats);
            analysisCompleted = analysisResult.completed;
            map.setDisplayedAttribute(-2);
            map.setDisplayedAttribute(VGAIsovist::Column::ISOVIST_AREA);
        } else if (outputType == AnalysisType::VISUAL) {
            bool localResult = true;
            bool globalResult = true;
            if (local) {
                auto &map = getDisplayedLatticeMap();
                auto analysis = VGAVisualLocal(map.getInternalMap(), gatesOnly);
                auto analysisResult = analysis.run(communicator);
                analysis.copyResultToMap(analysisResult.getAttributes(),
                                         analysisResult.getAttributeData(), map.getInternalMap(),
                                         analysisResult.columnStats);
                localResult = analysisResult.completed;
                map.setDisplayedAttribute(-2);
                map.setDisplayedAttribute(VGAVisualLocal::Column::VISUAL_CLUSTERING_COEFFICIENT);
            }
            if (global) {
                auto &map = getDisplayedLatticeMap();
                auto analysis = VGAVisualGlobal(map.getInternalMap(), radius, gatesOnly);
                analysis.setSimpleVersion(simpleVersion);
                analysis.setLegacyWriteMiscs(true);
                auto analysisResult = analysis.run(communicator);
                analysis.copyResultToMap(analysisResult.getAttributes(),
                                         analysisResult.getAttributeData(), map.getInternalMap(),
                                         analysisResult.columnStats);
                globalResult = analysisResult.completed;
                map.setDisplayedAttribute(-2);
                map.setDisplayedAttribute(VGAVisualGlobal::getColumnWithRadius(
                    VGAVisualGlobal::Column::VISUAL_INTEGRATION_HH, radius));
            }
            analysisCompleted = globalResult & localResult;
        } else if (outputType == AnalysisType::METRIC) {
            auto &map = getDisplayedLatticeMap();
            auto analysis = VGAMetric(map.getInternalMap(), radius, gatesOnly);
            auto analysisResult = analysis.run(communicator);
            analysis.copyResultToMap(analysisResult.getAttributes(),
                                     analysisResult.getAttributeData(), map.getInternalMap(),
                                     analysisResult.columnStats);
            analysisCompleted = analysisResult.completed;
            map.overrideDisplayedAttribute(-2);
            map.setDisplayedAttribute(VGAMetric::getColumnWithRadius(
                VGAMetric::Column::METRIC_MEAN_SHORTEST_PATH_DISTANCE, radius,
                map.getInternalMap().getRegion()));
        } else if (outputType == AnalysisType::ANGULAR) {
            auto &map = getDisplayedLatticeMap();
            auto analysis = VGAAngular(map.getInternalMap(), radius, gatesOnly);
            auto analysisResult = analysis.run(communicator);
            analysis.copyResultToMap(analysisResult.getAttributes(),
                                     analysisResult.getAttributeData(), map.getInternalMap(),
                                     analysisResult.columnStats);
            analysisCompleted = analysisResult.completed;
            map.overrideDisplayedAttribute(-2);
            map.setDisplayedAttribute(VGAAngular::getColumnWithRadius(
                VGAAngular::Column::ANGULAR_MEAN_DEPTH, radius, map.getInternalMap().getRegion()));
        } else if (outputType == AnalysisType::THRU_VISION) {
            auto &map = getDisplayedLatticeMap();
            auto analysis = VGAThroughVision(map.getInternalMap());
            auto analysisResult = analysis.run(communicator);
            analysis.copyResultToMap(analysisResult.getAttributes(),
                                     analysisResult.getAttributeData(), map.getInternalMap(),
                                     analysisResult.columnStats);
            analysisCompleted = analysisResult.completed;
            map.overrideDisplayedAttribute(-2);
            map.setDisplayedAttribute(VGAThroughVision::Column::THROUGH_VISION);
        }
    } catch (Communicator::CancelledException) {
        analysisCompleted = false;
    }

    return analysisCompleted;
}

//////////////////////////////////////////////////////////////////

bool MetaGraphDM::isEditableMap() {
    if (m_viewClass & DX_VIEWAXIAL) {
        return getDisplayedShapeGraph().isEditable();
    } else if (m_viewClass & DX_VIEWDATA) {
        return getDisplayedDataMap().isEditable();
    }
    // still to do: allow editing of drawing layers
    return false;
}

ShapeMapDM &MetaGraphDM::getEditableMap() {
    ShapeMapDM *map = nullptr;
    if (m_viewClass & DX_VIEWAXIAL) {
        map = &(getDisplayedShapeGraph());
    } else if (m_viewClass & DX_VIEWDATA) {
        map = &(getDisplayedDataMap());
    } else {
        // still to do: allow editing of drawing layers
    }
    if (map == nullptr || !map->isEditable()) {
        throw 0;
    }
    return *map;
}

bool MetaGraphDM::makeShape(const Line4f &line) {
    if (!isEditableMap()) {
        return false;
    }
    auto &map = getEditableMap();
    return (map.makeLineShape(line, true) != -1);
}

int MetaGraphDM::polyBegin(const Line4f &line) {
    if (!isEditableMap()) {
        return -1;
    }
    auto &map = getEditableMap();
    return map.polyBegin(line);
}

bool MetaGraphDM::polyAppend(int shapeRef, const Point2f &point) {
    if (!isEditableMap()) {
        return false;
    }
    auto &map = getEditableMap();
    return map.polyAppend(shapeRef, point);
}

bool MetaGraphDM::polyClose(int shapeRef) {
    if (!isEditableMap()) {
        return false;
    }
    auto &map = getEditableMap();
    return map.polyClose(shapeRef);
}

bool MetaGraphDM::polyCancel(int shapeRef) {
    if (!isEditableMap()) {
        return false;
    }
    auto &map = getEditableMap();
    return map.polyCancel(shapeRef);
}

bool MetaGraphDM::moveSelShape(const Line4f &line) {
    bool shapeMoved = false;
    if (m_viewClass & DX_VIEWAXIAL) {
        auto &map = getDisplayedShapeGraph();
        if (!map.isEditable()) {
            return false;
        }
        if (map.getSelCount() > 1) {
            return false;
        }
        int rowid = *map.getSelSet().begin();
        shapeMoved = map.moveShape(rowid, line);
        if (shapeMoved) {
            map.clearSel();
        }
    } else if (m_viewClass & DX_VIEWDATA) {
        auto &map = getDisplayedDataMap();
        if (!map.isEditable()) {
            return false;
        }
        if (map.getSelCount() > 1) {
            return false;
        }
        int rowid = *map.getSelSet().begin();
        shapeMoved = map.moveShape(rowid, line);
        if (shapeMoved) {
            map.clearSel();
        }
    }
    return shapeMoved;
}

//////////////////////////////////////////////////////////////////

// returns 0: fail, 1: made isovist, 2: made isovist and added new shapemap
// layer
int MetaGraphDM::makeIsovist(Communicator *communicator, const Point2f &p, double startangle,
                             double endangle, bool, bool closeIsovistPoly) {
    int isovistMade = 0;
    // first make isovist
    Isovist iso;

    if (makeBSPtree(m_bspNodeTree, communicator)) {
        m_viewClass &= ~DX_VIEWDATA;
        isovistMade = 1;
        iso.makeit(m_bspNodeTree.getRoot(), p, m_metaGraph.region, startangle, endangle,
                   closeIsovistPoly);
        size_t shapelayer = 0;
        auto mapRef = getMapRef(m_dataMaps, "Isovists");
        if (!mapRef.has_value()) {
            m_dataMaps.emplace_back("Isovists", ShapeMap::DATAMAP);
            setDisplayedDataMapRef(m_dataMaps.size() - 1);
            shapelayer = m_dataMaps.size() - 1;
            m_state |= DX_DATAMAPS;
            isovistMade = 2;
        } else {
            shapelayer = mapRef.value();
        }
        auto &map = m_dataMaps[shapelayer];

        std::vector<Point2f> polygon = iso.getPolygon();

        // false: closed polygon, true: isovist
        int polyref = map.getInternalMap().makePolyShape(polygon, false);
        map.getInternalMap().getAllShapes()[polyref].setCentroid(p);
        map.overrideDisplayedAttribute(-2);
        map.setDisplayedAttribute(-1);
        setViewClass(DX_SHOWSHAPETOP);
        AttributeTable &table = map.getInternalMap().getAttributeTable();
        AttributeRow &row = table.getRow(AttributeKey(polyref));
        IsovistUtils::setIsovistData(iso, table, row);
    }
    return isovistMade;
}

static std::pair<double, double> startendangle(Point2f vec, double fov) {
    std::pair<double, double> angles;
    // n.b. you must normalise this before getting the angle!
    vec.normalise();
    angles.first = vec.angle() - fov / 2.0;
    angles.second = vec.angle() + fov / 2.0;
    if (angles.first < 0.0)
        angles.first += 2.0 * M_PI;
    if (angles.second > 2.0 * M_PI)
        angles.second -= 2.0 * M_PI;
    return angles;
}

// returns 0: fail, 1: made isovist, 2: made isovist and added new shapemap
// layer
int MetaGraphDM::makeIsovistPath(Communicator *communicator, double fov, bool) {
    int pathMade = 0;

    // must be showing a suitable map -- that is, one which may have polylines or
    // lines

    int viewclass = getViewClass() & DX_VIEWFRONT;
    if (!(viewclass == DX_VIEWAXIAL || viewclass == DX_VIEWDATA)) {
        return 0;
    }

    size_t isovistmapref = 0;
    auto &map = (viewclass == DX_VIEWAXIAL) //
                    ? getDisplayedShapeGraph()
                    : getDisplayedDataMap();

    // must have a selection: the selected shapes will form the set from which to
    // create the isovist paths
    if (!map.hasSelectedElements()) {
        return 0;
    }

    ShapeMapDM *isovists = nullptr;

    bool first = true;
    if (makeBSPtree(m_bspNodeTree, communicator)) {
        std::set<int> selset = map.getSelSet();
        const auto &shapes = map.getAllShapes();
        for (auto &shapeRef : selset) {
            const SalaShape &path = shapes.at(shapeRef);
            if (path.isLine() || path.isPolyLine()) {
                if (first) {
                    pathMade = 1;
                    auto imrf = getMapRef(m_dataMaps, "Isovists");
                    if (!imrf.has_value()) {
                        m_dataMaps.emplace_back(
                            std::make_unique<ShapeMap>("Isovists", ShapeMap::DATAMAP));
                        isovistmapref = m_dataMaps.size() - 1;
                        setDisplayedDataMapRef(isovistmapref);
                        pathMade = 2;
                    } else {
                        isovistmapref = imrf.value();
                    }
                    isovists = &m_dataMaps[isovistmapref];
                    first = false;
                }
                // now make an isovist:
                Isovist iso;
                //
                std::pair<double, double> angles;
                angles.first = 0.0;
                angles.second = 0.0;
                //
                if (path.isLine()) {
                    Point2f start = path.getLine().t_start();
                    Point2f vec = path.getLine().vector();
                    if (fov < 2.0 * M_PI) {
                        angles = startendangle(vec, fov);
                    }
                    iso.makeit(m_bspNodeTree.getRoot(), start, m_metaGraph.region, angles.first,
                               angles.second);
                    int polyref = isovists->getInternalMap().makePolyShape(iso.getPolygon(), false);
                    isovists->getInternalMap().getAllShapes()[polyref].setCentroid(start);
                    AttributeTable &table = isovists->getInternalMap().getAttributeTable();
                    AttributeRow &row = table.getRow(AttributeKey(polyref));
                    IsovistUtils::setIsovistData(iso, table, row);
                } else {
                    for (size_t i = 0; i < path.points.size() - 1; i++) {
                        Line4f li = Line4f(path.points[i], path.points[i + 1]);
                        Point2f start = li.t_start();
                        Point2f vec = li.vector();
                        if (fov < 2.0 * M_PI) {
                            angles = startendangle(vec, fov);
                        }
                        iso.makeit(m_bspNodeTree.getRoot(), start, m_metaGraph.region, angles.first,
                                   angles.second);
                        int polyref =
                            isovists->getInternalMap().makePolyShape(iso.getPolygon(), false);
                        auto newPolyIter = isovists->getInternalMap().getAllShapes().find(polyref);
                        if (newPolyIter == isovists->getInternalMap().getAllShapes().end()) {
                            throw genlib::RuntimeException("Failed to create shape (" +
                                                           std::to_string(polyref) +
                                                           ") when making isovist path");
                        }
                        newPolyIter->second.setCentroid(start);
                        AttributeTable &table = isovists->getInternalMap().getAttributeTable();
                        AttributeRow &row = table.getRow(AttributeKey(polyref));
                        IsovistUtils::setIsovistData(iso, table, row);
                    }
                }
            }
        }
        if (isovists) {
            isovists->overrideDisplayedAttribute(-2);
            isovists->setDisplayedAttribute(-1);
            setDisplayedDataMapRef(isovistmapref);
        }
    }
    return pathMade;
}

// this version uses your own isovist (and assumes no communicator required for
// BSP tree
bool MetaGraphDM::makeIsovist(const Point2f &p, Isovist &iso) {
    if (makeBSPtree(m_bspNodeTree)) {
        iso.makeit(m_bspNodeTree.getRoot(), p, m_metaGraph.region);
        return true;
    }
    return false;
}

bool MetaGraphDM::makeBSPtree(BSPNodeTree &bspNodeTree, Communicator *communicator) {
    if (bspNodeTree.built()) {
        return true;
    }

    std::vector<Line4f> partitionlines;
    auto shownMaps = getShownDrawingMaps();
    for (const auto &mapLayer : shownMaps) {
        auto refShapes = mapLayer.first.get().getInternalMap().getAllShapes();
        for (const auto &refShape : refShapes) {
            std::vector<Line4f> newLines = refShape.second.getAsLines();
            // must check it is not a zero length line:
            for (const Line4f &line : newLines) {
                if (line.length() > 0.0) {
                    partitionlines.push_back(line);
                }
            }
        }
    }

    if (partitionlines.size()) {
        //
        // Now we'll try the BSP tree:
        //
        bspNodeTree.makeNewRoot(/* destroyIfBuilt = */ true);

        time_t atime = 0;
        if (communicator) {
            communicator->CommPostMessage(Communicator::NUM_RECORDS, partitionlines.size());
            qtimer(atime, 0);
        }

        try {
            BSPTree::make(communicator, atime, partitionlines, m_bspNodeTree.getRoot());
            m_bspNodeTree.setBuilt(true);
        } catch (Communicator::CancelledException) {
            m_bspNodeTree.setBuilt(false);
            // probably best to delete the half made bastard of a tree:
            m_bspNodeTree.destroy();
        }
    }

    partitionlines.clear();

    return m_bspNodeTree.built();
}

size_t MetaGraphDM::addShapeGraph(ShapeGraphDM &&shapeGraph) {
    m_shapeGraphs.emplace_back(std::move(shapeGraph));
    auto mapref = m_shapeGraphs.size() - 1;
    setDisplayedShapeGraphRef(mapref);
    m_state |= DX_SHAPEGRAPHS;
    setViewClass(DX_SHOWAXIALTOP);
    return mapref;
}

size_t MetaGraphDM::addShapeGraph(ShapeGraph &&shapeGraph) {
    return addShapeGraph(ShapeGraphDM(std::make_unique<ShapeGraph>(std::move(shapeGraph))));
}

size_t MetaGraphDM::addShapeGraph(const std::string &name, int type) {
    auto mapref = addShapeGraph(ShapeGraphDM(std::make_unique<ShapeGraph>(name, type)));
    // add a couple of default columns:
    AttributeTable &table =
        m_shapeGraphs[static_cast<size_t>(mapref)].getInternalMap().getAttributeTable();
    auto connIdx = table.insertOrResetLockedColumn(ShapeGraph::Column::CONNECTIVITY);
    if ((type & ShapeMap::LINEMAP) != 0) {
        table.insertOrResetLockedColumn(ShapeGraph::Column::LINE_LENGTH);
    }
    m_shapeGraphs[mapref].setDisplayedAttribute(static_cast<int>(connIdx));
    return mapref;
}
size_t MetaGraphDM::addShapeMap(const std::string &name) {
    m_dataMaps.emplace_back(name, ShapeMap::DATAMAP);
    m_state |= DX_DATAMAPS;
    setViewClass(DX_SHOWSHAPETOP);
    return m_dataMaps.size() - 1;
}
void MetaGraphDM::removeDisplayedMap() {
    switch (m_viewClass & DX_VIEWFRONT) {
    case DX_VIEWVGA: {
        if (!hasDisplayedLatticeMap())
            return;
        removeLatticeMap(getDisplayedLatticeMapRef());
        if (m_latticeMaps.empty()) {
            setViewClass(DX_SHOWHIDEVGA);
            m_state &= ~DX_LATTICEMAPS;
        }
        break;
    }
    case DX_VIEWAXIAL: {
        if (!hasDisplayedShapeGraph())
            return;
        removeShapeGraph(getDisplayedShapeGraphRef());
        if (m_shapeGraphs.empty()) {
            setViewClass(DX_SHOWHIDEAXIAL);
            m_state &= ~DX_SHAPEGRAPHS;
        }
        break;
    }
    case DX_VIEWDATA:
        if (!hasDisplayedDataMap())
            return;
        removeDataMap(getDisplayedDataMapRef());
        if (m_dataMaps.empty()) {
            setViewClass(DX_SHOWHIDESHAPE);
            m_state &= ~DX_DATAMAPS;
        }
        break;
    }
}

//////////////////////////////////////////////////////////////////

bool MetaGraphDM::convertDrawingToAxial(Communicator *comm, std::string layerName) {
    int oldstate = m_state;

    m_state &= ~DX_SHAPEGRAPHS;

    bool converted = true;

    try {
        auto shownMaps = getShownDrawingMaps();
        auto shownMapsInternal = getAsInternalMaps(shownMaps);
        for (auto mapLayer : shownMaps) {
            mapLayer.first.get().setShow(false);
        }
        auto mapref =
            addShapeGraph(MapConverter::convertDrawingToAxial(comm, layerName, shownMapsInternal));
        m_shapeGraphs[mapref].setDisplayedAttribute(ShapeGraph::Column::CONNECTIVITY);

        setDisplayedShapeGraphRef(mapref);
    } catch (Communicator::CancelledException) {
        converted = false;
    }

    m_state |= oldstate;

    if (converted) {
        m_state |= DX_SHAPEGRAPHS;
        setViewClass(DX_SHOWAXIALTOP);
    }

    return converted;
}

bool MetaGraphDM::convertDataToAxial(Communicator *comm, std::string layerName, bool keeporiginal,
                                     bool pushvalues) {
    int oldstate = m_state;

    m_state &= ~DX_SHAPEGRAPHS;

    bool converted = true;

    try {
        addShapeGraph(MapConverter::convertDataToAxial(
            comm, layerName, getDisplayedDataMap().getInternalMap(), pushvalues));

        m_shapeGraphs.back().overrideDisplayedAttribute(-2); // <- override if it's already showing
        m_shapeGraphs.back().setDisplayedAttribute(
            static_cast<int>(m_shapeGraphs.back().getAttributeTable().getColumnIndex(
                ShapeGraph::Column::CONNECTIVITY)));

        if (m_shapeGraphs.size() > 0)
            setDisplayedShapeGraphRef(m_shapeGraphs.size() - 1);
        else
            unsetDisplayedShapeGraphRef();
    } catch (Communicator::CancelledException) {
        converted = false;
    }

    m_state |= oldstate;

    if (converted) {
        if (!keeporiginal) {
            removeDataMap(getDisplayedDataMapRef());
            if (m_dataMaps.empty()) {
                setViewClass(DX_SHOWHIDESHAPE);
                m_state &= ~DX_DATAMAPS;
            }
        }
        m_state |= DX_SHAPEGRAPHS;
        setViewClass(DX_SHOWAXIALTOP);
    }

    return converted;
}

// typeflag: -1 convert drawing to convex, 0 or 1, convert data to convex (1 is
// pushvalues)
bool MetaGraphDM::convertToConvex(Communicator *comm, std::string layerName, bool keeporiginal,
                                  int shapeMapType, bool copydata) {
    int oldstate = m_state;

    m_state &= ~DX_SHAPEGRAPHS; // and convex maps...

    bool converted = false;

    try {
        if (shapeMapType == ShapeMap::DRAWINGMAP) {
            auto shownMaps = getShownDrawingMaps();
            auto shownMapsInternal = getAsInternalMaps(shownMaps);
            for (auto &pixel : shownMaps) {
                pixel.first.get().setShow(false);
            }
            addShapeGraph(MapConverter::convertDrawingToConvex(comm, layerName, shownMapsInternal));
            converted = true;
        } else if (shapeMapType == ShapeMap::DATAMAP) {
            addShapeGraph(MapConverter::convertDataToConvex(
                comm, layerName, getDisplayedDataMap().getInternalMap(), copydata));
            converted = true;
        }

        m_shapeGraphs.back().overrideDisplayedAttribute(-2); // <- override if it's already showing
        m_shapeGraphs.back().setDisplayedAttribute(-1);

        if (m_shapeGraphs.size() > 0)
            setDisplayedShapeGraphRef(m_shapeGraphs.size() - 1);
        else
            unsetDisplayedShapeGraphRef();

    } catch (Communicator::CancelledException) {
        converted = false;
    }

    m_state |= oldstate;

    if (converted) {
        if (shapeMapType != ShapeMap::DRAWINGMAP && !keeporiginal) {
            removeDataMap(getDisplayedDataMapRef());
            if (m_dataMaps.empty()) {
                setViewClass(DX_SHOWHIDESHAPE);
                m_state &= ~DX_DATAMAPS;
            }
        }
        m_state |= DX_SHAPEGRAPHS;
        setViewClass(DX_SHOWAXIALTOP);
    }

    return converted;
}

bool MetaGraphDM::convertDrawingToSegment(Communicator *comm, std::string layerName) {
    int oldstate = m_state;

    m_state &= ~DX_SHAPEGRAPHS;

    bool converted = true;

    try {

        auto shownMaps = getShownDrawingMaps();
        auto shownMapsInternal = getAsInternalMaps(shownMaps);

        for (auto &pixel : shownMaps) {
            pixel.first.get().setShow(false);
        }

        auto mapref = addShapeGraph(
            MapConverter::convertDrawingToSegment(comm, layerName, shownMapsInternal));
        m_shapeGraphs[mapref].setDisplayedAttribute(ShapeGraph::Column::CONNECTIVITY);

        if (m_shapeGraphs.size() > 0)
            setDisplayedShapeGraphRef(m_shapeGraphs.size() - 1);
        else
            unsetDisplayedShapeGraphRef();
    } catch (Communicator::CancelledException) {
        converted = false;
    }

    m_state |= oldstate;

    if (converted) {
        m_state |= DX_SHAPEGRAPHS;
        setViewClass(DX_SHOWAXIALTOP);
    }

    return converted;
}

bool MetaGraphDM::convertDataToSegment(Communicator *comm, std::string layerName, bool keeporiginal,
                                       bool pushvalues) {
    int oldstate = m_state;

    m_state &= ~DX_SHAPEGRAPHS;

    bool converted = true;

    try {
        addShapeGraph(MapConverter::convertDataToSegment(
            comm, layerName, getDisplayedDataMap().getInternalMap(), pushvalues));

        m_shapeGraphs.back().overrideDisplayedAttribute(-2); // <- override if it's already showing
        m_shapeGraphs.back().setDisplayedAttribute(-1);

        if (m_shapeGraphs.size() > 0)
            setDisplayedShapeGraphRef(m_shapeGraphs.size() - 1);
        else
            unsetDisplayedShapeGraphRef();
    } catch (Communicator::CancelledException) {
        converted = false;
    }

    m_state |= oldstate;

    if (converted) {
        if (!keeporiginal) {
            removeDataMap(getDisplayedDataMapRef());
            if (m_dataMaps.empty()) {
                setViewClass(DX_SHOWHIDESHAPE);
                m_state &= ~DX_DATAMAPS;
            }
        }
        m_state |= DX_SHAPEGRAPHS;
        setViewClass(DX_SHOWAXIALTOP);
    }

    return converted;
}

// note: type flag says whether this is graph to data map or drawing to data map

bool MetaGraphDM::convertToData(Communicator *, std::string layerName, bool keeporiginal,
                                int shapeMapType, bool copydata) {
    int oldstate = m_state;

    m_state &= ~DX_DATAMAPS;

    bool converted = false;

    try {
        // This should be much easier than before,
        // simply move the shapes from the drawing layer
        // note however that more than one layer might be combined:
        // create map layer...
        m_dataMaps.emplace_back(layerName, ShapeMap::DATAMAP);
        auto destmapref = m_dataMaps.size() - 1;
        auto &destmap = m_dataMaps.back();
        auto &table = destmap.getAttributeTable();
        size_t count = 0;

        // drawing to data
        if (shapeMapType == ShapeMap::DRAWINGMAP) {
            auto layercol = destmap.getInternalMap().addAttribute("Drawing Layer");
            // add all visible layers to the set of map:

            auto shownMaps = getShownDrawingMaps();
            auto shownMapsInternal = getAsInternalMaps(shownMaps);
            for (const auto &pixel : shownMapsInternal) {
                auto refShapes = pixel.first.get().getAllShapes();
                for (const auto &refShape : refShapes) {
                    int key = destmap.makeShape(refShape.second);
                    table.getRow(AttributeKey(key))
                        .setValue(layercol, static_cast<float>(pixel.second + 1));
                    count++;
                }
            }
            for (const auto &pixel : shownMaps) {
                pixel.first.get().setShow(false);
            }
        }
        // convex, axial or segment graph to data (similar)
        else {
            auto &sourcemap = getDisplayedShapeGraph();
            count = sourcemap.getShapeCount();
            // take viewed graph and push all geometry to it (since it is *all*
            // geometry, pushing is easy)
            int copyflag = copydata ? (ShapeMap::COPY_GEOMETRY | ShapeMap::COPY_ATTRIBUTES)
                                    : (ShapeMap::COPY_GEOMETRY);
            destmap.copy(sourcemap, copyflag);
        }
        //
        if (count == 0) {
            // if no objects converted then a crash is caused, so remove it:
            removeDataMap(destmapref);
            converted = false;
        } else {
            // we can stop here! -- remember to set up display:
            setDisplayedDataMapRef(destmapref);
            destmap.invalidateDisplayedAttribute();
            destmap.setDisplayedAttribute(-1);
            converted = true;
        }
    } catch (Communicator::CancelledException) {
        converted = false;
    }

    m_state |= oldstate;

    if (converted) {
        if (shapeMapType != ShapeMap::DRAWINGMAP && !keeporiginal) {
            removeShapeGraph(getDisplayedShapeGraphRef());
            if (m_shapeGraphs.empty()) {
                setViewClass(DX_SHOWHIDEAXIAL);
                m_state &= ~DX_SHAPEGRAPHS;
            }
        }
        m_state |= DX_DATAMAPS;
        setViewClass(DX_SHOWSHAPETOP);
    }

    return converted;
}

bool MetaGraphDM::convertToDrawing(Communicator *, std::string layerName,
                                   bool fromDisplayedDataMap) {
    bool converted = false;

    int oldstate = m_state;

    m_state &= ~DX_LINEDATA;

    try {
        const ShapeMapDM *sourcemap;
        if (fromDisplayedDataMap) {
            sourcemap = &(getDisplayedDataMap());
        } else {
            sourcemap = &(getDisplayedShapeGraph());
        }
        //
        if (sourcemap->getInternalMap().getShapeCount() != 0) {
            // this is very simple: create a new drawing layer, and add the data...
            auto group = m_drawingFiles.begin();
            for (; group != m_drawingFiles.end(); ++group) {
                if (group->groupData.getName() == "Converted Maps") {
                    break;
                }
            }
            if (group == m_drawingFiles.end()) {
                m_drawingFiles.emplace_back("Converted Maps");
                group = std::prev(m_drawingFiles.end());
            }
            group->maps.emplace_back(layerName, ShapeMap::DRAWINGMAP);
            group->maps.back().copy(*sourcemap, ShapeMap::COPY_GEOMETRY);
            //
            // dummy set still required:
            group->maps.back().invalidateDisplayedAttribute();
            group->maps.back().setDisplayedAttribute(-1);
            //
            // three levels of merge region:
            if (group->maps.size() == 1) {
                group->groupData.setRegion(group->maps.back().getRegion());
            } else {
                group->groupData.setRegion(
                    group->groupData.getRegion().runion(group->maps.back().getRegion()));
            }
            if (m_drawingFiles.size() == 1) {
                m_metaGraph.region = group->groupData.getRegion();
            } else {
                m_metaGraph.region = m_metaGraph.region.runion(group->groupData.getRegion());
            }
            //
            converted = true;
        }
        converted = true;
    } catch (Communicator::CancelledException) {
        converted = false;
    }

    m_state |= oldstate;

    if (converted) {
        m_state |= DX_LINEDATA;
    }

    return converted;
}

bool MetaGraphDM::convertAxialToSegment(Communicator *comm, std::string layerName,
                                        bool keeporiginal, bool pushvalues, double stubremoval) {
    if (!hasDisplayedShapeGraph()) {
        return false;
    }

    auto axialShapeGraphRef = getDisplayedShapeGraphRef();

    int oldstate = m_state;
    m_state &= ~DX_SHAPEGRAPHS;

    bool converted = true;

    try {
        addShapeGraph(
            MapConverter::convertAxialToSegment(comm, getDisplayedShapeGraph().getInternalMap(),
                                                layerName, keeporiginal, pushvalues, stubremoval));

        m_shapeGraphs.back().overrideDisplayedAttribute(-2); // <- override if it's already showing
        m_shapeGraphs.back().setDisplayedAttribute(
            static_cast<int>(m_shapeGraphs.back().getAttributeTable().getColumnIndex(
                ShapeGraph::Column::CONNECTIVITY)));

        if (m_shapeGraphs.size() > 0)
            setDisplayedShapeGraphRef(m_shapeGraphs.size() - 1);
        else
            unsetDisplayedShapeGraphRef();
    } catch (Communicator::CancelledException) {
        converted = false;
    }

    m_state |= oldstate;

    if (converted) {
        if (!keeporiginal) {
            removeShapeGraph(axialShapeGraphRef);
        }
        m_state |= DX_SHAPEGRAPHS;
        setViewClass(DX_SHOWAXIALTOP);
    }

    return converted;
}

int MetaGraphDM::loadMifMap(Communicator *comm, std::istream &miffile, std::istream &midfile) {
    int oldstate = m_state;
    m_state &= ~DX_DATAMAPS;

    int mapLoaded = -1;

    try {
        // create map layer...
        m_dataMaps.emplace_back(comm->GetMBInfileName(), ShapeMap::DATAMAP);
        auto mifmapref = m_dataMaps.size() - 1;
        auto &mifmap = m_dataMaps.back();
        mapLoaded = mifmap.getInternalMap().loadMifMap(miffile, midfile);
        if (mapLoaded == MINFO_OK || mapLoaded == MINFO_MULTIPLE) { // multiple is just a warning
                                                                    // display an attribute:
            mifmap.overrideDisplayedAttribute(-2);
            mifmap.setDisplayedAttribute(-1);
            setDisplayedDataMapRef(mifmapref);
        } else { // error: undo!
            removeDataMap(mifmapref);
        }
    } catch (Communicator::CancelledException) {
        mapLoaded = -1;
    }

    m_state = oldstate;

    if (mapLoaded == MINFO_OK ||
        mapLoaded == MINFO_MULTIPLE) { // MINFO_MULTIPLE is simply a warning
        m_state |= DX_DATAMAPS;
        setViewClass(DX_SHOWSHAPETOP);
    }

    return mapLoaded;
}

bool MetaGraphDM::makeAllLineMap(Communicator *communicator, const Point2f &seed) {
    int oldstate = m_state;
    m_state &= ~DX_SHAPEGRAPHS;   // Clear axial map data flag (stops accidental redraw
                                  // during reload)
    m_viewClass &= ~DX_VIEWAXIAL; // Also clear the view_class flag

    bool mapMade = true;

    try {
        // this is an index to look up the all line map, used by UI to determine if
        // can make fewest line map note: it is not saved for historical reasons
        if (hasAllLineMap()) {
            removeShapeGraph(m_allLineMapData->index);
            m_allLineMapData = std::nullopt;
        }

        {
            auto allm = AllLine::createAllLineMap();
            std::vector<std::reference_wrapper<const ShapeMap>> visibleDrawingFiles;
            auto shownMaps = getShownDrawingMaps();
            for (const auto &pixel : shownMaps) {
                visibleDrawingFiles.push_back(pixel.first.get().getInternalMap());
            }
            m_allLineMapData = AllLine::generate(communicator, allm, visibleDrawingFiles, seed);
            m_allLineMapData->index = addShapeGraph(std::move(allm));
            m_shapeGraphs[m_allLineMapData->index].setDisplayedAttribute(
                ShapeGraph::Column::CONNECTIVITY);
        }

        setDisplayedShapeGraphRef(m_allLineMapData->index);
    } catch (Communicator::CancelledException) {
        mapMade = false;
    }

    m_state = oldstate;

    if (mapMade) {
        m_state |= DX_SHAPEGRAPHS;
        setViewClass(DX_SHOWAXIALTOP);
    }

    return mapMade;
}

bool MetaGraphDM::makeFewestLineMap(Communicator *communicator, int replace) {
    int oldstate = m_state;
    m_state &= ~DX_SHAPEGRAPHS; // Clear axial map data flag (stops accidental redraw
                                // during reload)

    bool mapMade = true;

    try {
        // no all line map
        if (!hasAllLineMap()) {
            return false;
        }

        auto &alllinemap = m_shapeGraphs[m_allLineMapData->index];

        auto [fewestlinemap_subsets, fewestlinemap_minimal] = AllLine::extractFewestLineMaps(
            communicator, alllinemap.getInternalMap(), *m_allLineMapData, 0);

        if (replace != 0) {
            std::optional<size_t> index = std::nullopt;

            for (size_t i = 0; i < m_shapeGraphs.size(); i++) {
                if (m_shapeGraphs[i].getName() == "Fewest-Line Map (Subsets)" ||
                    m_shapeGraphs[i].getName() == "Fewest Line Map (Subsets)") {
                    index = i;
                }
            }

            if (index.has_value()) {
                removeShapeGraph(index.value());
            }

            for (size_t i = 0; i < m_shapeGraphs.size(); i++) {
                if (m_shapeGraphs[i].getName() == "Fewest-Line Map (Subsets)" ||
                    m_shapeGraphs[i].getName() == "Fewest Line Map (Subsets)") {
                    index = static_cast<int>(i);
                }
            }

            if (index.has_value()) {
                removeShapeGraph(index.value());
            }
        }
        auto newMap = addShapeGraph(std::move(fewestlinemap_subsets));
        m_shapeGraphs[newMap].setDisplayedAttribute(ShapeGraph::Column::CONNECTIVITY);

        newMap = addShapeGraph(std::move(fewestlinemap_minimal));
        m_shapeGraphs[newMap].setDisplayedAttribute(ShapeGraph::Column::CONNECTIVITY);

        setDisplayedShapeGraphRef(m_shapeGraphs.size() - 2);

    } catch (Communicator::CancelledException) {
        mapMade = false;
    }

    m_state = oldstate;

    if (mapMade) {
        m_state |= DX_SHAPEGRAPHS; // note: should originally have at least one axial map
        setViewClass(DX_SHOWAXIALTOP);
    }

    return mapMade;
}

bool MetaGraphDM::analyseAxial(Communicator *communicator, std::set<double> radiusSet,
                               int weightedMeasureCol, bool choice, bool fulloutput,
                               bool localAnalysis, bool forceLegacyColumnOrder) {
    m_state &= ~DX_SHAPEGRAPHS; // Clear axial map data flag (stops accidental redraw
                                // during reload)

    bool analysisCompleted = false;

    try {
        auto &map = getDisplayedShapeGraph();
        AxialIntegration analysis(radiusSet, weightedMeasureCol, choice, fulloutput);
        analysis.setForceLegacyColumnOrder(forceLegacyColumnOrder);
        analysisCompleted = analysis.run(communicator, map.getInternalMap(), false).completed;

        map.setDisplayedAttribute(-1); // <- override if it's already showing

        map.setDisplayedAttribute(static_cast<int>(AxialIntegration::getFormattedColumnIdx(
            map.getInternalMap().getAttributeTable(), AxialIntegration::Column::INTEGRATION,
            static_cast<int>(*radiusSet.rbegin()), std::nullopt,
            AxialIntegration::Normalisation::HH)));

        if (localAnalysis)
            analysisCompleted &=
                AxialLocal()
                    .run(communicator, getDisplayedShapeGraph().getInternalMap(), false)
                    .completed;
    } catch (Communicator::CancelledException) {
        analysisCompleted = false;
    }

    m_state |= DX_SHAPEGRAPHS;

    return analysisCompleted;
}

bool MetaGraphDM::analyseSegmentsTulip(Communicator *communicator, std::set<double> &radiusSet,
                                       bool selOnly, int tulipBins, int weightedMeasureCol,
                                       RadiusType radiusType, bool choice, int weightedMeasureCol2,
                                       int routeweightCol, bool interactive,
                                       bool forceLegacyColumnOrder) {
    m_state &= ~DX_SHAPEGRAPHS; // Clear axial map data flag (stops accidental redraw
                                // during reload)

    bool analysisCompleted = false;

    try {
        auto &map = getDisplayedShapeGraph();
        SegmentTulip analysis(radiusSet,
                              selOnly ? std::make_optional(map.getSelSet()) : std::nullopt,
                              tulipBins, weightedMeasureCol, radiusType, choice,
                              weightedMeasureCol2, routeweightCol, interactive);
        analysis.setForceLegacyColumnOrder(forceLegacyColumnOrder);
        analysisCompleted = analysis.run(communicator, map.getInternalMap(), false).completed;
        map.setDisplayedAttribute(-2); // <- override if it's already showing
        if (choice) {
            map.setDisplayedAttribute(static_cast<int>(SegmentTulip::getFormattedColumnIdx(
                map.getInternalMap().getAttributeTable(), SegmentTulip::Column::CHOICE, tulipBins,
                radiusType, static_cast<int>(*radiusSet.begin()))));
        } else {
            map.setDisplayedAttribute(static_cast<int>(SegmentTulip::getFormattedColumnIdx(
                map.getInternalMap().getAttributeTable(), SegmentTulip::Column::TOTAL_DEPTH,
                tulipBins, radiusType, static_cast<int>(*radiusSet.begin()))));
        }
    } catch (Communicator::CancelledException) {
        analysisCompleted = false;
    }

    m_state |= DX_SHAPEGRAPHS;

    return analysisCompleted;
}

bool MetaGraphDM::analyseSegmentsAngular(Communicator *communicator, std::set<double> radiusSet) {
    m_state &= ~DX_SHAPEGRAPHS; // Clear axial map data flag (stops accidental redraw
                                // during reload)

    bool analysisCompleted = false;

    try {
        auto &map = getDisplayedShapeGraph();
        analysisCompleted =
            SegmentAngular(radiusSet).run(communicator, map.getInternalMap(), false).completed;

        map.setDisplayedAttribute(-2); // <- override if it's already showing
        std::string depthColText = SegmentAngular::getFormattedColumn(
            SegmentAngular::Column::ANGULAR_MEAN_DEPTH, static_cast<int>(*radiusSet.begin()));
        map.setDisplayedAttribute(depthColText);

    } catch (Communicator::CancelledException) {
        analysisCompleted = false;
    }

    m_state |= DX_SHAPEGRAPHS;

    return analysisCompleted;
}

bool MetaGraphDM::analyseTopoMetMultipleRadii(Communicator *communicator,
                                              std::set<double> &radiusSet, AnalysisType outputType,
                                              double radius, bool selOnly) {
    m_state &= ~DX_SHAPEGRAPHS; // Clear axial map data flag (stops accidental redraw
                                // during reload)

    bool analysisCompleted = true;

    try {
        // note: "outputType" reused for analysis type (either 0 = topological or 1
        // = metric)
        auto &map = getDisplayedShapeGraph();
        for (size_t r = 0; r < radiusSet.size(); r++) {
            if (outputType == AnalysisType::ISOVIST) {
                if (!SegmentTopological(radius, selOnly ? std::make_optional(map.getSelSet())
                                                        : std::nullopt)
                         .run(communicator, map.getInternalMap(), false)
                         .completed)
                    analysisCompleted = false;
                if (!selOnly) {
                    map.setDisplayedAttribute(SegmentTopological::getFormattedColumn(
                        SegmentTopological::Column::TOPOLOGICAL_CHOICE, radius));
                } else {
                    map.setDisplayedAttribute(SegmentTopological::getFormattedColumn(
                        SegmentTopological::Column::TOPOLOGICAL_MEAN_DEPTH, radius));
                }

            } else {
                if (!SegmentMetric(radius,
                                   selOnly ? std::make_optional(map.getSelSet()) : std::nullopt)
                         .run(communicator, map.getInternalMap(), false)
                         .completed)
                    analysisCompleted = false;
                if (!selOnly) {
                    map.setDisplayedAttribute(SegmentMetric::getFormattedColumn(
                        SegmentMetric::Column::METRIC_CHOICE, radius));
                } else {
                    map.setDisplayedAttribute(SegmentMetric::getFormattedColumn(
                        SegmentMetric::Column::METRIC_MEAN_DEPTH, radius));
                }
            }
        }
    } catch (Communicator::CancelledException) {
        analysisCompleted = false;
    }

    m_state |= DX_SHAPEGRAPHS;

    return analysisCompleted;
}

bool MetaGraphDM::analyseTopoMet(Communicator *communicator, AnalysisType outputType, double radius,
                                 bool selOnly) {
    m_state &= ~DX_SHAPEGRAPHS; // Clear axial map data flag (stops accidental redraw
                                // during reload)

    bool analysisCompleted = false;

    auto &map = getDisplayedShapeGraph();

    try {
        // note: "outputType" reused for analysis type (either 0 = topological or 1
        // = metric)
        if (outputType == AnalysisType::ISOVIST) {
            analysisCompleted =
                SegmentTopological(radius,
                                   selOnly ? std::make_optional(map.getSelSet()) : std::nullopt)
                    .run(communicator, map.getInternalMap(), false)
                    .completed;
            if (!selOnly) {
                map.setDisplayedAttribute(SegmentTopological::getFormattedColumn(
                    SegmentTopological::Column::TOPOLOGICAL_CHOICE, radius));
            } else {
                map.setDisplayedAttribute(SegmentTopological::getFormattedColumn(
                    SegmentTopological::Column::TOPOLOGICAL_MEAN_DEPTH, radius));
            }
        } else {
            analysisCompleted =
                SegmentMetric(radius, selOnly ? std::make_optional(map.getSelSet()) : std::nullopt)
                    .run(communicator, map.getInternalMap(), false)
                    .completed;
            if (!selOnly) {
                map.setDisplayedAttribute(SegmentMetric::getFormattedColumn(
                    SegmentMetric::Column::METRIC_CHOICE, radius));
            } else {
                map.setDisplayedAttribute(SegmentMetric::getFormattedColumn(
                    SegmentMetric::Column::METRIC_MEAN_DEPTH, radius));
            }
        }
    } catch (Communicator::CancelledException) {
        analysisCompleted = false;
    }

    m_state |= DX_SHAPEGRAPHS;

    return analysisCompleted;
}

size_t MetaGraphDM::loadLineData(Communicator *communicator, const std::string &fileName,
                                 int loadType) {

    sala::ImportFileType importFileType = sala::ImportFileType::DXF;
    if (loadType & DX_DXF) {
        importFileType = sala::ImportFileType::DXF;
    }
    if (loadType & DX_CAT) {
        importFileType = sala::ImportFileType::CAT;
    } else if (loadType & DX_RT1) {
        importFileType = sala::ImportFileType::RT1;
    } else if (loadType & DX_NTF) {
        importFileType = sala::ImportFileType::NTF;
    }
    return loadLineData(communicator, fileName, importFileType, loadType & DX_REPLACE);
}

size_t MetaGraphDM::loadLineData(Communicator *communicator, const std::string &fileName,
                                 sala::ImportFileType importFileType, bool replace) {

    // if bsp tree exists
    m_bspNodeTree.destroy();

    m_state &= ~DX_LINEDATA; // Clear line data flag (stops accidental redraw during reload)

    if (replace) {
        m_drawingFiles.clear();
    }

    std::vector<ShapeMap> maps;

    std::ifstream fstream(fileName);

    auto newDrawingFile =
        addDrawingFile(fileName, sala::importFile(fstream, communicator, fileName,
                                                  sala::ImportType::DRAWINGMAP, importFileType));

    m_state |= DX_LINEDATA;

    return newDrawingFile;
}

size_t MetaGraphDM::addDrawingFile(std::string name, std::vector<ShapeMap> &&maps) {

    m_drawingFiles.emplace_back(name);

    auto &newDrawingMap = m_drawingFiles.back();
    for (auto &&shapeMap : maps) {
        newDrawingMap.addMap(std::make_unique<ShapeMap>(std::move(shapeMap)));
        auto &shpmdx = newDrawingMap.maps.back();
        // TODO: Investigate why setDisplayedAttribute needs to be set to -2 first
        shpmdx.setDisplayedAttribute(-2);
        shpmdx.setDisplayedAttribute(-1);
    }

    if (m_drawingFiles.size() == 1) {
        m_metaGraph.region = m_drawingFiles.back().groupData.getRegion();
    } else {
        m_metaGraph.region = m_metaGraph.region.runion(m_drawingFiles.back().groupData.getRegion());
    }
    return m_drawingFiles.size() - 1;
}

ShapeMapDM &MetaGraphDM::createNewShapeMap(sala::ImportType mapType, std::string name) {

    if (mapType == sala::ImportType::DATAMAP) {
        m_dataMaps.emplace_back(name, ShapeMap::DATAMAP);
        m_dataMaps.back().setDisplayedAttribute(0);
        return m_dataMaps.back();
    }
    // sala::ImportType::DRAWINGMAP
    m_drawingFiles.back().maps.emplace_back(name, ShapeMap::DRAWINGMAP);
    return m_drawingFiles.back().maps.back();
}

void MetaGraphDM::deleteShapeMap(sala::ImportType mapType, ShapeMapDM &shapeMap) {

    switch (mapType) {
    case sala::ImportType::DRAWINGMAP: {
        // go through the files to find if the layer is in one of them
        // if it is, remove it and if the remaining file is empty then
        // remove that too
        auto pixelGroup = m_drawingFiles.begin();
        for (; pixelGroup != m_drawingFiles.begin(); ++pixelGroup) {
            auto mapToRemove = pixelGroup->maps.end();
            auto pixel = pixelGroup->maps.begin();
            for (; pixel != pixelGroup->maps.end(); ++pixel) {
                if (&(*pixel) == &shapeMap) {
                    mapToRemove = pixel;
                    break;
                }
            }
            if (mapToRemove != pixelGroup->maps.end()) {
                pixelGroup->maps.erase(mapToRemove);
                if (pixelGroup->maps.size() == 0) {
                    m_drawingFiles.erase(pixelGroup);
                }
                break;
            }
        }
        break;
    }
    case sala::ImportType::DATAMAP: {
        for (size_t i = 0; i < m_dataMaps.size(); i++) {
            if (&m_dataMaps[i] == &shapeMap) {
                m_dataMaps.erase(std::next(m_dataMaps.begin(), static_cast<int>(i)));
                break;
            }
        }
    }
    }
}

void MetaGraphDM::updateParentRegions(ShapeMap &shapeMap) {
    if (m_drawingFiles.back().groupData.getRegion().atZero()) {
        m_drawingFiles.back().groupData.setRegion(shapeMap.getRegion());
    } else {
        m_drawingFiles.back().groupData.setRegion(
            m_drawingFiles.back().groupData.getRegion().runion(shapeMap.getRegion()));
    }
    if (m_metaGraph.region.atZero()) {
        m_metaGraph.region = m_drawingFiles.back().groupData.getRegion();
    } else {
        m_metaGraph.region = m_metaGraph.region.runion(m_drawingFiles.back().groupData.getRegion());
    }
}

// the tidy(ish) version: still needs to be at top level and switch between
// layers

bool MetaGraphDM::pushValuesToLayer(int desttype, size_t destlayer, PushValues::Func pushFunc,
                                    bool countCol) {
    auto sourcetype = m_viewClass;
    auto sourcelayer = getDisplayedMapRef().value();
    size_t colIn = static_cast<size_t>(getDisplayedAttribute());

    // temporarily turn off everything to prevent redraw during sensitive time:
    int oldstate = m_state;
    m_state &= ~(DX_DATAMAPS | DX_AXIALLINES | DX_LATTICEMAPS);

    AttributeTable &tableIn = getAttributeTable(sourcetype, sourcelayer);
    AttributeTable &tableOut = getAttributeTable(desttype, destlayer);
    std::string name = tableIn.getColumnName(colIn);
    if ((tableOut.hasColumn(name) &&
         tableOut.getColumn(tableOut.getColumnIndex(name)).isLocked()) ||
        name == "Object Count") {
        name = std::string("Copied ") + name;
    }
    size_t colOut = tableOut.insertOrResetColumn(name);

    bool valuesPushed = pushValuesToLayer(sourcetype, sourcelayer, desttype, destlayer, colIn,
                                          colOut, pushFunc, countCol);

    m_state = oldstate;

    return valuesPushed;
}

// helper

// the full ubercontrol version:

bool MetaGraphDM::pushValuesToLayer(int sourcetype, size_t sourcelayer, int desttype,
                                    size_t destlayer, std::optional<size_t> colIn, size_t colOut,
                                    PushValues::Func pushFunc, bool createCountCol) {
    AttributeTable &tableOut = getAttributeTable(desttype, destlayer);

    std::optional<std::string> countColName = std::nullopt;
    if (createCountCol) {
        countColName = "Object Count";
        tableOut.insertOrResetColumn(countColName.value());
    }

    if (colIn.has_value() && desttype == DX_VIEWVGA &&
        ((sourcetype & DX_VIEWDATA) || (sourcetype & DX_VIEWAXIAL))) {
        auto &sourceMap =
            sourcetype & DX_VIEWDATA ? m_dataMaps[sourcelayer] : m_shapeGraphs[sourcelayer];
        auto &destMap = m_latticeMaps[destlayer];
        const auto &colInName = sourceMap.getAttributeTable().getColumnName(*colIn);
        const auto &colOutName = destMap.getAttributeTable().getColumnName(colOut);
        PushValues::shapeToPoint(sourceMap.getInternalMap(), colInName, destMap.getInternalMap(),
                                 colOutName, pushFunc, countColName);
    } else if (sourcetype & DX_VIEWDATA) {
        if (desttype == DX_VIEWAXIAL) {
            auto &sourceMap = m_dataMaps[sourcelayer];
            auto &destMap = m_shapeGraphs[destlayer];
            const auto &colInName =
                colIn.has_value()
                    ? std::make_optional(sourceMap.getAttributeTable().getColumnName(*colIn))
                    : std::nullopt;
            const auto &colOutName = destMap.getAttributeTable().getColumnName(colOut);
            PushValues::shapeToAxial(sourceMap.getInternalMap(), colInName,
                                     destMap.getInternalMap(), colOutName, pushFunc);
        } else if (desttype == DX_VIEWDATA) {
            if (sourcelayer == destlayer) {
                // error: pushing to same map
                return false;
            }
            auto &sourceMap = m_dataMaps[sourcelayer];
            auto &destMap = m_dataMaps[destlayer];
            const auto &colInName =
                colIn.has_value()
                    ? std::make_optional(sourceMap.getAttributeTable().getColumnName(*colIn))
                    : std::nullopt;
            const auto &colOutName = destMap.getAttributeTable().getColumnName(colOut);
            PushValues::shapeToShape(sourceMap.getInternalMap(), colInName,
                                     destMap.getInternalMap(), colOutName, pushFunc);
        }
    } else {

        if (sourcetype & DX_VIEWVGA) {
            if (desttype == DX_VIEWDATA) {
                auto &sourceMap = m_latticeMaps[sourcelayer];
                auto &destMap = m_dataMaps[destlayer];
                const auto &colInName =
                    colIn.has_value()
                        ? std::make_optional(sourceMap.getAttributeTable().getColumnName(*colIn))
                        : std::nullopt;
                const auto &colOutName = destMap.getAttributeTable().getColumnName(colOut);
                PushValues::pointToShape(sourceMap.getInternalMap(), colInName,
                                         destMap.getInternalMap(), colOutName, pushFunc);
            } else if (desttype == DX_VIEWAXIAL) {
                auto &sourceMap = m_latticeMaps[sourcelayer];
                auto &destMap = m_shapeGraphs[destlayer];
                const auto &colInName =
                    colIn.has_value()
                        ? std::make_optional(sourceMap.getAttributeTable().getColumnName(*colIn))
                        : std::nullopt;
                const auto &colOutName = destMap.getAttributeTable().getColumnName(colOut);
                PushValues::pointToAxial(sourceMap.getInternalMap(), colInName,
                                         destMap.getInternalMap(), colOutName, pushFunc);
            }
        } else if (sourcetype & DX_VIEWAXIAL) {
            if (desttype == DX_VIEWDATA) {
                auto &sourceMap = m_shapeGraphs[sourcelayer];
                auto &destMap = m_dataMaps[destlayer];
                const auto &colInName =
                    colIn.has_value()
                        ? std::make_optional(sourceMap.getAttributeTable().getColumnName(*colIn))
                        : std::nullopt;
                const auto &colOutName = destMap.getAttributeTable().getColumnName(colOut);
                PushValues::axialToShape(sourceMap.getInternalMap(), colInName,
                                         destMap.getInternalMap(), colOutName, pushFunc);
            } else if (desttype == DX_VIEWAXIAL) {
                auto &sourceMap = m_shapeGraphs[sourcelayer];
                auto &destMap = m_shapeGraphs[destlayer];
                const auto &colInName =
                    colIn.has_value()
                        ? std::make_optional(sourceMap.getAttributeTable().getColumnName(*colIn))
                        : std::nullopt;
                const auto &colOutName = destMap.getAttributeTable().getColumnName(colOut);
                PushValues::axialToAxial(sourceMap.getInternalMap(), colInName,
                                         destMap.getInternalMap(), colOutName, pushFunc);
            }
        }
    }

    // display new data in the relevant layer
    if (desttype == DX_VIEWVGA) {
        m_latticeMaps[destlayer].overrideDisplayedAttribute(-2);
        m_latticeMaps[destlayer].setDisplayedAttribute(static_cast<int>(colOut));
    } else if (desttype == DX_VIEWAXIAL) {
        m_shapeGraphs[destlayer].overrideDisplayedAttribute(-2);
        m_shapeGraphs[destlayer].setDisplayedAttribute(static_cast<int>(colOut));
    } else if (desttype == DX_VIEWDATA) {
        m_dataMaps[destlayer].overrideDisplayedAttribute(-2);
        m_dataMaps[destlayer].setDisplayedAttribute(static_cast<int>(colOut));
    }

    return true;
}

// Agent functionality: some of it still kept here with the metagraph
// (to allow push value to layer and back again)

void MetaGraphDM::runAgentEngine(Communicator *comm, std::unique_ptr<IAnalysis> &analysis) {
    if (!hasDisplayedLatticeMap()) {
        throw(genlib::RuntimeException("No Lattice map on display"));
    }
    auto &map = getDisplayedLatticeMap();
    auto agentAnalysis = dynamic_cast<AgentAnalysis *>(analysis.get());
    if (!agentAnalysis) {
        throw(genlib::RuntimeException("No agent analysis provided to runAgentEngine function"));
    }
    agentAnalysis->run(comm);

    if (agentAnalysis->setTooRecordTrails()) {
        m_state |= DX_DATAMAPS;
    }
    map.overrideDisplayedAttribute(-2);
    map.setDisplayedAttribute(AgentAnalysis::Column::GATE_COUNTS);
}

// Thru vision
// TODO: Undocumented functionality
bool MetaGraphDM::analyseThruVision(Communicator *comm, std::optional<size_t> gatelayer) {
    bool analysisCompleted = false;

    auto &table = getDisplayedLatticeMap().getAttributeTable();

    // always have temporary gate counting layers -- makes it easier to code
    auto colgates = table.insertOrResetColumn(AgentAnalysis::Column::INTERNAL_GATE);
    table.insertOrResetColumn(AgentAnalysis::Column::INTERNAL_GATE_COUNTS);

    if (gatelayer.has_value()) {
        // switch the reference numbers from the gates layer to the vga layer
        pushValuesToLayer(DX_VIEWDATA, gatelayer.value(), DX_VIEWVGA, getDisplayedLatticeMapRef(),
                          std::nullopt, colgates, PushValues::Func::TOT);
    }

    try {
        analysisCompleted =
            VGAThroughVision(getDisplayedLatticeMap().getInternalMap()).run(comm).completed;
    } catch (Communicator::CancelledException) {
        analysisCompleted = false;
    }

    // note after the analysis, the column order might have changed... retrieve:
    colgates = table.getColumnIndex(AgentAnalysis::Column::INTERNAL_GATE);
    auto colcounts = table.getColumnIndex(AgentAnalysis::Column::INTERNAL_GATE_COUNTS);

    if (analysisCompleted && gatelayer.has_value()) {
        AttributeTable &tableout = m_dataMaps[gatelayer.value()].getAttributeTable();
        auto targetcol = tableout.insertOrResetColumn("Thru Vision Counts");
        pushValuesToLayer(DX_VIEWVGA, getDisplayedLatticeMapRef(), DX_VIEWDATA, gatelayer.value(),
                          colcounts, targetcol, PushValues::Func::TOT);
    }

    // and always delete the temporary columns:
    table.removeColumn(colcounts);
    table.removeColumn(colgates);

    return analysisCompleted;
}

///////////////////////////////////////////////////////////////////////////////////

std::optional<size_t> MetaGraphDM::getDisplayedMapRef() const {
    std::optional<size_t> ref = std::nullopt;
    switch (m_viewClass & DX_VIEWFRONT) {
    case DX_VIEWVGA:
        if (!hasDisplayedLatticeMap())
            return std::nullopt;
        ref = getDisplayedLatticeMapRef();
        break;
    case DX_VIEWAXIAL:
        if (!hasDisplayedShapeGraph())
            return std::nullopt;
        ref = getDisplayedShapeGraphRef();
        break;
    case DX_VIEWDATA:
        if (!hasDisplayedDataMap())
            return std::nullopt;
        ref = getDisplayedDataMapRef();
        break;
    }
    return ref;
}

// I'd like to use this more often so that several classes other than data maps
// and shape graphs can be used in the future

int MetaGraphDM::getDisplayedMapType() {
    switch (m_viewClass & DX_VIEWFRONT) {
    case DX_VIEWVGA:
        return ShapeMap::LATTICEMAP;
    case DX_VIEWAXIAL:
        return hasDisplayedShapeGraph() && getDisplayedShapeGraphRef() != static_cast<size_t>(-1)
                   ? getDisplayedShapeGraph().getMapType()
                   : ShapeMap::EMPTYMAP;
    case DX_VIEWDATA:
        return getDisplayedDataMap().getMapType();
    }
    return ShapeMap::EMPTYMAP;
}

AttributeTable &MetaGraphDM::getDisplayedMapAttributes() {
    switch (m_viewClass & DX_VIEWFRONT) {
    case DX_VIEWVGA:
        return getDisplayedLatticeMap().getAttributeTable();
    case DX_VIEWAXIAL:
        return getDisplayedShapeGraph().getAttributeTable();
    case DX_VIEWDATA:
        return getDisplayedDataMap().getAttributeTable();
    }
    throw genlib::RuntimeException("No map displayed to get attribute table from");
}

bool MetaGraphDM::hasVisibleDrawingLayers() {
    if (!m_drawingFiles.empty()) {
        for (const auto &pixelGroup : m_drawingFiles) {
            for (const auto &pixel : pixelGroup.maps) {
                if (pixel.isShown())
                    return true;
            }
        }
    }
    return false;
}

Region4f MetaGraphDM::getBoundingBox() const {
    Region4f bounds = m_metaGraph.region;
    if (bounds.atZero() &&
        ((getState() & MetaGraphDM::DX_SHAPEGRAPHS) == MetaGraphDM::DX_SHAPEGRAPHS)) {
        bounds = getDisplayedShapeGraph().getRegion();
    }
    if (bounds.atZero() && ((getState() & MetaGraphDM::DX_DATAMAPS) == MetaGraphDM::DX_DATAMAPS)) {
        bounds = getDisplayedDataMap().getRegion();
    }
    return bounds;
}

// note: 0 is not at all editable, 1 is editable off and 2 is editable on
int MetaGraphDM::isEditable() const {
    int editable = 0;
    switch (m_viewClass & DX_VIEWFRONT) {
    case DX_VIEWVGA:
        if (getDisplayedLatticeMap().getInternalMap().isProcessed()) {
            editable = DX_NOT_EDITABLE;
        } else {
            editable = DX_EDITABLE_ON;
        }
        break;
    case DX_VIEWAXIAL: {
        int type = getDisplayedShapeGraph().getInternalMap().getMapType();
        if (type != ShapeMap::SEGMENTMAP && type != ShapeMap::ALLLINEMAP) {
            editable = getDisplayedShapeGraph().isEditable() ? DX_EDITABLE_ON : DX_EDITABLE_OFF;
        } else {
            editable = DX_NOT_EDITABLE;
        }
    } break;
    case DX_VIEWDATA:
        editable = getDisplayedDataMap().isEditable() ? DX_EDITABLE_ON : DX_EDITABLE_OFF;
        break;
    }
    return editable;
}

bool MetaGraphDM::canUndo() const {
    bool canundo = false;
    switch (m_viewClass & DX_VIEWFRONT) {
    case DX_VIEWVGA:
        canundo = getDisplayedLatticeMap().canUndo();
        break;
    case DX_VIEWAXIAL:
        canundo = getDisplayedShapeGraph().canUndo();
        break;
    case DX_VIEWDATA:
        canundo = getDisplayedDataMap().canUndo();
        break;
    }
    return canundo;
}

void MetaGraphDM::undo() {
    switch (m_viewClass & DX_VIEWFRONT) {
    case DX_VIEWVGA:
        getDisplayedLatticeMap().undoPoints();
        break;
    case DX_VIEWAXIAL:
        getDisplayedShapeGraph().undo();
        break;
    case DX_VIEWDATA:
        getDisplayedDataMap().undo();
        break;
    }
}

// Moving to global ways of doing things:

std::optional<size_t> MetaGraphDM::addAttribute(const std::string &name) {
    std::optional<size_t> col = std::nullopt;
    switch (m_viewClass & DX_VIEWFRONT) {
    case DX_VIEWVGA:
        col = getDisplayedLatticeMap().getInternalMap().addAttribute(name);
        break;
    case DX_VIEWAXIAL:
        col = getDisplayedShapeGraph().getInternalMap().addAttribute(name);
        break;
    case DX_VIEWDATA:
        col = getDisplayedDataMap().getInternalMap().addAttribute(name);
        break;
    }
    return col;
}

void MetaGraphDM::removeAttribute(size_t col) {
    switch (m_viewClass & DX_VIEWFRONT) {
    case DX_VIEWVGA:
        getDisplayedLatticeMap().getInternalMap().removeAttribute(col);
        break;
    case DX_VIEWAXIAL:
        getDisplayedShapeGraph().getInternalMap().removeAttribute(col);
        break;
    case DX_VIEWDATA:
        getDisplayedDataMap().getInternalMap().removeAttribute(col);
        break;
    }
}

bool MetaGraphDM::isAttributeLocked(size_t col) {
    return getAttributeTable(m_viewClass).getColumn(col).isLocked();
}

int MetaGraphDM::getDisplayedAttribute() const {
    int col = -1;
    switch (m_viewClass & DX_VIEWFRONT) {
    case DX_VIEWVGA:
        col = getDisplayedLatticeMap().getDisplayedAttribute();
        break;
    case DX_VIEWAXIAL:
        col = getDisplayedShapeGraph().getDisplayedAttribute();
        break;
    case DX_VIEWDATA:
        col = getDisplayedDataMap().getDisplayedAttribute();
        break;
    }
    return col;
}

// this is coming from the front end, so force override:
void MetaGraphDM::setDisplayedAttribute(int col) {
    switch (m_viewClass & DX_VIEWFRONT) {
    case DX_VIEWVGA:
        getDisplayedLatticeMap().overrideDisplayedAttribute(-2);
        getDisplayedLatticeMap().setDisplayedAttribute(col);
        break;
    case DX_VIEWAXIAL:
        getDisplayedShapeGraph().overrideDisplayedAttribute(-2);
        getDisplayedShapeGraph().setDisplayedAttribute(col);
        break;
    case DX_VIEWDATA:
        getDisplayedDataMap().overrideDisplayedAttribute(-2);
        getDisplayedDataMap().setDisplayedAttribute(col);
        break;
    }
}

// const and non-const versions:

AttributeTable &MetaGraphDM::getAttributeTable(std::optional<size_t> type,
                                               std::optional<size_t> layer) {
    AttributeTable *tab = nullptr;
    if (!type.has_value()) {
        type = m_viewClass;
    }
    switch (type.value() & DX_VIEWFRONT) {
    case DX_VIEWVGA:
        tab = (!layer.has_value()) ? &(getDisplayedLatticeMap().getAttributeTable())
                                   : &(m_latticeMaps[layer.value()].getAttributeTable());
        break;
    case DX_VIEWAXIAL:
        tab = (!layer.has_value()) ? &(getDisplayedShapeGraph().getAttributeTable())
                                   : &(m_shapeGraphs[layer.value()].getAttributeTable());
        break;
    case DX_VIEWDATA:
        tab = (!layer.has_value()) ? &(getDisplayedDataMap().getAttributeTable())
                                   : &(m_dataMaps[layer.value()].getAttributeTable());
        break;
    }
    return *tab;
}

const AttributeTable &MetaGraphDM::getAttributeTable(std::optional<size_t> type,
                                                     std::optional<size_t> layer) const {
    const AttributeTable *tab = nullptr;
    if (!type.has_value()) {
        type = m_viewClass & DX_VIEWFRONT;
    }
    switch (type.value()) {
    case DX_VIEWVGA:
        tab = (!layer.has_value()) ? &(getDisplayedLatticeMap().getAttributeTable())
                                   : &(m_latticeMaps[layer.value()].getAttributeTable());
        break;
    case DX_VIEWAXIAL:
        tab = (!layer.has_value()) ? &(getDisplayedShapeGraph().getAttributeTable())
                                   : &(m_shapeGraphs[layer.value()].getAttributeTable());
        break;
    case DX_VIEWDATA:
        tab = (!layer.has_value()) ? &(getDisplayedDataMap().getAttributeTable())
                                   : &(m_dataMaps[layer.value()].getAttributeTable());
        break;
    }
    return *tab;
}

MetaGraphReadWrite::ReadWriteStatus MetaGraphDM::readFromFile(const std::string &filename) {

    if (filename.empty()) {
        return MetaGraphReadWrite::ReadWriteStatus::NOT_A_GRAPH;
    }

#ifdef _WIN32
    std::ifstream stream(filename.c_str(), std::ios::binary | std::ios::in);
#else
    std::ifstream stream(filename.c_str(), std::ios::in);
#endif
    auto result = readFromStream(stream, filename);
    stream.close();
    return result;
}

MetaGraphReadWrite::ReadWriteStatus MetaGraphDM::readFromStream(std::istream &stream,
                                                                const std::string &) {
    m_state = 0; // <- clear the state out

    // clear BSP tree if it exists:
    m_bspNodeTree.destroy();

    try {
        auto mgd = MetaGraphReadWrite::readFromStream(stream);
        m_readStatus = mgd.readWriteStatus;

        auto &dd = mgd.displayData;

        m_metaGraph = mgd.metaGraph;
        {
            auto gddIt = dd.perDrawingMap.begin();
            for (auto &&mapGroup : mgd.drawingFiles) {
                m_drawingFiles.emplace_back(mapGroup.first.name);
                m_drawingFiles.back().groupData.setRegion(mapGroup.first.region);
                auto ddIt = gddIt->begin();
                for (auto &&map : mapGroup.second) {
                    m_drawingFiles.back().maps.emplace_back(
                        std::make_unique<ShapeMap>(std::move(map)));
                    auto &newMapDM = m_drawingFiles.back().maps.back();
                    newMapDM.setEditable(std::get<0>(*ddIt));
                    newMapDM.setShow(std::get<1>(*ddIt));
                    newMapDM.setDisplayedAttribute(std::get<2>(*ddIt));
                    ddIt++;
                }
                gddIt++;
            }
            gddIt++;
        }
        {
            auto ddIt = dd.perLatticeMap.begin();
            for (auto &&map : mgd.latticeMaps) {
                m_latticeMaps.emplace_back(std::make_unique<LatticeMap>(std::move(map)));
                auto &newMapDM = m_latticeMaps.back();
                newMapDM.setDisplayedAttribute(*ddIt);
                ddIt++;
            }
        }
        {
            auto ddIt = dd.perDataMap.begin();
            for (auto &&map : mgd.dataMaps) {
                m_dataMaps.emplace_back(std::make_unique<ShapeMap>(std::move(map)));
                auto &newMapDM = m_dataMaps.back();
                newMapDM.setEditable(std::get<0>(*ddIt));
                newMapDM.setShow(std::get<1>(*ddIt));
                newMapDM.setDisplayedAttribute(std::get<2>(*ddIt));
                ddIt++;
            }
        }
        {
            auto ddIt = dd.perShapeGraph.begin();
            for (auto &&map : mgd.shapeGraphs) {
                m_shapeGraphs.emplace_back(std::make_unique<ShapeGraph>(std::move(map)));
                auto &newMapDM = m_shapeGraphs.back();
                newMapDM.setEditable(std::get<0>(*ddIt));
                newMapDM.setShow(std::get<1>(*ddIt));
                newMapDM.setDisplayedAttribute(std::get<2>(*ddIt));
                ddIt++;
            }
        }
        m_allLineMapData = mgd.allLineMapData;

        m_state = dd.state;
        m_viewClass = dd.viewClass;
        m_showGrid = dd.showGrid;
        m_showText = dd.showText;
        if (!dd.displayedLatticeMap.has_value()) {
            if (!mgd.dataMaps.empty()) {
                setViewClass(DX_SHOWVGATOP);
                m_displayedLatticeMap = 0;
            } else {
                m_displayedLatticeMap = std::nullopt;
            }
        } else {
            m_displayedLatticeMap = dd.displayedLatticeMap;
        }
        if (!dd.displayedDataMap.has_value()) {
            if (!mgd.dataMaps.empty()) {
                setViewClass(DX_SHOWSHAPETOP);
                m_displayedDatamap = 0;
            } else {
                m_displayedDatamap = std::nullopt;
            }
        } else {
            m_displayedDatamap = dd.displayedDataMap;
        }

        if (!dd.displayedShapeGraph.has_value()) {
            if (!mgd.dataMaps.empty()) {
                setViewClass(DX_SHOWAXIALTOP);
                m_displayedShapegraph = 0;
            } else {
                m_displayedShapegraph = std::nullopt;
            }
        } else {
            m_displayedShapegraph = dd.displayedShapeGraph;
        }
    } catch (MetaGraphReadWrite::MetaGraphReadError &e) {
        std::cerr << "MetaGraph reading failed: " << e.what() << std::endl;
    }
    return MetaGraphReadWrite::ReadWriteStatus::OK;
}

MetaGraphReadWrite::ReadWriteStatus MetaGraphDM::write(const std::string &filename, int version,
                                                       bool currentlayer, bool ignoreDisplayData) {
    std::ofstream stream;

    int oldstate = m_state;
    m_state = 0; // <- temporarily clear out state, avoids any potential read /
                 // write errors

    // editable, show, displayed attribute
    typedef std::tuple<bool, bool, int> ShapeMapDisplayData;

    std::vector<std::pair<ShapeMapGroupData, std::vector<std::reference_wrapper<ShapeMap>>>>
        drawingFiles;
    std::vector<std::vector<ShapeMapDisplayData>> perDrawingMap;
    std::vector<std::reference_wrapper<LatticeMap>> latticeMaps;
    std::vector<int> perLatticeMap;
    std::vector<std::reference_wrapper<ShapeMap>> dataMaps;
    std::vector<ShapeMapDisplayData> perDataMap;
    std::vector<std::reference_wrapper<ShapeGraph>> shapeGraphs;
    std::vector<ShapeMapDisplayData> perShapeGraph;

    for (auto &mapGroupDM : m_drawingFiles) {
        drawingFiles.push_back(std::make_pair(mapGroupDM.groupData.getInternalData(),
                                              std::vector<std::reference_wrapper<ShapeMap>>()));
        perDrawingMap.emplace_back();
        auto &newMapGroup = drawingFiles.back();
        auto &newDisplayData = perDrawingMap.back();
        for (auto &mapDM : mapGroupDM.maps) {
            newMapGroup.second.push_back(mapDM.getInternalMap());
            newDisplayData.push_back(std::make_tuple(mapDM.isEditable(), mapDM.isShown(),
                                                     mapDM.getDisplayedAttribute()));
        }
    }
    for (auto &mapDM : m_latticeMaps) {
        latticeMaps.push_back(mapDM.getInternalMap());
        perLatticeMap.push_back(mapDM.getDisplayedAttribute());
    }
    for (auto &mapDM : m_dataMaps) {
        dataMaps.push_back(mapDM.getInternalMap());
        perDataMap.push_back(
            std::make_tuple(mapDM.isEditable(), mapDM.isShown(), mapDM.getDisplayedAttribute()));
    }
    for (auto &mapDM : m_shapeGraphs) {
        shapeGraphs.push_back(mapDM.getInternalMap());
        perShapeGraph.push_back(
            std::make_tuple(mapDM.isEditable(), mapDM.isShown(), mapDM.getDisplayedAttribute()));
    }

    if (!ignoreDisplayData) {

        int tempState = 0, tempViewClass = 0;
        if (currentlayer) {
            if (m_viewClass & DX_VIEWVGA) {
                tempState = DX_LATTICEMAPS;
                tempViewClass = DX_VIEWVGA;
            } else if (m_viewClass & DX_VIEWAXIAL) {
                tempState = DX_SHAPEGRAPHS;
                tempViewClass = DX_VIEWAXIAL;
            } else if (m_viewClass & DX_VIEWDATA) {
                tempState = DX_DATAMAPS;
                tempViewClass = DX_VIEWDATA;
            }
        } else {
            tempState = oldstate;
            tempViewClass = m_viewClass;
        }

        MetaGraphReadWrite::writeToFile(
            filename, // MetaGraph Data
            version, m_metaGraph.name, m_metaGraph.region, m_metaGraph.fileProperties, drawingFiles,
            latticeMaps, dataMaps, shapeGraphs, m_allLineMapData,
            // display data
            tempState, tempViewClass, m_showGrid, m_showText, perDrawingMap,
            m_displayedLatticeMap.has_value()
                ? std::make_optional(static_cast<unsigned int>(*m_displayedLatticeMap))
                : std::nullopt,
            perLatticeMap,
            m_displayedDatamap.has_value()
                ? std::make_optional(static_cast<unsigned int>(*m_displayedDatamap))
                : std::nullopt,
            perDataMap,
            m_displayedShapegraph.has_value()
                ? std::make_optional(static_cast<unsigned int>(*m_displayedShapegraph))
                : std::nullopt,
            perShapeGraph);
    } else {
        MetaGraphReadWrite::writeToFile(filename, // MetaGraph Data
                                        version, m_metaGraph.name, m_metaGraph.region,
                                        m_metaGraph.fileProperties, drawingFiles, latticeMaps,
                                        dataMaps, shapeGraphs, m_allLineMapData);
    }
    m_state = oldstate;
    return MetaGraphReadWrite::ReadWriteStatus::OK;
}

std::streampos MetaGraphDM::skipVirtualMem(std::istream &stream) {
    // it's graph virtual memory: skip it
    int nodes = -1;
    stream.read(reinterpret_cast<char *>(&nodes), sizeof(nodes));

    nodes *= 2;

    for (int i = 0; i < nodes; i++) {
        int connections;
        stream.read(reinterpret_cast<char *>(&connections), sizeof(connections));
        stream.seekg(stream.tellg() +
                     std::streamoff(static_cast<size_t>(connections) * sizeof(connections)));
    }
    return (stream.tellg());
}

std::vector<SimpleLine> MetaGraphDM::getVisibleDrawingLines() {

    std::vector<SimpleLine> lines;

    auto shownMaps = getShownDrawingMaps();
    for (const auto &pixel : shownMaps) {
        const std::vector<SimpleLine> &newLines =
            pixel.first.get().getInternalMap().getAllShapesAsSimpleLines();
        lines.insert(std::end(lines), std::begin(newLines), std::end(newLines));
    }
    return lines;
}

size_t MetaGraphDM::addNewLatticeMap(const std::string &name) {
    std::string myname = name;
    int counter = 1;
    bool duplicate = true;
    while (duplicate) {
        duplicate = false;
        for (auto &latticeMap : m_latticeMaps) {
            if (latticeMap.getName() == myname) {
                duplicate = true;
                myname = dXstring::formatString(counter++, name + " %d");
                break;
            }
        }
    }
    m_latticeMaps.push_back(std::make_unique<LatticeMap>(m_metaGraph.region, myname));
    setDisplayedLatticeMapRef(m_latticeMaps.size() - 1);
    return m_latticeMaps.size() - 1;
}

void MetaGraphDM::makeViewportShapes(const Region4f &viewport) const {
    currentLayer = -1;
    size_t di = m_drawingFiles.size() - 1;
    for (auto iter = m_drawingFiles.rbegin(); iter != m_drawingFiles.rend(); iter++) {
        if (isShown(*iter)) {
            currentLayer = (int)di;

            iter->groupData.invalidateCurrentLayer();
            for (size_t i = iter->maps.size() - 1; static_cast<int>(i) != -1; i--) {
                if (iter->maps[i].isShown()) {
                    if (!iter->maps[i].isValid()) {
                        continue;
                    }
                    iter->groupData.setCurrentLayer(i);
                    iter->maps[i].makeViewportShapes(
                        (viewport.atZero() ? m_metaGraph.region : viewport));
                }
            }
        }
        di--;
    }
}

bool MetaGraphDM::findNextShape(const ShapeMapGroup &spf, bool &nextlayer) const {
    if (!spf.groupData.hasCurrentLayer())
        return false;

    while (spf.groupData.getCurrentLayer().has_value() &&
           spf.maps[spf.groupData.getCurrentLayer().value()].valid() &&
           !spf.maps[spf.groupData.getCurrentLayer().value()].findNextShape(nextlayer)) {
        spf.groupData.setCurrentLayer(spf.groupData.getCurrentLayer().value() + 1);
        while (spf.groupData.getCurrentLayer().has_value() &&
               spf.groupData.getCurrentLayer().value() < spf.maps.size() &&
               !spf.maps[spf.groupData.getCurrentLayer().value()].isShown())
            spf.groupData.setCurrentLayer(spf.groupData.getCurrentLayer().value() + 1);
        if (spf.groupData.getCurrentLayer().value() == spf.maps.size()) {
            spf.groupData.invalidateCurrentLayer();
            return false;
        }
    }
    return true;
}

bool MetaGraphDM::findNextShape(bool &nextlayer) const {
    if (!currentLayer.has_value())
        return false;
    while (!findNextShape(m_drawingFiles[currentLayer.value()], nextlayer)) {
        while (++(*currentLayer) < m_drawingFiles.size() &&
               !isShown(m_drawingFiles[currentLayer.value()]))
            ;
        if (currentLayer == m_drawingFiles.size()) {
            currentLayer = std::nullopt;
            return false;
        }
    }
    return true;
}
