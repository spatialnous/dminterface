// SPDX-FileCopyrightText: 2000-2010 University College London, Alasdair Turner
// SPDX-FileCopyrightText: 2011-2012 Tasos Varoudis
// SPDX-FileCopyrightText: 2024 Petros Koutsolampros
//
// SPDX-License-Identifier: GPL-3.0-or-later

// A representation of a sala PointMap in depthmapX

#pragma once

#include "attributemapdx.h"
#include "pointmapdx.h"

#include "salalib/shapemap.h"

class ShapeMapDX : public AttributeMapDX {

    mutable bool m_show; // used when shape map is a drawing layer
    bool m_editable;

    mutable int m_currentShape = -1;
    mutable std::vector<size_t> m_displayShapes;

    mutable bool m_newshape = false; // if a new shape has been added

    mutable int m_curlinkline = -1;
    mutable int m_curunlinkpoint = -1;

    mutable bool m_showLines;
    mutable bool m_showFill;
    mutable bool m_showCentroids;

    std::set<int> m_selectionSet; // note: uses keys

  private:
    void moveData(ShapeMapDX &other) {
        getInternalMap().moveData(other.getInternalMap());
        m_show = other.isShown();
        m_displayedAttribute = other.m_displayedAttribute;
        m_displayShapes = std::move(other.m_displayShapes);
    }

  protected:
    // which attribute is currently displayed:
    mutable int m_displayedAttribute;
    mutable bool m_invalidate;

  public: // ctor
    ShapeMapDX(std::unique_ptr<ShapeMap> &&map)
        : AttributeMapDX(std::move(map)), m_show(true), m_editable(false), m_showLines(true),
          m_showFill(true), m_showCentroids(false), m_displayedAttribute(-1),
          m_invalidate(false){
              // -1 is the shape ref column (which will be shown by default)

              // for polygons:

              // note show is

          };
    ShapeMapDX(const std::string &name, int type)
        : ShapeMapDX(std::make_unique<ShapeMap>(name, type)) {}

    void copy(const ShapeMapDX &other, int copyflags = 0, bool copyMapType = false) {
        getInternalMap().copy(other.getInternalMap(), copyflags, copyMapType);
    }
    ~ShapeMapDX() override {}
    ShapeMapDX() = delete;
    ShapeMapDX(const ShapeMapDX &other) = delete;
    ShapeMapDX(ShapeMapDX &&other) = default;
    ShapeMapDX &operator=(ShapeMapDX &&other) = default;

  public: // methods
    bool valid() const { return !m_invalidate; }

    ShapeMap &getInternalMap() override { return *static_cast<ShapeMap *>(m_map.get()); }
    const ShapeMap &getInternalMap() const override {
        return *static_cast<ShapeMap *>(m_map.get());
    }

    void init(size_t size, const QtRegion &r);

    double getDisplayMinValue() const;

    double getDisplayMaxValue() const;

    const DisplayParams &getDisplayParams() const;
    // make a local copy of the display params for access speed:
    void setDisplayParams(const DisplayParams &dp, bool applyToAll = false);

    void setDisplayedAttribute(int col);
    void setDisplayedAttribute(const std::string &col);
    // use set displayed attribute instead unless you are deliberately changing the column order:
    void overrideDisplayedAttribute(int attribute) { m_displayedAttribute = attribute; }
    // now, there is a slightly odd thing here: the displayed attribute can go out of step with the
    // underlying attribute data if there is a delete of an attribute in idepthmap.h, so it just
    // needs checking before returning!
    int getDisplayedAttribute() const;

    float getDisplayedSelectedAvg() {
        return (getSelectedAvg(static_cast<size_t>(m_displayedAttribute)));
    }
    float getDisplayedAverage();
    void invalidateDisplayedAttribute();

    void clearAll();

    int makePointShapeWithRef(const Point2f &point, int shape_ref, bool tempshape = false,
                              const std::map<int, float> &extraAttributes = std::map<int, float>());
    int makePointShape(const Point2f &point, bool tempshape = false,
                       const std::map<int, float> &extraAttributes = std::map<int, float>());
    int makeLineShapeWithRef(const Line &line, int shape_ref, bool through_ui = false,
                             bool tempshape = false,
                             const std::map<int, float> &extraAttributes = std::map<int, float>());
    int makeLineShape(const Line &line, bool through_ui = false, bool tempshape = false,
                      const std::map<int, float> &extraAttributes = std::map<int, float>());
    int makePolyShapeWithRef(const std::vector<Point2f> &points, bool open, int shape_ref,
                             bool tempshape = false,
                             const std::map<int, float> &extraAttributes = std::map<int, float>());
    int makePolyShape(const std::vector<Point2f> &points, bool open, bool tempshape = false,
                      const std::map<int, float> &extraAttributes = std::map<int, float>());
    int makeShape(const SalaShape &poly, int override_shape_ref = -1,
                  const std::map<int, float> &extraAttributes = std::map<int, float>());
    int makeShapeFromPointSet(const PointMapDX &pointmap);

