// SPDX-FileCopyrightText: 2000-2010 University College London, Alasdair Turner
// SPDX-FileCopyrightText: 2011-2012 Tasos Varoudis
// SPDX-FileCopyrightText: 2024 Petros Koutsolampros
//
// SPDX-License-Identifier: GPL-3.0-or-later

// A representation of a sala ShapeGraph in depthmapX

#pragma once

#include "shapemapdx.hpp"

#include "salalib/shapegraph.hpp"

class ShapeGraphDX : public ShapeMapDX {

  public:
    ShapeGraphDX(std::unique_ptr<ShapeGraph> &&map) : ShapeMapDX(std::move(map)) {}

    ShapeGraph &getInternalMap() override { return *static_cast<ShapeGraph *>(m_map.get()); }
    const ShapeGraph &getInternalMap() const override {
        return *static_cast<ShapeGraph *>(m_map.get());
    }

    void makeConnections(const KeyVertices &keyvertices);

    void unlinkFromShapeMap(const ShapeMap &shapemap);

    void makeSegmentConnections(std::vector<Connector> &connectionset);

    bool read(std::istream &stream);
    bool write(std::ostream &stream);
    std::vector<SimpleLine> getAllLinkLines() { return getInternalMap().getAllLinkLines(); }

    auto isSegmentMap() { return getInternalMap().isSegmentMap(); }
    auto isAllLineMap() { return getInternalMap().isAllLineMap(); }
};
