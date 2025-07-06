// SPDX-FileCopyrightText: 2000-2010 University College London, Alasdair Turner
// SPDX-FileCopyrightText: 2011-2012 Tasos Varoudis
// SPDX-FileCopyrightText: 2024 Petros Koutsolampros
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "salalib/attributemap.hpp"

class AttributeMapDM {

  protected:
    std::unique_ptr<AttributeMap> m_map;

  public:
    AttributeMapDM(std::unique_ptr<AttributeMap> &&map) : m_map(std::move(map)) {}
    AttributeMapDM &operator=(AttributeMapDM &&other) {
        m_map = std::move(other.m_map);
        return *this;
    }
    virtual ~AttributeMapDM() {}
    AttributeMapDM() = delete;
    AttributeMapDM(const AttributeMapDM &other) = delete;
    AttributeMapDM(AttributeMapDM &&other) = default;

    virtual AttributeMap &getInternalMap() { return *m_map; }
    virtual const AttributeMap &getInternalMap() const { return *m_map; }

    const AttributeTable &getAttributeTable() const { return getInternalMap().getAttributeTable(); }
    AttributeTable &getAttributeTable() { return getInternalMap().getAttributeTable(); }
    AttributeTableHandle &getAttributeTableHandle() {
        return getInternalMap().getAttributeTableHandle();
    }
    const AttributeTableHandle &getAttributeTableHandle() const {
        return getInternalMap().getAttributeTableHandle();
    }
    LayerManagerImpl &getLayers() { return getInternalMap().getLayers(); }

    const Region4f &getRegion() const { return m_map->getRegion(); }
};
