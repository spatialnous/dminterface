// SPDX-FileCopyrightText: 2000-2010 University College London, Alasdair Turner
// SPDX-FileCopyrightText: 2011-2012 Tasos Varoudis
// SPDX-FileCopyrightText: 2024 Petros Koutsolampros
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Interface: the meta graph loads and holds all sorts of arbitrary data...
#include "pointmapdm.hpp"
#include "salalib/analysistype.hpp"
#include "salalib/radiustype.hpp"
#include "shapegraphdm.hpp"
#include "shapemapdm.hpp"
#include "shapemapgroupdatadm.hpp"

#include "salalib/bspnodetree.hpp"
#include "salalib/ianalysis.hpp"
#include "salalib/importtypedefs.hpp"
#include "salalib/isovist.hpp"
#include "salalib/metagraph.hpp"
#include "salalib/metagraphreadwrite.hpp"
#include "salalib/pushvalues.hpp"

#include <memory>
#include <optional>
#include <vector>

///////////////////////////////////////////////////////////////////////////////////

class Communicator;

// A meta graph is precisely what it says it is

class MetaGraphDM {
    MetaGraph m_metaGraph;

    MetaGraphReadWrite::ReadWriteStatus m_readStatus = MetaGraphReadWrite::ReadWriteStatus::OK;
    int m_state;
    int m_viewClass;
    bool m_showGrid;
    bool m_showText;

    struct ShapeMapGroup {
        ShapeMapGroupDataDM groupData;
        std::vector<ShapeMapDM> maps;
        ShapeMapGroup(const std::string &groupName) : groupData(groupName) {}
        ShapeMapGroup(ShapeMapGroup &&other)
            : groupData(std::move(other.groupData)), maps(std::move(other.maps)) {}
        ShapeMapGroup &operator=(ShapeMapGroup &&other) = default;
        size_t addMap(std::unique_ptr<ShapeMap> &&shapeMap) {
            maps.push_back(std::move(shapeMap));
            if (maps.size() == 1) {
                groupData.setRegion(maps.back().getRegion());
            } else {
                groupData.setRegion(groupData.getRegion().runion(maps.back().getRegion()));
            }
            return maps.size() - 1;
        }
    };
    std::vector<ShapeMapGroup> m_drawingFiles;
    std::vector<ShapeMapDM> m_dataMaps;
    std::vector<ShapeGraphDM> m_shapeGraphs;
    std::vector<PointMapDM> m_pointMaps;
    std::optional<decltype(MetaGraphDM::m_dataMaps)::size_type> m_displayedDatamap = std::nullopt;
    std::optional<decltype(MetaGraphDM::m_pointMaps)::size_type> m_displayedPointmap = std::nullopt;
    std::optional<decltype(MetaGraphDM::m_shapeGraphs)::size_type> m_displayedShapegraph =
        std::nullopt;

    BSPNodeTree m_bspNodeTree;

  public:
    MetaGraphDM(std::string name = "");
    MetaGraphDM(MetaGraphDM &&other)
        : m_state(), m_viewClass(), m_showGrid(), m_showText(),
          m_drawingFiles(std::move(other.m_drawingFiles)), m_dataMaps(std::move(other.m_dataMaps)),
          m_shapeGraphs(std::move(other.m_shapeGraphs)), m_pointMaps(std::move(other.m_pointMaps)),
          currentLayer(std::nullopt) {}
    MetaGraphDM &operator=(MetaGraphDM &&other) = default;
    ~MetaGraphDM() {}

  public:
    enum {
        DX_SHOWHIDEVGA = 0x0100,
        DX_SHOWVGATOP = 0x0200,
        DX_SHOWHIDEAXIAL = 0x0400,
        DX_SHOWAXIALTOP = 0x0800,
        DX_SHOWHIDESHAPE = 0x1000,
        DX_SHOWSHAPETOP = 0x2000
    };
    enum {
        DX_VIEWNONE = 0x00,
        DX_VIEWVGA = 0x01,
        DX_VIEWBACKVGA = 0x02,
        DX_VIEWAXIAL = 0x04,
        DX_VIEWBACKAXIAL = 0x08,
        DX_VIEWDATA = 0x20,
        DX_VIEWBACKDATA = 0x40,
        DX_VIEWFRONT = 0x25
    };
    enum {
        DX_ADD = 0x0001,
        DX_REPLACE = 0x0002,
        DX_CAT = 0x0010,
        DX_DXF = 0x0020,
        DX_NTF = 0x0040,
        DX_RT1 = 0x0080,
        DX_GML = 0x0100
    };
    enum {
        DX_NONE = 0x0000,
        DX_POINTMAPS = 0x0002,
        DX_LINEDATA = 0x0004,
        DX_ANGULARGRAPH = 0x0010,
        DX_DATAMAPS = 0x0020,
        DX_AXIALLINES = 0x0040,
        DX_SHAPEGRAPHS = 0x0100,
        DX_BUGGY = 0x8000
    };
    enum { DX_NOT_EDITABLE = 0, DX_EDITABLE_OFF = 1, DX_EDITABLE_ON = 2 };

