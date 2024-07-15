// SPDX-FileCopyrightText: 2000-2010 University College London, Alasdair Turner
// SPDX-FileCopyrightText: 2011-2012 Tasos Varoudis
// SPDX-FileCopyrightText: 2024 Petros Koutsolampros
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pointmapdx.h"

void PointMapDX::setDisplayedAttribute(int col) {
    if (m_displayed_attribute == col) {
        if (getInternalMap().getAttributeTableHandle().getDisplayColIndex() !=
            m_displayed_attribute) {
            getInternalMap().getAttributeTableHandle().setDisplayColIndex(m_displayed_attribute);
        }
        return;
    } else {
        m_displayed_attribute = col;
    }

    getInternalMap().getAttributeTableHandle().setDisplayColIndex(m_displayed_attribute);
}

void PointMapDX::setDisplayedAttribute(const std::string &col) {
    setDisplayedAttribute(
        static_cast<int>(getInternalMap().getAttributeTable().getColumnIndex(col)));
}

bool PointMapDX::read(std::istream &stream) {
    bool read = getInternalMap().readMetadata(stream);

    // NOTE: You MUST set m_spacepix manually!
    m_displayed_attribute = -1;

    int displayed_attribute; // n.b., temp variable necessary to force recalc
                             // below

    // our data read
    stream.read((char *)&displayed_attribute, sizeof(displayed_attribute));

    read = read && getInternalMap().readPointsAndAttributes(stream);

    m_selection = NO_SELECTION;
    m_pinned_selection = false;

    // now, as soon as loaded, must recalculate our screen display:
    // note m_displayed_attribute should be -2 in order to force recalc...
    m_displayed_attribute = -2;
    setDisplayedAttribute(displayed_attribute);
    return read;
}

bool PointMapDX::write(std::ostream &stream) {
    bool written = getInternalMap().writeMetadata(stream);

    // TODO: Compatibility. The attribute columns will be stored sorted
    // alphabetically so the displayed attribute needs to match that
    auto sortedDisplayedAttribute =
        static_cast<int>(getInternalMap().getAttributeTable().getColumnSortedIndex(
            static_cast<size_t>(m_displayed_attribute)));
    stream.write((char *)&sortedDisplayedAttribute, sizeof(sortedDisplayedAttribute));

    written = written && getInternalMap().writePointsAndAttributes(stream);
    return written;
}

void PointMapDX::copy(const PointMapDX &sourcemap, bool copypoints, bool copyattributes) {
    getInternalMap().copy(sourcemap.getInternalMap(), copypoints, copyattributes);

    // -2 follows axial map convention, where -1 is the reference number
    m_displayed_attribute = sourcemap.m_displayed_attribute;

    m_selection = sourcemap.m_selection;
    m_pinned_selection = sourcemap.m_pinned_selection;

    s_bl = sourcemap.s_bl;
    s_tr = sourcemap.s_tr;

    // screen
    m_viewing_deprecated = sourcemap.m_viewing_deprecated;
    m_draw_step = sourcemap.m_draw_step;

    curmergeline = sourcemap.curmergeline;
}

// -2 for point not in visibility graph, -1 for point has no data
double PointMapDX::getLocationValue(const Point2f &point) {
    if (m_displayed_attribute == -1) {
        return getInternalMap().getLocationValue(point, std::nullopt);
    }
    return getInternalMap().getLocationValue(point, m_displayed_attribute);
}

// Selection stuff

// eventually we will use returned info to draw the selected point quickly

bool PointMapDX::clearSel() {
    if (m_selection == NO_SELECTION) {
        return false;
    }
    m_selectionSet.clear();
    m_selection = NO_SELECTION;
    return true;
}

