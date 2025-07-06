// SPDX-FileCopyrightText: 2000-2010 University College London, Alasdair Turner
// SPDX-FileCopyrightText: 2011-2012 Tasos Varoudis
// SPDX-FileCopyrightText: 2024 Petros Koutsolampros
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pointmapdm.hpp"

void PointMapDM::setDisplayedAttribute(int col) {
    if (m_displayedAttribute == col) {
        if (getInternalMap().getAttributeTableHandle().getDisplayColIndex() !=
            m_displayedAttribute) {
            getInternalMap().getAttributeTableHandle().setDisplayColIndex(m_displayedAttribute);
        }
        return;
    } else {
        m_displayedAttribute = col;
    }

    getInternalMap().getAttributeTableHandle().setDisplayColIndex(m_displayedAttribute);
}

void PointMapDM::setDisplayedAttribute(const std::string &col) {
    setDisplayedAttribute(
        static_cast<int>(getInternalMap().getAttributeTable().getColumnIndex(col)));
}

bool PointMapDM::read(std::istream &stream) {
    bool read = getInternalMap().readMetadata(stream);

    // NOTE: You MUST set m_spacepix manually!
    m_displayedAttribute = -1;

    int displayedAttribute; // n.b., temp variable necessary to force recalc
                            // below

    // our data read
    stream.read(reinterpret_cast<char *>(&displayedAttribute), sizeof(displayedAttribute));

    read = read && getInternalMap().readPointsAndAttributes(stream);

    m_selection = NO_SELECTION;
    m_pinnedSelection = false;

    // now, as soon as loaded, must recalculate our screen display:
    // note m_displayedAttribute should be -2 in order to force recalc...
    m_displayedAttribute = -2;
    setDisplayedAttribute(displayedAttribute);
    return read;
}

bool PointMapDM::write(std::ostream &stream) {
    bool written = getInternalMap().writeMetadata(stream);

    // TODO: Compatibility. The attribute columns will be stored sorted
    // alphabetically so the displayed attribute needs to match that
    auto sortedDisplayedAttribute =
        static_cast<int>(getInternalMap().getAttributeTable().getColumnSortedIndex(
            static_cast<size_t>(m_displayedAttribute)));
    stream.write(reinterpret_cast<const char *>(&sortedDisplayedAttribute),
                 sizeof(sortedDisplayedAttribute));

    written = written && getInternalMap().writePointsAndAttributes(stream);
    return written;
}

void PointMapDM::copy(const PointMapDM &sourcemap, bool copypoints, bool copyattributes) {
    getInternalMap().copy(sourcemap.getInternalMap(), copypoints, copyattributes);

    // -2 follows axial map convention, where -1 is the reference number
    m_displayedAttribute = sourcemap.m_displayedAttribute;

    m_selection = sourcemap.m_selection;
    m_pinnedSelection = sourcemap.m_pinnedSelection;

    m_sBl = sourcemap.m_sBl;
    m_sTr = sourcemap.m_sTr;

    // screen
    m_viewingDeprecated = sourcemap.m_viewingDeprecated;
    m_drawStep = sourcemap.m_drawStep;

    m_curmergeline = sourcemap.m_curmergeline;
}

// -2 for point not in visibility graph, -1 for point has no data
double PointMapDM::getLocationValue(const Point2f &point) {
    if (m_displayedAttribute == -1) {
        return getInternalMap().getLocationValue(point, std::nullopt);
    }
    return getInternalMap().getLocationValue(point, m_displayedAttribute);
}

// Selection stuff

// eventually we will use returned info to draw the selected point quickly

bool PointMapDM::clearSel() {
    if (m_selection == NO_SELECTION) {
        return false;
    }
    m_selectionSet.clear();
    m_selection = NO_SELECTION;
    return true;
}

bool PointMapDM::setCurSel(Region4f &r, bool add) {
    if (m_selection == NO_SELECTION) {
        add = false;
    } else if (!add) {
        // Since we started using point locations in the sel set this is a lot
        // easier!
        clearSel();
    }

    // n.b., assumes constrain set to true (for if you start the selection off the
    // grid)
    m_sBl = getInternalMap().pixelate(r.bottomLeft, true);
    m_sTr = getInternalMap().pixelate(r.topRight, true);

    if (!add) {
        m_selBounds = r;
    } else {
        m_selBounds = m_selBounds.runion(r);
    }

    int mask = 0;
    mask |= Point::FILLED;

    for (auto i = m_sBl.x; i <= m_sTr.x; i++) {
        for (auto j = m_sBl.y; j <= m_sTr.y; j++) {
            if (getPoint(PixelRef(i, j)).getState() & mask) {
                m_selectionSet.insert(PixelRef(i, j));
                if (add) {
                    m_selection &= ~SINGLE_SELECTION;
                    m_selection |= COMPOUND_SELECTION;
                } else {
                    m_selection |= SINGLE_SELECTION;
                }
            }
        }
    }

    // Set the region to our actual region:
    r = Region4f(depixelate(m_sBl), depixelate(m_sTr));

    return true;
}

bool PointMapDM::setCurSel(const std::vector<int> &selset, bool add) {
    // note: override cursel, can only be used with analysed pointdata:
    if (!add) {
        clearSel();
    }
    m_selection = COMPOUND_SELECTION;
    // not sure what to do with m_selBounds (is it necessary?)
    for (size_t i = 0; i < selset.size(); i++) {
        PixelRef pix = selset[i];
        if (getInternalMap().includes(pix)) {
            m_selectionSet.insert(pix);
        }
    }
    return true;
}