    std::string &getName() { return m_metaGraph.name; }
    const Region4f &getRegion() { return m_metaGraph.region; }
    void setRegion(Point2f &bottomLeft, Point2f &topRight) {
        m_metaGraph.region.bottomLeft = bottomLeft;
        m_metaGraph.region.topRight = topRight;
    }
    MetaGraphReadWrite::ReadWriteStatus getReadStatus() const { return m_readStatus; }
    bool isShown() const {
        for (auto &drawingFile : m_drawingFiles)
            for (auto &map : drawingFile.maps)
                if (map.isShown())
                    return true;
        return false;
    }
    auto &getInternalData() { return m_metaGraph; }
    auto &getFileProperties() { return m_metaGraph.fileProperties; }
    auto &getDrawingFiles() { return m_drawingFiles; }
    // P.K. The MetaGraph file format does not really store enough information
    // about the ShapeMap groups (drawing files with their layers as ShapeMaps)
    // so we resort to just checking if all the group maps are visible. Perhaps
    // ShapeMapGroupDataDM should also have an m_show variable
    bool isShown(const ShapeMapGroup &spf) const {
        for (auto &pixel : spf.maps)
            if (pixel.isShown())
                return true;
        return false;
    }

    // TODO: drawing state functions/fields that should be eventually removed
    void makeViewportShapes(const Region4f &viewport) const;
    bool findNextShape(const ShapeMapGroup &spf, bool &nextlayer) const;
    bool findNextShape(bool &nextlayer) const;
    const SalaShape &getNextShape() const {
        if (!currentLayer.has_value()) {
            throw new genlib::RuntimeException("No current layer selected");
        }
        auto &currentDrawingFile = m_drawingFiles[currentLayer.value()];
        if (!currentDrawingFile.groupData.getCurrentLayer().has_value()) {
            throw new genlib::RuntimeException("Current drawing file (" +
                                               currentDrawingFile.groupData.getName() +
                                               ") has no layer to match to a map");
        }
        return currentDrawingFile.maps[currentDrawingFile.groupData.getCurrentLayer().value()]
            .getNextShape();
    }
    mutable std::optional<size_t> currentLayer;

  public:
    int getVersion() {
        // note, if unsaved, m_file_version is -1
        return m_metaGraph.version;
    }

    std::vector<PointMapDM> &getPointMaps() { return m_pointMaps; }
    bool hasDisplayedPointMap() const { return m_displayedPointmap.has_value(); }
    PointMapDM &getDisplayedPointMap() { return m_pointMaps[m_displayedPointmap.value()]; }
    const PointMapDM &getDisplayedPointMap() const {
        return m_pointMaps[m_displayedPointmap.value()];
    }
    void setDisplayedPointMapRef(decltype(MetaGraphDM::m_pointMaps)::size_type map) {
        if (m_displayedPointmap.has_value() && m_displayedPointmap != map)
            getDisplayedPointMap().clearSel();
        m_displayedPointmap = map;
    }
    auto getDisplayedPointMapRef() const { return m_displayedPointmap.value(); }
    void redoPointMapBlockLines() // (flags blockedlines, but also flags that you need to rebuild a
                                  // bsp tree if you have one)
    {
        for (auto &pointMap : m_pointMaps) {
            pointMap.getInternalMap().resetBlockedLines();
        }
    }
    size_t addNewPointMap(const std::string &name = std::string("VGA Map"));

  private:
    // helpful to know this for creating fewest line maps, although has to be reread at input
    std::optional<AllLine::MapData> m_allLineMapData = std::nullopt;

    void removePointMap(size_t i) {
        if (m_displayedPointmap.has_value()) {
            if (m_pointMaps.size() == 1)
                m_displayedPointmap = std::nullopt;
            else if (m_displayedPointmap.value() != 0 && m_displayedPointmap.value() >= i)
                m_displayedPointmap.value()--;
        }
        m_pointMaps.erase(std::next(m_pointMaps.begin(), static_cast<int>(i)));
    }

