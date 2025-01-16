// SPDX-FileCopyrightText: 2000-2010 University College London, Alasdair Turner
// SPDX-FileCopyrightText: 2011-2012 Tasos Varoudis
// SPDX-FileCopyrightText: 2024 Petros Koutsolampros
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Interface: the meta graph loads and holds all sorts of arbitrary data...
#include "options.h"
#include "pointmapdx.h"
#include "shapegraphdx.h"
#include "shapemapdx.h"
#include "shapemapgroupdatadx.h"

#include "salalib/bspnodetree.h"
#include "salalib/connector.h"
#include "salalib/ianalysis.h"
#include "salalib/importtypedefs.h"
#include "salalib/isovist.h"
#include "salalib/metagraph.h"
#include "salalib/metagraphreadwrite.h"
#include "salalib/pushvalues.h"
#include "salalib/spacepix.h"

#include "salalib/genlib/p2dpoly.h"

#include <memory>
#include <mutex>
#include <optional>
#include <vector>

///////////////////////////////////////////////////////////////////////////////////

class Communicator;

// A meta graph is precisely what it says it is

class MetaGraphDX {
    MetaGraph m_metaGraph;

    MetaGraphReadWrite::ReadStatus m_readStatus = MetaGraphReadWrite::ReadStatus::OK;
    int m_state;
    int m_viewClass;
    bool m_showGrid;
    bool m_showText;

    struct ShapeMapGroup {
        ShapeMapGroupDataDX groupData;
        std::vector<ShapeMapDX> maps;
        ShapeMapGroup(const std::string &groupName) : groupData(groupName) {}
        ShapeMapGroup(ShapeMapGroup &&other)
            : groupData(std::move(other.groupData)), maps(std::move(other.maps)) {}
        ShapeMapGroup &operator=(ShapeMapGroup &&other) = default;
        size_t addMap(std::unique_ptr<ShapeMap> &&shapeMap) {
            maps.push_back(std::move(shapeMap));
            if (maps.size() == 1) {
                groupData.setRegion(maps.back().getRegion());
            } else {
                groupData.setRegion(runion(groupData.getRegion(), maps.back().getRegion()));
            }
            return maps.size() - 1;
        }
    };
    std::vector<ShapeMapGroup> m_drawingFiles;
    std::vector<ShapeMapDX> m_dataMaps;
    std::vector<ShapeGraphDX> m_shapeGraphs;
    std::vector<PointMapDX> m_pointMaps;
    std::optional<size_t> m_displayedDatamap = std::nullopt;
    std::optional<size_t> m_displayedPointmap = std::nullopt;
    std::optional<size_t> m_displayedShapegraph = std::nullopt;

    BSPNodeTree m_bspNodeTree;

  public:
    MetaGraphDX(std::string name = "");
    MetaGraphDX(MetaGraphDX &&other)
        : m_drawingFiles(std::move(other.m_drawingFiles)), m_dataMaps(std::move(other.m_dataMaps)),
          m_shapeGraphs(std::move(other.m_shapeGraphs)), m_pointMaps(std::move(other.m_pointMaps)) {
    }
    MetaGraphDX &operator=(MetaGraphDX &&other) = default;
    ~MetaGraphDX(){};

  public:
    enum {
        SHOWHIDEVGA = 0x0100,
        SHOWVGATOP = 0x0200,
        SHOWHIDEAXIAL = 0x0400,
        SHOWAXIALTOP = 0x0800,
        SHOWHIDESHAPE = 0x1000,
        SHOWSHAPETOP = 0x2000
    };
    enum {
        VIEWNONE = 0x00,
        VIEWVGA = 0x01,
        VIEWBACKVGA = 0x02,
        VIEWAXIAL = 0x04,
        VIEWBACKAXIAL = 0x08,
        VIEWDATA = 0x20,
        VIEWBACKDATA = 0x40,
        VIEWFRONT = 0x25
    };
    enum {
        ADD = 0x0001,
        REPLACE = 0x0002,
        CAT = 0x0010,
        DXF = 0x0020,
        NTF = 0x0040,
        RT1 = 0x0080,
        GML = 0x0100
    };
    enum {
        NONE = 0x0000,
        POINTMAPS = 0x0002,
        LINEDATA = 0x0004,
        ANGULARGRAPH = 0x0010,
        DATAMAPS = 0x0020,
        AXIALLINES = 0x0040,
        SHAPEGRAPHS = 0x0100,
        BUGGY = 0x8000
    };
    enum { NOT_EDITABLE = 0, EDITABLE_OFF = 1, EDITABLE_ON = 2 };

