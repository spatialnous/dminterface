// SPDX-FileCopyrightText: 2000-2010 University College London, Alasdair Turner
// SPDX-FileCopyrightText: 2011-2012 Tasos Varoudis
// SPDX-FileCopyrightText: 2024 Petros Koutsolampros
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "shapemapdm.hpp"
#include "salalib/tolerances.hpp"

void ShapeMapDM::init(size_t size, const Region4f &r) {
    m_displayShapes.clear();
    getInternalMap().init(size, r);
}

double ShapeMapDM::getDisplayMinValue() const {
    return (m_displayedAttribute != -1)
               ? getInternalMap().getDisplayMinValue(static_cast<size_t>(m_displayedAttribute))
               : 0;
}

double ShapeMapDM::getDisplayMaxValue() const {
    return (m_displayedAttribute != -1)
               ? getInternalMap().getDisplayMaxValue(static_cast<size_t>(m_displayedAttribute))
               : getInternalMap().getDefaultMaxValue();
}

const DisplayParams &ShapeMapDM::getDisplayParams() const {
    return getInternalMap().getDisplayParams(static_cast<size_t>(m_displayedAttribute));
}

void ShapeMapDM::setDisplayParams(const DisplayParams &dp, bool applyToAll) {
    getInternalMap().setDisplayParams(dp, static_cast<size_t>(m_displayedAttribute), applyToAll);
}

void ShapeMapDM::setDisplayedAttribute(int col) {
    if (!m_invalidate && m_displayedAttribute == col) {
        return;
    }
    m_displayedAttribute = col;
    m_invalidate = true;

    // always override at this stage:
    getInternalMap().getAttributeTableHandle().setDisplayColIndex(m_displayedAttribute);

    m_invalidate = false;
}

void ShapeMapDM::setDisplayedAttribute(const std::string &col) {
    setDisplayedAttribute(
        static_cast<int>(getInternalMap().getAttributeTable().getColumnIndex(col)));
}

int ShapeMapDM::getDisplayedAttribute() const {
    if (m_displayedAttribute == getInternalMap().getAttributeTableHandle().getDisplayColIndex())
        return m_displayedAttribute;
    if (getInternalMap().getAttributeTableHandle().getDisplayColIndex() != -2) {
        m_displayedAttribute = getInternalMap().getAttributeTableHandle().getDisplayColIndex();
    }
    return m_displayedAttribute;
}

float ShapeMapDM::getDisplayedAverage() {
    return (static_cast<float>(
        getInternalMap().getDisplayedAverage(static_cast<size_t>(m_displayedAttribute))));
}

void ShapeMapDM::invalidateDisplayedAttribute() { m_invalidate = true; }

void ShapeMapDM::clearAll() {
    m_displayShapes.clear();
    m_undobuffer.clear();
    getInternalMap().clearAll();
    m_displayedAttribute = -1;
}

int ShapeMapDM::makePointShape(const Point2f &point, bool tempshape,
                               const std::map<size_t, float> &extraAttributes) {
    return makePointShapeWithRef(point, getInternalMap().getNextShapeKey(), tempshape,
                                 extraAttributes);
}

bool ShapeMapDM::read(std::istream &stream) {

    m_displayShapes.clear();

    bool read = false;
    std::tie(read, m_editable, m_show, m_displayedAttribute) = getInternalMap().read(stream);

    m_undobuffer.clear();

    invalidateDisplayedAttribute();
    setDisplayedAttribute(m_displayedAttribute);

    return true;
}