    bool moveShape(int shaperef, const Line &line, bool undoing = false);

    int polyBegin(const Line &line);
    bool polyAppend(int shape_ref, const Point2f &point);
    bool polyClose(int shape_ref);
    bool polyCancel(int shape_ref);

    bool removeSelected();
    void removeShape(int shaperef, bool undoing);

    void undo();
    void makeShapeConnections();

    double getLocationValue(const Point2f &point) const;
    bool findNextShape(bool &nextlayer) const;
    const SalaShape &getNextShape() const;
    const PafColor getShapeColor() const;
    bool getShapeSelected() const;

    bool linkShapes(const Point2f &p);
    bool unlinkShapes(const Point2f &p);

    bool findNextLinkLine() const;
    Line getNextLinkLine() const;
    // specific to axial line graphs
    bool findNextUnlinkPoint() const;
    Point2f getNextUnlinkPoint() const;

    // To showing
    bool isShown() const { return m_show; }
    void setShow(bool on = true) const { m_show = on; }
    // To all editing
    bool isEditable() const { return m_editable; }
    void setEditable(bool on = true) { m_editable = on; }

    bool getShowLines() const { return m_showLines; }
    bool getShowFill() const { return m_showFill; }
    bool getShowCentroids() const { return m_showCentroids; }

    bool isValid() const { return !m_invalidate; }

    void getPolygonDisplay(bool &show_lines, bool &show_fill, bool &show_centroids);
    void setPolygonDisplay(bool show_lines, bool show_fill, bool show_centroids);

    std::vector<std::pair<SimpleLine, PafColor>> //
    getAllLinesWithColour(const std::set<int> &selSet);
    std::vector<std::pair<std::vector<Point2f>, PafColor>> //
    getAllPolygonsWithColour(const std::set<int> &selSet);
    std::vector<std::pair<Point2f, PafColor>> //
    getAllPointsWithColour(const std::set<int> &selSet);

    std::vector<Point2f> getAllUnlinkPoints();

    void makeViewportShapes(const QtRegion &viewport) const;

    auto getShapeCount() { return getInternalMap().getShapeCount(); }
    auto getSpacing() const { return getInternalMap().getSpacing(); }

  public:
    // Selection
    bool hasSelectedElements() const { return !m_selectionSet.empty(); }
    bool setCurSel(const std::vector<int> &selset, bool add = false);
    bool setCurSel(QtRegion &r, bool add = false);
    float getSelectedAvg(size_t attributeIdx);
    bool clearSel();
    std::set<int> &getSelSet() { return m_selectionSet; }
    const std::set<int> &getSelSet() const { return m_selectionSet; }
    size_t getSelCount() { return m_selectionSet.size(); }
    QtRegion getSelBounds();

    bool selectionToLayer(const std::string &name = std::string("Unnamed"));

    bool read(std::istream &stream);
    bool write(std::ostream &stream);

    // Simple wrappers
    auto &getName() { return getInternalMap().getName(); }
    const auto &getName() const { return getInternalMap().getName(); }
    auto getMapType() { return getInternalMap().getMapType(); }
    auto getAllPointsWithColour() {
        return getInternalMap().getAllPointsWithColour(m_selectionSet);
    }
    auto getAllLinesWithColour() {
        return getInternalMap().getAllSimpleLinesWithColour(m_selectionSet);
    }
    auto getAllPolygonsWithColour() {
        return getInternalMap().getAllPolygonsWithColour(m_selectionSet);
    }
    auto getAllShapes() const { return getInternalMap().getAllShapes(); }
    auto linkShapesFromRefs(int ref1, int ref2) {
        return getInternalMap().linkShapesFromRefs(ref1, ref2);
    };
    auto unlinkShapesFromRefs(int ref1, int ref2) {
        return getInternalMap().unlinkShapesFromRefs(ref1, ref2);
    };
    auto getShapesInRegion(const QtRegion &r) const {
        return getInternalMap().getShapesInRegion(r);
    }
};
