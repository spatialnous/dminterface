// SPDX-FileCopyrightText: 2000-2010 University College London, Alasdair Turner
// SPDX-FileCopyrightText: 2011-2012 Tasos Varoudis
// SPDX-FileCopyrightText: 2024 Petros Koutsolampros
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "shapemapdx.h"
#include "salalib/tolerances.h"
#include <numeric>

void ShapeMapDX::init(size_t size, const QtRegion &r) {
    m_displayShapes.clear();
    getInternalMap().init(size, r);
}

double ShapeMapDX::getDisplayMinValue() const {
    return (m_displayedAttribute != -1)
               ? getInternalMap().getDisplayMinValue(static_cast<size_t>(m_displayedAttribute))
               : 0;
}

double ShapeMapDX::getDisplayMaxValue() const {
    return (m_displayedAttribute != -1)
               ? getInternalMap().getDisplayMaxValue(static_cast<size_t>(m_displayedAttribute))
               : getInternalMap().getDefaultMaxValue();
}

const DisplayParams &ShapeMapDX::getDisplayParams() const {
    return getInternalMap().getDisplayParams(static_cast<size_t>(m_displayedAttribute));
}

void ShapeMapDX::setDisplayParams(const DisplayParams &dp, bool applyToAll) {
    getInternalMap().setDisplayParams(dp, static_cast<size_t>(m_displayedAttribute), applyToAll);
}

void ShapeMapDX::setDisplayedAttribute(int col) {
    if (!m_invalidate && m_displayedAttribute == col) {
        return;
    }
    m_displayedAttribute = col;
    m_invalidate = true;

    // always override at this stage:
    getInternalMap().getAttributeTableHandle().setDisplayColIndex(m_displayedAttribute);

    m_invalidate = false;
}

void ShapeMapDX::setDisplayedAttribute(const std::string &col) {
    setDisplayedAttribute(
        static_cast<int>(getInternalMap().getAttributeTable().getColumnIndex(col)));
}

int ShapeMapDX::getDisplayedAttribute() const {
    if (m_displayedAttribute == getInternalMap().getAttributeTableHandle().getDisplayColIndex())
        return m_displayedAttribute;
    if (getInternalMap().getAttributeTableHandle().getDisplayColIndex() != -2) {
        m_displayedAttribute = getInternalMap().getAttributeTableHandle().getDisplayColIndex();
    }
    return m_displayedAttribute;
}

float ShapeMapDX::getDisplayedAverage() {
    return (static_cast<float>(
        getInternalMap().getDisplayedAverage(static_cast<size_t>(m_displayedAttribute))));
}

void ShapeMapDX::invalidateDisplayedAttribute() { m_invalidate = true; }

void ShapeMapDX::clearAll() {
    m_displayShapes.clear();
    getInternalMap().clearAll();
    m_displayedAttribute = -1;
}

int ShapeMapDX::makePointShape(const Point2f &point, bool tempshape,
                               const std::map<int, float> &extraAttributes) {
    return makePointShapeWithRef(point, getInternalMap().getNextShapeKey(), tempshape,
                                 extraAttributes);
}

bool ShapeMapDX::read(std::istream &stream) {

    m_displayShapes.clear();

    bool read = false;
    std::tie(read, m_editable, m_show, m_displayedAttribute) = getInternalMap().read(stream);

    invalidateDisplayedAttribute();
    setDisplayedAttribute(m_displayedAttribute);

    return true;
}

bool ShapeMapDX::write(std::ostream &stream) {
    bool written = getInternalMap().writeNameType(stream);

    stream.write(reinterpret_cast<const char *>(&m_show), sizeof(m_show));
    stream.write(reinterpret_cast<const char *>(&m_editable), sizeof(m_editable));

    written = written && getInternalMap().writePart2(stream);

    // TODO: Compatibility. The attribute columns will be stored sorted
    // alphabetically so the displayed attribute needs to match that
    auto sortedDisplayedAttribute = getInternalMap().getAttributeTable().getColumnSortedIndex(
        static_cast<size_t>(m_displayedAttribute));
    stream.write(reinterpret_cast<const char *>(&sortedDisplayedAttribute),
                 sizeof(sortedDisplayedAttribute));
    written = written && getInternalMap().writePart3(stream);
    return written;
}