    std::string &getName() { return m_metaGraph.name; }
    const QtRegion &getRegion() { return m_metaGraph.region; }
    void setRegion(Point2f &bottomLeft, Point2f &topRight) {
        m_metaGraph.region.bottomLeft = bottomLeft;
        m_metaGraph.region.topRight = topRight;
    }
    MetaGraphReadWrite::ReadStatus getReadStatus() const { return m_readStatus; };
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
    // ShapeMapGroupDataDX should also have an m_show variable
    bool isShown(const ShapeMapGroup &spf) const {
        for (auto &pixel : spf.maps)
            if (pixel.isShown())
                return true;
        return false;
    }

    // TODO: drawing state functions/fields that should be eventually removed
    void makeViewportShapes(const QtRegion &viewport) const;
    bool findNextShape(const ShapeMapGroup &spf, bool &nextlayer) const;
    bool findNextShape(bool &nextlayer) const;
    const SalaShape &getNextShape() const {
        auto &currentDrawingFile = m_drawingFiles[static_cast<size_t>(currentLayer)];
        return currentDrawingFile
            .maps[static_cast<size_t>(currentDrawingFile.groupData.getCurrentLayer())]
            .getNextShape();
    }
    mutable int currentLayer;

  public:
    int getVersion() {
        // note, if unsaved, m_file_version is -1
        return m_metaGraph.version;
    }