void PointMapDM::setScreenPixel(double unit) {
    if (unit / getInternalMap().getSpacing() > 1) {
        m_drawStep = static_cast<int>(unit / getInternalMap().getSpacing());
    } else {
        m_drawStep = 1;
    }
}

void PointMapDM::makeViewportPoints(const Region4f &viewport) const {
    // n.b., relies on "constrain" being set to true
    m_bl = pixelate(viewport.bottomLeft, true);
    m_cur = m_bl; // cursor for points
    m_cur.x -= 1; // findNext expects to find cur.x in the -1 position
    m_rc = m_bl;  // cursor for grid lines
    m_prc = m_bl; // cursor for point centre grid lines
    m_prc.x -= 1;
    m_prc.y -= 1;
    // n.b., relies on "constrain" being set to true
    m_tr = pixelate(viewport.topRight, true);
    m_curmergeline = -1;

    m_finished = false;
}

bool PointMapDM::findNextPoint() const {
    if (m_finished) {
        return false;
    }
    do {
        m_cur.x += static_cast<short>(m_drawStep);
        if (m_cur.x > m_tr.x) {
            m_cur.x = m_bl.x;
            m_cur.y += static_cast<short>(m_drawStep);
            if (m_cur.y > m_tr.y) {
                m_cur = m_tr; // safety first --- this will at least return something
                m_finished = true;
                return false;
            }
        }
    } while (!getPoint(m_cur).filled() && !getPoint(m_cur).blocked());
    return true;
}

bool PointMapDM::findNextRow() const {
    m_rc.y += 1;
    if (m_rc.y > m_tr.y)
        return false;
    return true;
}
Line4f PointMapDM::getNextRow() const {
    Point2f offset(getSpacing() / 2.0, getSpacing() / 2.0);
    return Line4f(depixelate(PixelRef(m_bl.x, m_rc.y)) - offset,
                  depixelate(PixelRef(m_tr.x + 1, m_rc.y)) - offset);
}
bool PointMapDM::findNextPointRow() const {
    m_prc.y += 1;
    if (m_prc.y > m_tr.y)
        return false;
    return true;
}
Line4f PointMapDM::getNextPointRow() const {
    Point2f offset(getSpacing() / 2.0, 0);
    return Line4f(depixelate(PixelRef(m_bl.x, m_prc.y)) - offset,
                  depixelate(PixelRef(m_tr.x + 1, m_prc.y)) - offset);
}
bool PointMapDM::findNextCol() const {
    m_rc.x += 1;
    if (m_rc.x > m_tr.x)
        return false;
    return true;
}
Line4f PointMapDM::getNextCol() const {
    Point2f offset(getSpacing() / 2.0, getSpacing() / 2.0);
    return Line4f(depixelate(PixelRef(m_rc.x, m_bl.y)) - offset,
                  depixelate(PixelRef(m_rc.x, m_tr.y + 1)) - offset);
}
bool PointMapDM::findNextPointCol() const {
    m_prc.x += 1;
    if (m_prc.x > m_tr.x)
        return false;
    return true;
}
Line4f PointMapDM::getNextPointCol() const {
    Point2f offset(0.0, getSpacing() / 2.0);
    return Line4f(depixelate(PixelRef(m_prc.x, m_bl.y)) - offset,
                  depixelate(PixelRef(m_prc.x, m_tr.y + 1)) - offset);
}

bool PointMapDM::findNextMergeLine() const {
    if (m_curmergeline < static_cast<int>(getInternalMap().getMergeLines().size())) {
        m_curmergeline++;
    }
    return (m_curmergeline < static_cast<int>(getInternalMap().getMergeLines().size()));
}

Line4f PointMapDM::getNextMergeLine() const {
    if (m_curmergeline < static_cast<int>(getInternalMap().getMergeLines().size())) {
        return Line4f(
            depixelate(getInternalMap().getMergeLines()[static_cast<size_t>(m_curmergeline)].a),
            depixelate(getInternalMap().getMergeLines()[static_cast<size_t>(m_curmergeline)].b));
    }
    return Line4f();
}

bool PointMapDM::refInSelectedSet(const PixelRef &ref) const {
    return m_selectionSet.find(ref) != m_selectionSet.end();
}

bool PointMapDM::getPointSelected() const { return refInSelectedSet(m_cur); }

PafColor PointMapDM::getPointColor(PixelRef pixelRef) const {
    PafColor color;
    int state = getInternalMap().pointState(pixelRef);
    if (state & Point::HIGHLIGHT) {
        return PafColor(SALA_HIGHLIGHTED_COLOR);
    } else if (refInSelectedSet(pixelRef)) {
        return PafColor(SALA_SELECTED_COLOR);
    } else {
        if (state & Point::FILLED) {
            if (getInternalMap().isProcessed()) {
                return dXreimpl::getDisplayColor(AttributeKey(pixelRef),
                                                 getAttributeTable().getRow(AttributeKey(pixelRef)),
                                                 getAttributeTableHandle(), m_selectionSet, true);
            } else if (state & Point::EDGE) {
                return PafColor(0x0077BB77);
            } else if (state & Point::CONTEXTFILLED) {
                return PafColor(0x007777BB);
            } else {
                return PafColor(0x00777777);
            }
        } else {
            return PafColor(); // <- note alpha channel set to transparent - will not be drawn
        }
    }
}

PafColor PointMapDM::getCurrentPointColor() const { return getPointColor(m_cur); }