bool ShapeMapDX::findNextShape(bool &nextlayer) const {
    // note: will not work immediately after a new poly has been added:
    // makeViewportShapes first
    if (m_newshape) {
        return false;
    }

    // TODO: Remove static_cast<size_t>(-1)
    while ((++m_currentShape < (int)getInternalMap().getAllShapes().size()) &&
           m_displayShapes[static_cast<size_t>(m_currentShape)] == static_cast<size_t>(-1))
        ;

    if (m_currentShape < (int)getInternalMap().getAllShapes().size()) {
        return true;
    } else {
        m_currentShape = (int)getInternalMap().getAllShapes().size();
        nextlayer = true;
        return false;
    }
}

const SalaShape &ShapeMapDX::getNextShape() const {
    auto x = m_displayShapes[static_cast<size_t>(m_currentShape)]; // x has display order in it
    m_displayShapes[static_cast<size_t>(m_currentShape)] =
        static_cast<size_t>(-1); // you've drawn it
    return depthmapX::getMapAtIndex(getInternalMap().getAllShapes(), x)->second;
}

// this is all very similar to spacepixel, apart from a few minor details

void ShapeMapDX::makeViewportShapes(const QtRegion &viewport) const {

    auto &shapes = getInternalMap().getAllShapes();
    if (m_displayShapes.empty() || m_newshape) {
        m_displayShapes.assign(shapes.size(), static_cast<size_t>(-1));
        m_newshape = false;
    }

    m_currentShape = -1; // note: findNext expects first to be labelled -1

    m_displayShapes = getInternalMap().makeViewportShapes(viewport);

    m_curlinkline = -1;
    m_curunlinkpoint = -1;
}

int ShapeMapDX::makePointShapeWithRef(const Point2f &point, int shapeRef, bool tempshape,
                                      const std::map<int, float> &extraAttributes) {
    int newShapeRef =
        getInternalMap().makePointShapeWithRef(point, shapeRef, tempshape, extraAttributes);
    if (!tempshape) {
        m_newshape = true;
    }
    return newShapeRef;
}

int ShapeMapDX::makeLineShape(const Line &line, bool throughUi, bool tempshape,
                              const std::map<int, float> &extraAttributes) {
    return makeLineShapeWithRef(line, getInternalMap().getNextShapeKey(), throughUi, tempshape,
                                extraAttributes);
}

int ShapeMapDX::makeLineShapeWithRef(const Line &line, int shapeRef, bool throughUi, bool tempshape,
                                     const std::map<int, float> &extraAttributes) {
    // note, map must have editable flag on if we are to make a shape through the
    // user interface:
    if (throughUi && !m_editable) {
        return -1;
    }
    int newShapeRef = getInternalMap().makeLineShapeWithRef(line, shapeRef, throughUi, tempshape,
                                                            extraAttributes);

    if (!tempshape) {
        m_newshape = true;
    }

    if (throughUi) {
        // update displayed attribute if through ui:
        invalidateDisplayedAttribute();
        setDisplayedAttribute(m_displayedAttribute);
    }
    return newShapeRef;
}

int ShapeMapDX::makePolyShape(const std::vector<Point2f> &points, bool open, bool tempshape,
                              const std::map<int, float> &extraAttributes) {
    return makePolyShapeWithRef(points, getInternalMap().getNextShapeKey(), open, tempshape,
                                extraAttributes);
}

int ShapeMapDX::makePolyShapeWithRef(const std::vector<Point2f> &points, bool open, int shapeRef,
                                     bool tempshape, const std::map<int, float> &extraAttributes) {
    int newShapeRef =
        getInternalMap().makePolyShapeWithRef(points, open, shapeRef, tempshape, extraAttributes);
    if (!tempshape) {
        m_newshape = true;
    }
    return newShapeRef;
}

int ShapeMapDX::makeShape(const SalaShape &poly, int overrideShapeRef,
                          const std::map<int, float> &extraAttributes) {
    int shapeRef = getInternalMap().makeShape(poly, overrideShapeRef, extraAttributes);
    m_newshape = true;
    return shapeRef;
}

// n.b., only works from current selection (and uses point selected attribute)

int ShapeMapDX::makeShapeFromPointSet(const PointMapDX &pointmap) {
    int shapeRef =
        getInternalMap().makeShapeFromPointSet(pointmap.getInternalMap(), pointmap.getSelSet());
    m_newshape = true;
    return shapeRef;
}