  public:
    int getState() const { return m_state; }
    // use with caution: only very rarely needed outside MetaGraph itself
    void setState(int state) { m_state = state; }
    size_t loadLineData(Communicator *communicator, const std::string &fileName, int loadType);
    size_t loadLineData(Communicator *communicator, const std::string &fileName,
                        sala::ImportFileType importFileType, bool replace);
    size_t addDrawingFile(std::string name, std::vector<ShapeMap> &&maps);
    ShapeMapDM &createNewShapeMap(sala::ImportType mapType, std::string name);
    void deleteShapeMap(sala::ImportType mapType, ShapeMapDM &shapeMap);
    void updateParentRegions(ShapeMap &shapeMap);
    bool clearPoints();
    bool setGrid(double spacing, const Point2f &offset = Point2f()); // override of PointMap
    void setShowGrid(bool showGrid) { m_showGrid = showGrid; }
    bool getShowGrid() const { return m_showGrid; }
    void setShowText(bool showText) { m_showText = showText; }
    bool getShowText() const { return m_showText; }
    bool makePoints(const Point2f &p, int semifilled,
                    Communicator *communicator = nullptr); // override of PointMap
    bool hasVisibleDrawingShapes();
    std::vector<std::pair<std::reference_wrapper<const ShapeMapDM>, int>> getShownDrawingMaps();
    std::vector<std::pair<std::reference_wrapper<const ShapeMap>, int>>
    getAsInternalMaps(std::vector<std::pair<std::reference_wrapper<const ShapeMapDM>, int>> maps);
    std::vector<Line4f> getShownDrawingFilesAsLines();
    std::vector<SalaShape> getShownDrawingFilesAsShapes();
    bool makeGraph(Communicator *communicator, int algorithm, double maxdist);
    bool unmakeGraph(bool removeLinks);
    bool analyseGraph(Communicator *communicator, int pointDepthSelection, AnalysisType outputType,
                      int local, bool gatesOnly, int global, double radius, bool simpleVersion);
    //
    // helpers for editing maps
    bool isEditableMap();
    ShapeMapDM &getEditableMap();
    // currently only making / moving lines, but should be able to extend this to polys fairly
    // easily:
    bool makeShape(const Line4f &line);
    bool moveSelShape(const Line4f &line);
    // onto polys as well:
    int polyBegin(const Line4f &line);
    bool polyAppend(int shapeRef, const Point2f &point);
    bool polyClose(int shapeRef);
    bool polyCancel(int shapeRef);
    //
    size_t addShapeGraph(ShapeGraphDM &&shapeGraph);
    size_t addShapeGraph(ShapeGraph &&shapeGraph);
    size_t addShapeGraph(const std::string &name, int type);
    size_t addShapeMap(const std::string &name);
    void removeDisplayedMap();
    //
    // various map conversions
    bool convertDrawingToAxial(Communicator *comm,
                               std::string layerName); // n.b., name copied for thread safety
    bool convertDataToAxial(Communicator *comm, std::string layerName, bool keeporiginal,
                            bool pushvalues);
    bool convertDrawingToSegment(Communicator *comm, std::string layerName);
    bool convertDataToSegment(Communicator *comm, std::string layerName, bool keeporiginal,
                              bool pushvalues);
    bool convertToData(Communicator *, std::string layerName, bool keeporiginal, int shapeMapType,
                       bool copydata);
    bool convertToDrawing(Communicator *, std::string layerName, bool fromDisplayedDataMap);
    bool convertToConvex(Communicator *comm, std::string layerName, bool keeporiginal,
                         int shapeMapType, bool copydata);
    bool convertAxialToSegment(Communicator *comm, std::string layerName, bool keeporiginal,
                               bool pushvalues, double stubremoval);
    int loadMifMap(Communicator *comm, std::istream &miffile, std::istream &midfile);
    bool makeAllLineMap(Communicator *communicator, const Point2f &seed);
    bool makeFewestLineMap(Communicator *communicator, int replace);
    bool analyseAxial(Communicator *communicator, std::set<double> radiusSet,
                      int weightedMeasureCol, bool choice, bool fulloutput, bool localAnalysis,
                      bool forceLegacyColumnOrder = false);
    bool analyseSegmentsTulip(Communicator *communicator, std::set<double> &radiusSet, bool selOnly,
                              int tulipBins, int weightedMeasureCol, RadiusType radiusType,
                              bool choice, int weightedMeasureCol2 = -1, int routeweightCol = -1,
                              bool interactive = false, bool forceLegacyColumnOrder = false);
    bool analyseSegmentsAngular(Communicator *communicator, std::set<double> radiusSet);
    bool analyseTopoMetMultipleRadii(Communicator *communicator, std::set<double> &radiusSet,
                                     AnalysisType outputType, double radius, bool selOnly);
    bool analyseTopoMet(Communicator *communicator, AnalysisType outputType, double radius,
                        bool selOnly);
    //
    bool hasAllLineMap() { return m_allLineMapData.has_value(); }
    bool hasFewestLineMaps() {
        for (const auto &shapeGraph : m_shapeGraphs) {
            if (shapeGraph.getName() == "Fewest-Line Map (Subsets)" ||
                shapeGraph.getName() == "Fewest Line Map (Subsets)" ||
                shapeGraph.getName() == "Fewest-Line Map (Minimal)" ||
                shapeGraph.getName() == "Fewest Line Map (Minimal)") {
                return true;
            }
        }
        return false;
    }
    bool pushValuesToLayer(int desttype, size_t destlayer, PushValues::Func pushFunc,
                           bool countCol = false);
    bool pushValuesToLayer(int sourcetype, size_t sourcelayer, int desttype, size_t destlayer,
                           std::optional<size_t> colIn, size_t colOut, PushValues::Func pushFunc,
                           bool createCountCol = false);
    //
    std::optional<size_t> getDisplayedMapRef() const;
    //
    // NB -- returns 0 (not editable), 1 (editable off) or 2 (editable on)
    int isEditable() const;
    bool canUndo() const;
    void undo();