    std::vector<PointMapDX> &getPointMaps() { return m_pointMaps; }
    bool hasDisplayedPointMap() const { return m_displayedPointmap.has_value(); }
    PointMapDX &getDisplayedPointMap() { return m_pointMaps[m_displayedPointmap.value()]; }
    const PointMapDX &getDisplayedPointMap() const {
        return m_pointMaps[m_displayedPointmap.value()];
    }
    void setDisplayedPointMapRef(size_t map) {
        if (m_displayedPointmap.has_value() && m_displayedPointmap != map)
            getDisplayedPointMap().clearSel();
        m_displayedPointmap = map;
    }
    size_t getDisplayedPointMapRef() const { return m_displayedPointmap.value(); }
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
                        depthmapX::ImportFileType importFileType, bool replace);
    size_t addDrawingFile(std::string name, std::vector<ShapeMap> &&maps);
    ShapeMapDX &createNewShapeMap(depthmapX::ImportType mapType, std::string name);
    void deleteShapeMap(depthmapX::ImportType mapType, ShapeMapDX &shapeMap);
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
    std::vector<std::pair<std::reference_wrapper<const ShapeMapDX>, int>> getShownDrawingMaps();
    std::vector<std::pair<std::reference_wrapper<const ShapeMap>, int>>
    getAsInternalMaps(std::vector<std::pair<std::reference_wrapper<const ShapeMapDX>, int>> maps);
    std::vector<Line> getShownDrawingFilesAsLines();
    std::vector<SalaShape> getShownDrawingFilesAsShapes();
    bool makeGraph(Communicator *communicator, int algorithm, double maxdist);
    bool unmakeGraph(bool removeLinks);
    bool analyseGraph(Communicator *communicator, Options options,
                      bool simpleVersion); // <- options copied to keep thread safe
    //
    // helpers for editing maps
    bool isEditableMap();
    ShapeMapDX &getEditableMap();
    // currently only making / moving lines, but should be able to extend this to polys fairly
    // easily:
    bool makeShape(const Line &line);
    bool moveSelShape(const Line &line);
    // onto polys as well:
    int polyBegin(const Line &line);
    bool polyAppend(int shapeRef, const Point2f &point);
    bool polyClose(int shapeRef);
    bool polyCancel(int shapeRef);
    //
    size_t addShapeGraph(ShapeGraphDX &&shapeGraph);
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
    bool analyseAxial(Communicator *communicator, Options options,
                      bool forceLegacyColumnOrder = false); // <- options copied to keep thread safe
    bool analyseSegmentsTulip(
        Communicator *communicator, Options options,
        bool forceLegacyColumnOrder = false); // <- options copied to keep thread safe
    bool analyseSegmentsAngular(Communicator *communicator,
                                Options options); // <- options copied to keep thread safe
    bool analyseTopoMetMultipleRadii(Communicator *communicator,
                                     Options options); // <- options copied to keep thread safe
    bool analyseTopoMet(Communicator *communicator,
                        Options options); // <- options copied to keep thread safe
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
    ShapeMapDX &getDisplayedDataMap() { return m_dataMaps[m_displayedDatamap.value()]; }
    const ShapeMapDX &getDisplayedDataMap() const { return m_dataMaps[m_displayedDatamap.value()]; }
    size_t getDisplayedDataMapRef() const { return m_displayedDatamap.value(); }

    void removeDataMap(size_t i) {
        if (m_displayedDatamap.has_value()) {
            if (m_dataMaps.size() == 1)
                m_displayedDatamap = std::nullopt;
            else if (m_displayedDatamap.value() != 0 && m_displayedDatamap.value() >= i)
                m_displayedDatamap.value()--;
        }
        m_dataMaps.erase(std::next(m_dataMaps.begin(), static_cast<int>(i)));
    }

    void setDisplayedDataMapRef(size_t map) {
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

    std::vector<ShapeGraphDX> &getShapeGraphs() { return m_shapeGraphs; }
    bool hasDisplayedShapeGraph() const { return m_displayedShapegraph.has_value(); }
    ShapeGraphDX &getDisplayedShapeGraph() { return m_shapeGraphs[m_displayedShapegraph.value()]; }
    const ShapeGraphDX &getDisplayedShapeGraph() const {
        return m_shapeGraphs[m_displayedShapegraph.value()];
    }
    void unsetDisplayedShapeGraphRef() { m_displayedShapegraph = std::nullopt; }
    void setDisplayedShapeGraphRef(size_t map) {
        if (m_displayedShapegraph.has_value() && m_displayedShapegraph != map)
            getDisplayedShapeGraph().clearSel();
        m_displayedShapegraph = map;
    }
    size_t getDisplayedShapeGraphRef() const { return m_displayedShapegraph.value(); }

    void removeShapeGraph(size_t i) {
        if (m_displayedShapegraph.has_value()) {
            if (m_shapeGraphs.size() == 1)
                m_displayedShapegraph = std::nullopt;
            else if (m_displayedShapegraph.value() > 0 && m_displayedShapegraph.value() >= i)
                m_displayedShapegraph.value()--;
        }
        m_shapeGraphs.erase(std::next(m_shapeGraphs.begin(), static_cast<int>(i)));
    }

    std::vector<ShapeMapDX> &getDataMaps() { return m_dataMaps; }

    //
    int getDisplayedMapType();
    AttributeTable &getDisplayedMapAttributes();
    bool hasVisibleDrawingLayers();
    QtRegion getBoundingBox() const;
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

    int getLineFileCount() const { return (int)m_drawingFiles.size(); }

    const std::string &getLineFileName(size_t fileIdx) const {
        return m_drawingFiles[fileIdx].groupData.getName();
    }
    size_t getLineLayerCount(size_t fileIdx) const { return m_drawingFiles[fileIdx].maps.size(); }

    ShapeMapDX &getLineLayer(size_t fileIdx, size_t layerIdx) {
        return m_drawingFiles[fileIdx].maps[layerIdx];
    }
    const ShapeMapDX &getLineLayer(size_t fileIdx, size_t layerIdx) const {
        return m_drawingFiles[fileIdx].maps[layerIdx];
    }
    int getViewClass() { return m_viewClass; }
    // These functions make specifying conditions to do things much easier:
    bool viewingNone() { return (m_viewClass == VIEWNONE); }
    bool viewingProcessed() {
        return ((m_viewClass & (VIEWAXIAL | VIEWDATA)) ||
                (m_viewClass & VIEWVGA && getDisplayedPointMap().getInternalMap().isProcessed()));
    }
    bool viewingShapes() { return (m_viewClass & (VIEWAXIAL | VIEWDATA)) != 0; }
    bool viewingProcessedLines() { return ((m_viewClass & VIEWAXIAL) == VIEWAXIAL); }
    bool viewingProcessedShapes() { return ((m_viewClass & VIEWDATA) == VIEWDATA); }
    bool viewingProcessedPoints() {
        return ((m_viewClass & VIEWVGA) && getDisplayedPointMap().getInternalMap().isProcessed());
    }
    bool viewingUnprocessedPoints() {
        return ((m_viewClass & VIEWVGA) && !getDisplayedPointMap().getInternalMap().isProcessed());
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
        if (m_viewClass & VIEWVGA)
            return getDisplayedPointMap().isSelected();
        else if (m_viewClass & VIEWAXIAL)
            return getDisplayedShapeGraph().hasSelectedElements();
        else if (m_viewClass & VIEWDATA)
            return getDisplayedDataMap().hasSelectedElements();
        else
            return false;
    }
    bool setCurSel(QtRegion &r, bool add = false) // set current selection
    {
        if (m_viewClass & VIEWAXIAL)
            return getDisplayedShapeGraph().setCurSel(r, add);
        else if (m_viewClass & VIEWDATA)
            return getDisplayedDataMap().setCurSel(r, add);
        else if (m_viewClass & VIEWVGA)
            return getDisplayedPointMap().setCurSel(r, add);
        else if (m_state & POINTMAPS && !getDisplayedPointMap()
                                             .getInternalMap()
                                             .isProcessed()) // this is a default select application
            return getDisplayedPointMap().setCurSel(r, add);
        else if (m_state & DATAMAPS) // I'm not sure why this is a possibility, but it appears
                                     // you might have state & DATAMAPS without VIEWDATA...
            return getDisplayedDataMap().setCurSel(r, add);
        else
            return false;
    }
    bool clearSel() {
        // really needs a separate clearSel for the datalayers... at the moment this is handled
        // in PointMap
        if (m_viewClass & VIEWVGA)
            return getDisplayedPointMap().clearSel();
        else if (m_viewClass & VIEWAXIAL)
            return getDisplayedShapeGraph().clearSel();
        else if (m_viewClass & VIEWDATA)
            return getDisplayedDataMap().clearSel();
        else
            return false;
    }
    int getSelCount() {
        if (m_viewClass & VIEWVGA)
            return getDisplayedPointMap().getSelCount();
        else if (m_viewClass & VIEWAXIAL)
            return (int)getDisplayedShapeGraph().getSelCount();
        else if (m_viewClass & VIEWDATA)
            return (int)getDisplayedDataMap().getSelCount();
        else
            return 0;
    }
    float getSelAvg() {
        if (m_viewClass & VIEWVGA)
            return (float)getDisplayedPointMap().getDisplayedSelectedAvg();
        else if (m_viewClass & VIEWAXIAL)
            return (float)getDisplayedShapeGraph().getDisplayedSelectedAvg();
        else if (m_viewClass & VIEWDATA)
            return (float)getDisplayedDataMap().getDisplayedSelectedAvg();
        else
            return -1.0f;
    }
    QtRegion getSelBounds() {
        if (m_viewClass & VIEWVGA)
            return getDisplayedPointMap().getSelBounds();
        else if (m_viewClass & VIEWAXIAL)
            return getDisplayedShapeGraph().getSelBounds();
        else if (m_viewClass & VIEWDATA)
            return getDisplayedDataMap().getSelBounds();
        else
            return QtRegion();
    }
    // setSelSet expects a set of ref ids:
    void setSelSet(const std::vector<int> &selset, bool add = false) {
        if (m_viewClass & VIEWVGA && m_state & POINTMAPS)
            getDisplayedPointMap().setCurSel(selset, add);
        else if (m_viewClass & VIEWAXIAL)
            getDisplayedShapeGraph().setCurSel(selset, add);
        else // if (m_viewClass & VIEWDATA)
            getDisplayedDataMap().setCurSel(selset, add);
    }
    std::set<int> &getSelSet() {
        if (m_viewClass & VIEWVGA && m_state & POINTMAPS)
            return getDisplayedPointMap().getSelSet();
        else if (m_viewClass & VIEWAXIAL)
            return getDisplayedShapeGraph().getSelSet();
        else // if (m_viewClass & VIEWDATA)
            return getDisplayedDataMap().getSelSet();
    }
    const std::set<int> &getSelSet() const {
        if (m_viewClass & VIEWVGA && m_state & POINTMAPS)
            return getDisplayedPointMap().getSelSet();
        else if (m_viewClass & VIEWAXIAL)
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
    MetaGraphReadWrite::ReadStatus readFromFile(const std::string &filename);
    MetaGraphReadWrite::ReadStatus readFromStream(std::istream &stream, const std::string &);
    MetaGraphReadWrite::ReadStatus write(const std::string &filename, int version,
                                         bool currentlayer = false, bool ignoreDisplayData = false);

    std::vector<SimpleLine> getVisibleDrawingLines();

  protected:
    std::streampos skipVirtualMem(std::istream &stream);
};
