// SPDX-FileCopyrightText: 2000-2010 University College London, Alasdair Turner
// SPDX-FileCopyrightText: 2011-2012 Tasos Varoudis
// SPDX-FileCopyrightText: 2024 Petros Koutsolampros
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "shapegraphdm.hpp"

void ShapeGraphDM::makeConnections(const KeyVertices &keyvertices) {
    getInternalMap().makeConnections(keyvertices);
    m_displayedAttribute = -1; // <- override if it's already showing
    auto connCol =
        getInternalMap().getAttributeTable().getColumnIndex(ShapeGraph::Column::CONNECTIVITY);

    setDisplayedAttribute(static_cast<int>(connCol));
}

void ShapeGraphDM::unlinkFromShapeMap(const ShapeMap &shapemap) {
    getInternalMap().unlinkFromShapeMap(shapemap);

    // reset displayed attribute if it happens to be "Connectivity":
    auto connCol =
        getInternalMap().getAttributeTable().getColumnIndex(ShapeGraph::Column::CONNECTIVITY);
    if (getDisplayedAttribute() == static_cast<int>(connCol)) {
        invalidateDisplayedAttribute();
        setDisplayedAttribute(
            static_cast<int>(connCol)); // <- reflect changes to connectivity counts
    }
}

void ShapeGraphDM::makeSegmentConnections(std::vector<Connector> &connectionset) {
    getInternalMap().makeSegmentConnections(connectionset);

    m_displayedAttribute = -2; // <- override if it's already showing

    auto uwConnCol =
        getInternalMap().getAttributeTable().getColumnIndex(ShapeGraph::Column::CONNECTIVITY);
    setDisplayedAttribute(static_cast<int>(uwConnCol));
}

bool ShapeGraphDM::read(std::istream &stream) {

    bool read = getInternalMap().readShapeGraphData(stream);
    // now base class read:
    read = read && ShapeMapDM::read(stream);

    return read;
}

bool ShapeGraphDM::write(std::ostream &stream) {
    bool written = getInternalMap().writeShapeGraphData(stream);

    // now simply run base class write:
    written = written & ShapeMapDM::write(stream);

    return written;
}
