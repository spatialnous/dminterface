// SPDX-FileCopyrightText: 2000-2010 University College London, Alasdair Turner
// SPDX-FileCopyrightText: 2011-2012 Tasos Varoudis
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "salalib/shapemapgroupdata.hpp"

#include <deque>
#include <string>

class ShapeMapGroupDataDM {

    ShapeMapGroupData m_mapGroupData;
    mutable std::optional<size_t> m_currentLayer;

  public:
    ShapeMapGroupDataDM(ShapeMapGroupData &&mapGroupData)
        : m_mapGroupData(mapGroupData), m_currentLayer(std::nullopt) {}
    ShapeMapGroupDataDM(const std::string &name = std::string())
        : m_mapGroupData(name), m_currentLayer(std::nullopt) {}
    ShapeMapGroupDataDM(ShapeMapGroupDataDM &&other)
        : m_mapGroupData(other.m_mapGroupData), m_currentLayer(other.m_currentLayer) {}
    ShapeMapGroupDataDM &operator=(ShapeMapGroupDataDM &&other) {
        m_mapGroupData = other.m_mapGroupData;
        m_currentLayer = other.m_currentLayer;
        return *this;
    }
    ShapeMapGroupDataDM(const ShapeMapGroupDataDM &) = default;
    ShapeMapGroupDataDM &operator=(const ShapeMapGroupDataDM &) = default;

    void setName(const std::string &name) { m_mapGroupData.name = name; }
    const std::string &getName() const { return m_mapGroupData.name; }

    const Region4f &getRegion() const { return m_mapGroupData.region; }
    void setRegion(Region4f region) { m_mapGroupData.region = region; }

    const ShapeMapGroupData &getInternalData() const { return m_mapGroupData; }

    bool hasCurrentLayer() const { return m_currentLayer.has_value(); }
    std::optional<size_t> getCurrentLayer() const { return m_currentLayer; }
    void invalidateCurrentLayer() const { m_currentLayer = std::nullopt; }
    void setCurrentLayer(size_t currentLayer) const { m_currentLayer = currentLayer; }

  public:
    bool read(std::istream &stream);
    bool write(std::ofstream &stream);
};