    bool hasDisplayedDataMap() const { return m_displayedDatamap.has_value(); }
    ShapeMapDM &getDisplayedDataMap() { return m_dataMaps[m_displayedDatamap.value()]; }
    const ShapeMapDM &getDisplayedDataMap() const { return m_dataMaps[m_displayedDatamap.value()]; }
    auto getDisplayedDataMapRef() const { return m_displayedDatamap.value(); }

    void removeDataMap(size_t i) {
        if (m_displayedDatamap.has_value()) {
            if (m_dataMaps.size() == 1)
                m_displayedDatamap = std::nullopt;
            else if (m_displayedDatamap.value() != 0 && m_displayedDatamap.value() >= i)
                m_displayedDatamap.value()--;
        }
        m_dataMaps.erase(std::next(m_dataMaps.begin(), static_cast<int>(i)));
    }

    void setDisplayedDataMapRef(decltype(MetaGraphDM::m_dataMaps)::size_type map) {
        if (m_displayedDatamap.has_value() && m_displayedDatamap != map)
            getDisplayedDataMap().clearSel();
        m_displayedDatamap = map;
    }

    template <class T>
    std::optional<size_t> getMapRef(std::vector<T> &maps, const std::string &name) const {
        // note, only finds first map with this name
        for (size_t i = 0; i < maps.size(); i++) {
            if (maps[i].getName() == name)
                return std::optional<size_t>{i};
        }
        return std::nullopt;
    }

    std::vector<ShapeGraphDM> &getShapeGraphs() { return m_shapeGraphs; }
    bool hasDisplayedShapeGraph() const { return m_displayedShapegraph.has_value(); }
    ShapeGraphDM &getDisplayedShapeGraph() { return m_shapeGraphs[m_displayedShapegraph.value()]; }
    const ShapeGraphDM &getDisplayedShapeGraph() const {
        return m_shapeGraphs[m_displayedShapegraph.value()];
    }
    void unsetDisplayedShapeGraphRef() { m_displayedShapegraph = std::nullopt; }
    void setDisplayedShapeGraphRef(decltype(MetaGraphDM::m_shapeGraphs)::size_type map) {
        if (m_displayedShapegraph.has_value() && m_displayedShapegraph != map)
            getDisplayedShapeGraph().clearSel();
        m_displayedShapegraph = map;
    }
    auto getDisplayedShapeGraphRef() const { return m_displayedShapegraph.value(); }

    void removeShapeGraph(size_t i) {
        if (m_displayedShapegraph.has_value()) {
            if (m_shapeGraphs.size() == 1)
                m_displayedShapegraph = std::nullopt;
            else if (m_displayedShapegraph.value() > 0 && m_displayedShapegraph.value() >= i)
                m_displayedShapegraph.value()--;
        }
        m_shapeGraphs.erase(std::next(m_shapeGraphs.begin(), static_cast<int>(i)));
    }