bool ShapeMapDX::moveShape(int shaperef, const Line &line, bool undoing) {
    bool moved = getInternalMap().moveShape(shaperef, line, undoing);

    if (getInternalMap().hasGraph()) {
        // update displayed attribute for any changes:
        invalidateDisplayedAttribute();
        setDisplayedAttribute(m_displayedAttribute);
    }
    return moved;
}

int ShapeMapDX::polyBegin(const Line &line) {
    auto newShapeRef = getInternalMap().polyBegin(line);

    // update displayed attribute
    invalidateDisplayedAttribute();
    setDisplayedAttribute(m_displayedAttribute);

    // flag new shape
    m_newshape = true;

    return newShapeRef;
}

bool ShapeMapDX::polyAppend(int shapeRef, const Point2f &point) {
    return getInternalMap().polyAppend(shapeRef, point);
}
bool ShapeMapDX::polyClose(int shapeRef) { return getInternalMap().polyClose(shapeRef); }

bool ShapeMapDX::polyCancel(int shapeRef) {
    bool polyCancelled = getInternalMap().polyCancel(shapeRef);

    // update displayed attribute
    invalidateDisplayedAttribute();
    setDisplayedAttribute(m_displayedAttribute);
    return polyCancelled;
}

bool ShapeMapDX::removeSelected() {
    // note, map must have editable flag on if we are to remove shape:
    if (!m_editable) {
        return false;
        // m_selectionSet selection set is in order!
        // (it should be: code currently uses add() throughout)
        for (auto &shapeRef : m_selectionSet) {
            removeShape(shapeRef, false);
        }
        m_selectionSet.clear();

        invalidateDisplayedAttribute();
        setDisplayedAttribute(m_displayedAttribute);

        return true;
    }
    return false;
}

void ShapeMapDX::removeShape(int shaperef, bool undoing) {
    getInternalMap().removeShape(shaperef, undoing);

    m_invalidate = true;
    m_newshape = true;
}

void ShapeMapDX::undo() {
    getInternalMap().undo();
    invalidateDisplayedAttribute();
    setDisplayedAttribute(m_displayedAttribute);
    m_newshape = true;
}

void ShapeMapDX::makeShapeConnections() {
    getInternalMap().makeShapeConnections();

    m_displayedAttribute = -1; // <- override if it's already showing
    auto connCol = getInternalMap().getAttributeTable().getColumnIndex("Connectivity");

    setDisplayedAttribute(static_cast<int>(connCol));
}

double ShapeMapDX::getLocationValue(const Point2f &point) const {
    if (m_displayedAttribute == -1) {
        return getInternalMap().getLocationValue(point, std::nullopt);
    }
    return getInternalMap().getLocationValue(point, m_displayedAttribute);
}

const PafColor ShapeMapDX::getShapeColor() const {
    AttributeKey key(static_cast<int>(m_displayShapes[static_cast<size_t>(m_currentShape)]));
    const AttributeRow &row = getInternalMap().getAttributeTable().getRow(key);
    return dXreimpl::getDisplayColor(key, row, getInternalMap().getAttributeTableHandle(),
                                     m_selectionSet, true);
    ;
}

bool ShapeMapDX::getShapeSelected() const {
    return m_selectionSet.find(static_cast<int>(
               m_displayShapes[static_cast<size_t>(m_currentShape)])) != m_selectionSet.end();
}

bool ShapeMapDX::linkShapes(const Point2f &p) {
    if (m_selectionSet.size() != 1) {
        return false;
    }
    return getInternalMap().linkShapes(p, *m_selectionSet.begin());
}

bool ShapeMapDX::unlinkShapes(const Point2f &p) {
    if (m_selectionSet.size() != 1) {
        return false;
    }
    clearSel();
    return getInternalMap().unlinkShapes(p, *m_selectionSet.begin());
}

bool ShapeMapDX::findNextLinkLine() const {
    if (m_curlinkline < (int)getInternalMap().getLinks().size()) {
        m_curlinkline++;
    }
    return (m_curlinkline < (int)getInternalMap().getLinks().size());
}