bool PointMapDX::setCurSel(QtRegion &r, bool add) {
    if (m_selection == NO_SELECTION) {
        add = false;
    } else if (!add) {
        // Since we started using point locations in the sel set this is a lot
        // easier!
        clearSel();
    }

    // n.b., assumes constrain set to true (for if you start the selection off the
    // grid)
    s_bl = getInternalMap().pixelate(r.bottom_left, true);
    s_tr = getInternalMap().pixelate(r.top_right, true);

    if (!add) {
        m_sel_bounds = r;
    } else {
        m_sel_bounds = runion(m_sel_bounds, r);
    }

    int mask = 0;
    mask |= Point::FILLED;

    for (auto i = s_bl.x; i <= s_tr.x; i++) {
        for (auto j = s_bl.y; j <= s_tr.y; j++) {
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
    r = QtRegion(depixelate(s_bl), depixelate(s_tr));

    return true;
}

bool PointMapDX::setCurSel(const std::vector<int> &selset, bool add) {
    // note: override cursel, can only be used with analysed pointdata:
    if (!add) {
        clearSel();
    }
    m_selection = COMPOUND_SELECTION;
    // not sure what to do with m_sel_bounds (is it necessary?)
    for (size_t i = 0; i < selset.size(); i++) {
        PixelRef pix = selset[i];
        if (getInternalMap().includes(pix)) {
            m_selectionSet.insert(pix);
        }
    }
    return true;
}

void PointMapDX::setScreenPixel(double unit) {
    if (unit / getInternalMap().getSpacing() > 1) {
        m_draw_step = int(unit / getInternalMap().getSpacing());
    } else {
        m_draw_step = 1;
    }
}

void PointMapDX::makeViewportPoints(const QtRegion &viewport) const {
    // n.b., relies on "constrain" being set to true
    bl = pixelate(viewport.bottom_left, true);
    cur = bl;   // cursor for points
    cur.x -= 1; // findNext expects to find cur.x in the -1 position
    rc = bl;    // cursor for grid lines
    prc = bl;   // cursor for point centre grid lines
    prc.x -= 1;
    prc.y -= 1;
    // n.b., relies on "constrain" being set to true
    tr = pixelate(viewport.top_right, true);
    curmergeline = -1;

    m_finished = false;
}

bool PointMapDX::findNextPoint() const {
    if (m_finished) {
        return false;
    }
    do {
        cur.x += static_cast<short>(m_draw_step);
        if (cur.x > tr.x) {
            cur.x = bl.x;
            cur.y += static_cast<short>(m_draw_step);
            if (cur.y > tr.y) {
                cur = tr; // safety first --- this will at least return something
                m_finished = true;
                return false;
            }
        }
    } while (!getPoint(cur).filled() && !getPoint(cur).blocked());
    return true;
}

bool PointMapDX::findNextRow() const {
    rc.y += 1;
    if (rc.y > tr.y)
        return false;
    return true;
}
Line PointMapDX::getNextRow() const {
    Point2f offset(getSpacing() / 2.0, getSpacing() / 2.0);
    return Line(depixelate(PixelRef(bl.x, rc.y)) - offset,
                depixelate(PixelRef(tr.x + 1, rc.y)) - offset);
}
bool PointMapDX::findNextPointRow() const {
    prc.y += 1;
    if (prc.y > tr.y)
        return false;
    return true;
}
Line PointMapDX::getNextPointRow() const {
    Point2f offset(getSpacing() / 2.0, 0);
    return Line(depixelate(PixelRef(bl.x, prc.y)) - offset,
                depixelate(PixelRef(tr.x + 1, prc.y)) - offset);
}
bool PointMapDX::findNextCol() const {
    rc.x += 1;
    if (rc.x > tr.x)
        return false;
    return true;
}
Line PointMapDX::getNextCol() const {
    Point2f offset(getSpacing() / 2.0, getSpacing() / 2.0);
    return Line(depixelate(PixelRef(rc.x, bl.y)) - offset,
                depixelate(PixelRef(rc.x, tr.y + 1)) - offset);
}
bool PointMapDX::findNextPointCol() const {
    prc.x += 1;
    if (prc.x > tr.x)
        return false;
    return true;
}
Line PointMapDX::getNextPointCol() const {
    Point2f offset(0.0, getSpacing() / 2.0);
    return Line(depixelate(PixelRef(prc.x, bl.y)) - offset,
                depixelate(PixelRef(prc.x, tr.y + 1)) - offset);
}

bool PointMapDX::findNextMergeLine() const {
    if (curmergeline < (int)getInternalMap().getMergeLines().size()) {
        curmergeline++;
    }
    return (curmergeline < (int)getInternalMap().getMergeLines().size());
}

Line PointMapDX::getNextMergeLine() const {
    if (curmergeline < (int)getInternalMap().getMergeLines().size()) {
        return Line(
            depixelate(getInternalMap().getMergeLines()[static_cast<size_t>(curmergeline)].a),
            depixelate(getInternalMap().getMergeLines()[static_cast<size_t>(curmergeline)].b));
    }
    return Line();
}

bool PointMapDX::refInSelectedSet(const PixelRef &ref) const {
    return m_selectionSet.find(ref) != m_selectionSet.end();
}

bool PointMapDX::getPointSelected() const { return refInSelectedSet(cur); }

PafColor PointMapDX::getPointColor(PixelRef pixelRef) const {
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
            return PafColor();
        }
    }
    return PafColor(); // <- note alpha channel set to transparent - will not be
                       // drawn
}

PafColor PointMapDX::getCurrentPointColor() const { return getPointColor(cur); }