    std::vector<ShapeMapDM> &getDataMaps() { return m_dataMaps; }

    //
    int getDisplayedMapType();
    AttributeTable &getDisplayedMapAttributes();
    bool hasVisibleDrawingLayers();
    Region4f getBoundingBox() const;
    //
    int getDisplayedAttribute() const;
    void setDisplayedAttribute(int col);
    std::optional<size_t> addAttribute(const std::string &name);
    void removeAttribute(size_t col);
    bool isAttributeLocked(size_t col);
    AttributeTable &getAttributeTable(std::optional<size_t> type = std::nullopt,
                                      std::optional<size_t> layer = std::nullopt);
    const AttributeTable &getAttributeTable(std::optional<size_t> type = std::nullopt,
                                            std::optional<size_t> layer = std::nullopt) const;

    int getLineFileCount() const { return static_cast<int>(m_drawingFiles.size()); }

    const std::string &getLineFileName(size_t fileIdx) const {
        return m_drawingFiles[fileIdx].groupData.getName();
    }
    size_t getLineLayerCount(size_t fileIdx) const { return m_drawingFiles[fileIdx].maps.size(); }

    ShapeMapDM &getLineLayer(size_t fileIdx, size_t layerIdx) {
        return m_drawingFiles[fileIdx].maps[layerIdx];
    }
    const ShapeMapDM &getLineLayer(size_t fileIdx, size_t layerIdx) const {
        return m_drawingFiles[fileIdx].maps[layerIdx];
    }
    int getViewClass() { return m_viewClass; }
    // These functions make specifying conditions to do things much easier:
    bool viewingNone() { return (m_viewClass == DX_VIEWNONE); }
    bool viewingProcessed() {
        return (
            (m_viewClass & (DX_VIEWAXIAL | DX_VIEWDATA)) ||
            (m_viewClass & DX_VIEWVGA && getDisplayedPointMap().getInternalMap().isProcessed()));
    }
    bool viewingShapes() { return (m_viewClass & (DX_VIEWAXIAL | DX_VIEWDATA)) != 0; }
    bool viewingProcessedLines() { return ((m_viewClass & DX_VIEWAXIAL) == DX_VIEWAXIAL); }
    bool viewingProcessedShapes() { return ((m_viewClass & DX_VIEWDATA) == DX_VIEWDATA); }
    bool viewingProcessedPoints() {
        return ((m_viewClass & DX_VIEWVGA) &&
                getDisplayedPointMap().getInternalMap().isProcessed());
    }
    bool viewingUnprocessedPoints() {
        return ((m_viewClass & DX_VIEWVGA) &&
                !getDisplayedPointMap().getInternalMap().isProcessed());
    }
    //
    bool setViewClass(int command);
    //
    double getLocationValue(const Point2f &point);
    //
  public:
    // these are dependent on what the view class is:
    bool isSelected() // does a selection exist
    {
        if (m_viewClass & DX_VIEWVGA)
            return getDisplayedPointMap().isSelected();
        else if (m_viewClass & DX_VIEWAXIAL)
            return getDisplayedShapeGraph().hasSelectedElements();
        else if (m_viewClass & DX_VIEWDATA)
            return getDisplayedDataMap().hasSelectedElements();
        else
            return false;
    }
    bool setCurSel(Region4f &r, bool add = false) // set current selection
    {
        if (m_viewClass & DX_VIEWAXIAL)
            return getDisplayedShapeGraph().setCurSel(r, add);
        else if (m_viewClass & DX_VIEWDATA)
            return getDisplayedDataMap().setCurSel(r, add);
        else if (m_viewClass & DX_VIEWVGA)
            return getDisplayedPointMap().setCurSel(r, add);
        else if (m_state & DX_POINTMAPS &&
                 !getDisplayedPointMap()
                      .getInternalMap()
                      .isProcessed()) // this is a default select application
            return getDisplayedPointMap().setCurSel(r, add);
        else if (m_state & DX_DATAMAPS) // I'm not sure why this is a possibility, but it appears
                                        // you might have state & DATAMAPS without VIEWDATA...
            return getDisplayedDataMap().setCurSel(r, add);
        else
            return false;
    }
    bool clearSel() {
        // really needs a separate clearSel for the datalayers... at the moment this is handled
        // in PointMap
        if (m_viewClass & DX_VIEWVGA)
            return getDisplayedPointMap().clearSel();
        else if (m_viewClass & DX_VIEWAXIAL)
            return getDisplayedShapeGraph().clearSel();
        else if (m_viewClass & DX_VIEWDATA)
            return getDisplayedDataMap().clearSel();
        else
            return false;
    }
    size_t getSelCount() {
        if (m_viewClass & DX_VIEWVGA)
            return getDisplayedPointMap().getSelCount();
        else if (m_viewClass & DX_VIEWAXIAL)
            return getDisplayedShapeGraph().getSelCount();
        else if (m_viewClass & DX_VIEWDATA)
            return getDisplayedDataMap().getSelCount();
        else
            return 0;
    }
    float getSelAvg() {
        if (m_viewClass & DX_VIEWVGA)
            return static_cast<float>(getDisplayedPointMap().getDisplayedSelectedAvg());
        else if (m_viewClass & DX_VIEWAXIAL)
            return static_cast<float>(getDisplayedShapeGraph().getDisplayedSelectedAvg());
        else if (m_viewClass & DX_VIEWDATA)
            return static_cast<float>(getDisplayedDataMap().getDisplayedSelectedAvg());
        else
            return -1.0f;
    }
    Region4f getSelBounds() {
        if (m_viewClass & DX_VIEWVGA)
            return getDisplayedPointMap().getSelBounds();
        else if (m_viewClass & DX_VIEWAXIAL)
            return getDisplayedShapeGraph().getSelBounds();
        else if (m_viewClass & DX_VIEWDATA)
            return getDisplayedDataMap().getSelBounds();
        else
            return Region4f();
    }
    // setSelSet expects a set of ref ids:
    void setSelSet(const std::vector<int> &selset, bool add = false) {
        if (m_viewClass & DX_VIEWVGA && m_state & DX_POINTMAPS)
            getDisplayedPointMap().setCurSel(selset, add);
        else if (m_viewClass & DX_VIEWAXIAL)
            getDisplayedShapeGraph().setCurSel(selset, add);
        else // if (m_viewClass & VIEWDATA)
            getDisplayedDataMap().setCurSel(selset, add);
    }
    std::set<int> &getSelSet() {
        if (m_viewClass & DX_VIEWVGA && m_state & DX_POINTMAPS)
            return getDisplayedPointMap().getSelSet();
        else if (m_viewClass & DX_VIEWAXIAL)
            return getDisplayedShapeGraph().getSelSet();
        else // if (m_viewClass & VIEWDATA)
            return getDisplayedDataMap().getSelSet();
    }
    const std::set<int> &getSelSet() const {
        if (m_viewClass & DX_VIEWVGA && m_state & DX_POINTMAPS)
            return getDisplayedPointMap().getSelSet();
        else if (m_viewClass & DX_VIEWAXIAL)
            return getDisplayedShapeGraph().getSelSet();
        else // if (m_viewClass & VIEWDATA)
            return getDisplayedDataMap().getSelSet();
    }