bool ShapeMapDM::write(std::ostream &stream) {
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

bool ShapeMapDM::findNextShape(bool &nextlayer) const {
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

const SalaShape &ShapeMapDM::getNextShape() const {
    auto x = m_displayShapes[static_cast<size_t>(m_currentShape)]; // x has display order in it
    m_displayShapes[static_cast<size_t>(m_currentShape)] =
        static_cast<size_t>(-1); // you've drawn it
    return genlib::getMapAtIndex(getInternalMap().getAllShapes(), x)->second;
}

// this is all very similar to spacepixel, apart from a few minor details

void ShapeMapDM::makeViewportShapes(const Region4f &viewport) const {

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

int ShapeMapDM::makePointShapeWithRef(const Point2f &point, int shapeRef, bool tempshape,
                                      const std::map<size_t, float> &extraAttributes) {
    int newShapeRef =
        getInternalMap().makePointShapeWithRef(point, shapeRef, tempshape, extraAttributes);
    if (!tempshape) {
        m_newshape = true;
    }
    return newShapeRef;
}

int ShapeMapDM::makeLineShape(const Line4f &line, bool throughUi, bool tempshape,
                              const std::map<size_t, float> &extraAttributes) {
    return makeLineShapeWithRef(line, getInternalMap().getNextShapeKey(), throughUi, tempshape,
                                extraAttributes);
}

int ShapeMapDM::makeLineShapeWithRef(const Line4f &line, int shapeRef, bool throughUi,
                                     bool tempshape,
                                     const std::map<size_t, float> &extraAttributes) {
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
        // if through ui, set undo counter:
        m_undobuffer.push_back(SalaEvent(SalaEvent::SALA_CREATED, shapeRef));
    }
    return newShapeRef;
}

int ShapeMapDM::makePolyShape(const std::vector<Point2f> &points, bool open, bool tempshape,
                              const std::map<size_t, float> &extraAttributes) {
    return makePolyShapeWithRef(points, getInternalMap().getNextShapeKey(), open, tempshape,
                                extraAttributes);
}

int ShapeMapDM::makePolyShapeWithRef(const std::vector<Point2f> &points, bool open, int shapeRef,
                                     bool tempshape,
                                     const std::map<size_t, float> &extraAttributes) {
    int newShapeRef =
        getInternalMap().makePolyShapeWithRef(points, open, shapeRef, tempshape, extraAttributes);
    if (!tempshape) {
        m_newshape = true;
    }
    return newShapeRef;
}

int ShapeMapDM::makeShape(const SalaShape &poly, int overrideShapeRef,
                          const std::map<size_t, float> &extraAttributes) {
    int shapeRef = getInternalMap().makeShape(poly, overrideShapeRef, extraAttributes);
    m_newshape = true;
    return shapeRef;
}

// n.b., only works from current selection (and uses point selected attribute)

int ShapeMapDM::makeShapeFromPointSet(const PointMapDM &pointmap) {
    int shapeRef =
        getInternalMap().makeShapeFromPointSet(pointmap.getInternalMap(), pointmap.getSelSet());
    m_newshape = true;
    return shapeRef;
}

bool ShapeMapDM::moveShape(int shaperef, const Line4f &line, bool undoing) {

    if (!undoing) {
        auto shapeIter = getInternalMap().getAllShapes().find(shaperef);
        if (shapeIter == getInternalMap().getAllShapes().end()) {
            return false;
        }
        // set undo counter, but only if this is not an undo itself:
        m_undobuffer.push_back(SalaEvent(SalaEvent::SALA_MOVED, shaperef));
        m_undobuffer.back().geometry = shapeIter->second;
    }

    bool moved = getInternalMap().moveShape(shaperef, line);

    if (getInternalMap().hasGraph()) {
        // update displayed attribute for any changes:
        invalidateDisplayedAttribute();
        setDisplayedAttribute(m_displayedAttribute);
    }
    return moved;
}

int ShapeMapDM::polyBegin(const Line4f &line) {
    auto newShapeRef = getInternalMap().polyBegin(line);

    // update displayed attribute
    invalidateDisplayedAttribute();
    setDisplayedAttribute(m_displayedAttribute);

    // set undo counter:
    m_undobuffer.push_back(SalaEvent(SalaEvent::SALA_CREATED, newShapeRef));

    // flag new shape
    m_newshape = true;

    return newShapeRef;
}

bool ShapeMapDM::polyAppend(int shapeRef, const Point2f &point) {
    return getInternalMap().polyAppend(shapeRef, point);
}
bool ShapeMapDM::polyClose(int shapeRef) { return getInternalMap().polyClose(shapeRef); }

bool ShapeMapDM::polyCancel(int shapeRef) {
    bool polyCancelled = getInternalMap().polyCancel(shapeRef);

    m_undobuffer.pop_back();
    // update displayed attribute
    invalidateDisplayedAttribute();
    setDisplayedAttribute(m_displayedAttribute);
    return polyCancelled;
}

bool ShapeMapDM::removeSelected() {
    // note, map must have editable flag on if we are to remove shape:
    if (m_editable) {
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

void ShapeMapDM::removeShape(int shaperef, bool undoing) {

    auto shapeIter = getInternalMap().getAllShapes().find(shaperef);
    if (shapeIter == getInternalMap().getAllShapes().end()) {
        throw genlib::RuntimeException("Shape with ref " + std::to_string(shaperef) +
                                       " not found when trying to remove it");
    }
    if (!undoing) { // <- if not currently undoing another event, then add to the
                    // undo buffer:
        m_undobuffer.push_back(SalaEvent(SalaEvent::SALA_DELETED, shaperef));
        m_undobuffer.back().geometry = shapeIter->second;
    }

    getInternalMap().removeShape(shaperef);

    m_invalidate = true;
    m_newshape = true;
}

void ShapeMapDM::undo() {

    if (m_undobuffer.size() == 0) {
        return;
    }

    SalaEvent &event = m_undobuffer.back();

    if (event.action == SalaEvent::SALA_CREATED) {

        removeShape(event.shapeRef,
                    true); // <- note, must tell remove shape it's an undo, or it
                           // will add this remove to the undo stack!

    } else if (event.action == SalaEvent::SALA_DELETED) {

        makeShape(event.geometry, event.shapeRef);
        auto &shapes = getInternalMap().getAllShapes();
        auto &connectors = getInternalMap().getConnections();
        auto &attributes = getInternalMap().getAttributeTable();
        auto &links = getInternalMap().getLinks();
        auto &unlinks = getInternalMap().getUnlinks();
        auto &region = getInternalMap().getRegion();
        auto rowIt = shapes.find(event.shapeRef);

        if (rowIt != shapes.end() && getInternalMap().hasGraph()) {
            auto rowid = static_cast<size_t>(std::distance(shapes.begin(), rowIt));
            auto &row = attributes.getRow(AttributeKey(event.shapeRef));
            // redo connections... n.b. TO DO this is intended to use the slower "any
            // connection" method, so it can handle any sort of graph
            // ...but that doesn't exist yet, so for the moment do lines:
            //
            // insert new connector at the row:
            connectors[rowid] = Connector();
            //
            // now go through all connectors, ensuring they're reindexed above this
            // one: Argh!  ...but, remember the reason we're doing this is for fast
            // processing elsewhere
            // -- this is a user triggered *undo*, they'll just have to wait:
            for (size_t i = 0; i < connectors.size(); i++) {
                for (size_t j = 0; j < connectors[i].connections.size(); j++) {
                    if (connectors[i].connections[j] >= rowid) {
                        connectors[i].connections[j] += 1;
                    }
                }
            }
            // it gets worse, the links and unlinks will also be all over the shop due
            // to the inserted row:
            size_t j;
            for (j = 0; j < links.size(); j++) {
                if (links[j].a >= rowid)
                    links[j].a += 1;
                if (links[j].b >= rowid)
                    links[j].b += 1;
            }
            for (j = 0; j < unlinks.size(); j++) {
                if (unlinks[j].a >= rowid)
                    unlinks[j].a += 1;
                if (unlinks[j].b >= rowid)
                    unlinks[j].b += 1;
            }
            //
            // calculate this line's connections
            connectors[rowid].connections = getInternalMap().getLineConnections(
                event.shapeRef, TOLERANCE_B * std::max(region.height(), region.width()));
            // update:
            auto connCol = attributes.getOrInsertLockedColumn("Connectivity");
            row.setValue(connCol, static_cast<float>(connectors[rowid].connections.size()));
            //
            if (event.geometry.isLine()) {
                auto lengCol = attributes.getOrInsertLockedColumn("Line Length");
                row.setValue(
                    lengCol,
                    static_cast<float>(genlib::getMapAtIndex(shapes, rowid)->second.getLength()));
            }
            //
            // now go through our connections, and add ourself:
            const auto &connections = connectors[rowid].connections;
            for (auto connection : connections) {
                if (connection != rowid) { // <- exclude self!
                    genlib::insert_sorted(connectors[connection].connections, rowid);
                    getInternalMap().getAttributeRowFromShapeIndex(connection).incrValue(connCol);
                }
            }
        }
    } else if (event.action == SalaEvent::SALA_MOVED) {

        moveShape(event.shapeRef, event.geometry.getLine(),
                  true); // <- note, must tell remove shape it's an undo, or it will
                         // add this remove to the undo stack!
    }

    m_undobuffer.pop_back();

    invalidateDisplayedAttribute();
    setDisplayedAttribute(m_displayedAttribute);
    m_newshape = true;
}

void ShapeMapDM::makeShapeConnections() {
    getInternalMap().makeShapeConnections();

    m_displayedAttribute = -1; // <- override if it's already showing
    auto connCol = getInternalMap().getAttributeTable().getColumnIndex("Connectivity");

    setDisplayedAttribute(static_cast<int>(connCol));
}

double ShapeMapDM::getLocationValue(const Point2f &point) const {
    if (m_displayedAttribute == -1) {
        return getInternalMap().getLocationValue(point, std::nullopt);
    }
    return getInternalMap().getLocationValue(point, m_displayedAttribute);
}

const PafColor ShapeMapDM::getShapeColor() const {
    AttributeKey key(static_cast<int>(m_displayShapes[static_cast<size_t>(m_currentShape)]));
    const AttributeRow &row = getInternalMap().getAttributeTable().getRow(key);
    return dXreimpl::getDisplayColor(key, row, getInternalMap().getAttributeTableHandle(),
                                     m_selectionSet, true);
}

bool ShapeMapDM::getShapeSelected() const {
    return m_selectionSet.find(static_cast<int>(
               m_displayShapes[static_cast<size_t>(m_currentShape)])) != m_selectionSet.end();
}

bool ShapeMapDM::linkShapes(const Point2f &p) {
    if (m_selectionSet.size() != 1) {
        return false;
    }
    return getInternalMap().linkShapes(p, *m_selectionSet.begin());
}

bool ShapeMapDM::unlinkShapes(const Point2f &p) {
    if (m_selectionSet.size() != 1) {
        return false;
    }
    clearSel();
    return getInternalMap().unlinkShapes(p, *m_selectionSet.begin());
}

bool ShapeMapDM::findNextLinkLine() const {
    if (m_curlinkline < static_cast<int>(getInternalMap().getLinks().size())) {
        m_curlinkline++;
    }
    return (m_curlinkline < static_cast<int>(getInternalMap().getLinks().size()));
}

Line4f ShapeMapDM::getNextLinkLine() const {
    // note, links are stored directly by rowid, not by key:
    if (m_curlinkline < static_cast<int>(getInternalMap().getLinks().size())) {
        return Line4f(
            genlib::getMapAtIndex(getInternalMap().getAllShapes(),
                                  getInternalMap().getLinks()[static_cast<size_t>(m_curlinkline)].a)
                ->second.getCentroid(),
            genlib::getMapAtIndex(getInternalMap().getAllShapes(),
                                  getInternalMap().getLinks()[static_cast<size_t>(m_curlinkline)].b)
                ->second.getCentroid());
    }
    return Line4f();
}
bool ShapeMapDM::findNextUnlinkPoint() const {
    if (m_curunlinkpoint < static_cast<int>(getInternalMap().getUnlinks().size())) {
        m_curunlinkpoint++;
    }
    return (m_curunlinkpoint < static_cast<int>(getInternalMap().getUnlinks().size()));
}

Point2f ShapeMapDM::getNextUnlinkPoint() const {
    // note, links are stored directly by rowid, not by key:
    if (m_curunlinkpoint < static_cast<int>(getInternalMap().getUnlinks().size())) {
        return genlib::getMapAtIndex(
                   getInternalMap().getAllShapes(),
                   getInternalMap().getUnlinks()[static_cast<size_t>(m_curunlinkpoint)].a)
            ->second.getLine()
            .intersection_point(
                genlib::getMapAtIndex(
                    getInternalMap().getAllShapes(),
                    getInternalMap().getUnlinks()[static_cast<size_t>(m_curunlinkpoint)].b)
                    ->second.getLine(),
                TOLERANCE_A);
    }
    return Point2f();
}

void ShapeMapDM::getPolygonDisplay(bool &showLines, bool &showFill, bool &showCentroids) {
    showLines = m_showLines;
    showFill = m_showFill;
    showCentroids = m_showCentroids;
}

void ShapeMapDM::setPolygonDisplay(bool showLines, bool showFill, bool showCentroids) {
    m_showLines = showLines;
    m_showFill = showFill;
    m_showCentroids = showCentroids;
}

std::vector<std::pair<SimpleLine, PafColor>>
ShapeMapDM::getAllLinesWithColour(const std::set<int> &selSet) {
    return getInternalMap().getAllSimpleLinesWithColour(selSet);
}

std::vector<std::pair<std::vector<Point2f>, PafColor>>
ShapeMapDM::getAllPolygonsWithColour(const std::set<int> &selSet) {
    return getInternalMap().getAllPolygonsWithColour(selSet);
}

std::vector<std::pair<Point2f, PafColor>>
ShapeMapDM::getAllPointsWithColour(const std::set<int> &selSet) {
    return getInternalMap().getAllPointsWithColour(selSet);
}

std::vector<Point2f> ShapeMapDM::getAllUnlinkPoints() {
    return getInternalMap().getAllUnlinkPoints();
}

bool ShapeMapDM::setCurSel(const std::vector<int> &selset, bool add) {
    if (add == false) {
        clearSel();
    }

    for (auto shape : selset) {
        m_selectionSet.insert(shape);
    }

    return !m_selectionSet.empty();
}

bool ShapeMapDM::setCurSel(Region4f &r, bool add) {
    if (add == false) {
        clearSel();
    }

    std::map<int, SalaShape> shapesInRegion = getInternalMap().getShapesInRegion(r);
    for (auto &shape : shapesInRegion) {
        m_selectionSet.insert(shape.first);
    }

    return !shapesInRegion.empty();
}

float ShapeMapDM::getSelectedAvg(size_t attributeIdx) {
    return getInternalMap().getAttributeTable().getSelAvg(attributeIdx, m_selectionSet);
}

bool ShapeMapDM::clearSel() {
    // note, only clear if need be, as m_attributes->deselectAll is slow
    if (!m_selectionSet.empty()) {
        m_selectionSet.clear();
    }
    return true;
}

Region4f ShapeMapDM::getSelBounds() {
    Region4f r;
    if (!m_selectionSet.empty()) {
        for (auto &shapeRef : m_selectionSet) {
            r = r.runion(getInternalMap().getAllShapes().at(shapeRef).getBoundingBox());
        }
    }
    return r;
}

bool ShapeMapDM::selectionToLayer(const std::string &name) {
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