Line ShapeMapDX::getNextLinkLine() const {
    // note, links are stored directly by rowid, not by key:
    if (m_curlinkline < (int)getInternalMap().getLinks().size()) {
        return Line(depthmapX::getMapAtIndex(
                        getInternalMap().getAllShapes(),
                        getInternalMap().getLinks()[static_cast<size_t>(m_curlinkline)].a)
                        ->second.getCentroid(),
                    depthmapX::getMapAtIndex(
                        getInternalMap().getAllShapes(),
                        getInternalMap().getLinks()[static_cast<size_t>(m_curlinkline)].b)
                        ->second.getCentroid());
    }
    return Line();
}
bool ShapeMapDX::findNextUnlinkPoint() const {
    if (m_curunlinkpoint < (int)getInternalMap().getUnlinks().size()) {
        m_curunlinkpoint++;
    }
    return (m_curunlinkpoint < (int)getInternalMap().getUnlinks().size());
}

Point2f ShapeMapDX::getNextUnlinkPoint() const {
    // note, links are stored directly by rowid, not by key:
    if (m_curunlinkpoint < (int)getInternalMap().getUnlinks().size()) {
        return intersection_point(
            depthmapX::getMapAtIndex(
                getInternalMap().getAllShapes(),
                getInternalMap().getUnlinks()[static_cast<size_t>(m_curunlinkpoint)].a)
                ->second.getLine(),
            depthmapX::getMapAtIndex(
                getInternalMap().getAllShapes(),
                getInternalMap().getUnlinks()[static_cast<size_t>(m_curunlinkpoint)].b)
                ->second.getLine(),
            TOLERANCE_A);
    }
    return Point2f();
}

void ShapeMapDX::getPolygonDisplay(bool &showLines, bool &showFill, bool &showCentroids) {
    showLines = m_showLines;
    showFill = m_showFill;
    showCentroids = m_showCentroids;
}

void ShapeMapDX::setPolygonDisplay(bool showLines, bool showFill, bool showCentroids) {
    m_showLines = showLines;
    m_showFill = showFill;
    m_showCentroids = showCentroids;
}

std::vector<std::pair<SimpleLine, PafColor>>
ShapeMapDX::getAllLinesWithColour(const std::set<int> &selSet) {
    return getInternalMap().getAllSimpleLinesWithColour(selSet);
}

std::vector<std::pair<std::vector<Point2f>, PafColor>>
ShapeMapDX::getAllPolygonsWithColour(const std::set<int> &selSet) {
    return getInternalMap().getAllPolygonsWithColour(selSet);
}

std::vector<std::pair<Point2f, PafColor>>
ShapeMapDX::getAllPointsWithColour(const std::set<int> &selSet) {
    return getInternalMap().getAllPointsWithColour(selSet);
}

std::vector<Point2f> ShapeMapDX::getAllUnlinkPoints() {
    return getInternalMap().getAllUnlinkPoints();
}

bool ShapeMapDX::setCurSel(const std::vector<int> &selset, bool add) {
    if (add == false) {
        clearSel();
    }

    for (auto shape : selset) {
        m_selectionSet.insert(shape);
    }

    return !m_selectionSet.empty();
}

bool ShapeMapDX::setCurSel(QtRegion &r, bool add) {
    if (add == false) {
        clearSel();
    }

    std::map<int, SalaShape> shapesInRegion = getInternalMap().getShapesInRegion(r);
    for (auto &shape : shapesInRegion) {
        m_selectionSet.insert(shape.first);
    }

    return !shapesInRegion.empty();
}

float ShapeMapDX::getSelectedAvg(size_t attributeIdx) {
    return getInternalMap().getAttributeTable().getSelAvg(attributeIdx, m_selectionSet);
}

bool ShapeMapDX::clearSel() {
    // note, only clear if need be, as m_attributes->deselectAll is slow
    if (!m_selectionSet.empty()) {
        m_selectionSet.clear();
    }
    return true;
}

QtRegion ShapeMapDX::getSelBounds() {
    QtRegion r;
    if (!m_selectionSet.empty()) {
        for (auto &shapeRef : m_selectionSet) {
            r = runion(r, getInternalMap().getAllShapes().at(shapeRef).getBoundingBox());
        }
    }
    return r;
}

bool ShapeMapDX::selectionToLayer(const std::string &name) {
    bool retvar = false;
    if (m_selectionSet.size()) {
        dXreimpl::pushSelectionToLayer(getInternalMap().getAttributeTable(),
                                       getInternalMap().getLayers(), name, m_selectionSet);
        retvar = getInternalMap().getLayers().isLayerVisible(
            getInternalMap().getLayers().getLayerIndex(name));
        if (retvar) {
            clearSel();
        }
    }
    return retvar;
}