  public:
    void runAgentEngine(Communicator *comm, std::unique_ptr<IAnalysis> &analysis);
    // thru vision
    bool analyseThruVision(Communicator *comm = nullptr,
                           std::optional<size_t> gatelayer = std::nullopt);

  public: // BSP tree for making isovists
    bool makeBSPtree(BSPNodeTree &bspNodeTree, Communicator *communicator = nullptr);
    void resetBSPtree() { m_bspNodeTree.resetBSPtree(); }
    // returns 0: fail, 1: made isovist, 2: made isovist and added new shapemap layer
    int makeIsovist(Communicator *communicator, const Point2f &p, double startangle = 0,
                    double endangle = 0, bool = true, bool closeIsovistPoly = false);
    // returns 0: fail, 1: made isovist, 2: made isovist and added new shapemap layer
    int makeIsovistPath(Communicator *communicator, double fovAngle = 2.0 * M_PI, bool = true);
    bool makeIsovist(const Point2f &p, Isovist &iso);

  protected:
    // properties
  public:
    // likely to use communicator if too slow...
    MetaGraphReadWrite::ReadWriteStatus readFromFile(const std::string &filename);
    MetaGraphReadWrite::ReadWriteStatus readFromStream(std::istream &stream, const std::string &);
    MetaGraphReadWrite::ReadWriteStatus write(const std::string &filename, int version,
                                              bool currentlayer = false,
                                              bool ignoreDisplayData = false);

    std::vector<SimpleLine> getVisibleDrawingLines();

  protected:
    std::streampos skipVirtualMem(std::istream &stream);
};
