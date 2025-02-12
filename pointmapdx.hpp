// SPDX-FileCopyrightText: 2000-2010 University College London, Alasdair Turner
// SPDX-FileCopyrightText: 2011-2012 Tasos Varoudis
// SPDX-FileCopyrightText: 2024 Petros Koutsolampros
//
// SPDX-License-Identifier: GPL-3.0-or-later

// A representation of a sala PointMap in depthmapX

#pragma once

#include "attributemapdx.hpp"

#include "salalib/pointmap.hpp"

class PointMapDX : public AttributeMapDX {

    enum {
        NO_SELECTION = 0,
        SINGLE_SELECTION = 1,
        COMPOUND_SELECTION = 2,
        LAYER_SELECTION = 4,
        OVERRIDE_SELECTION = 8
    };

    // Selection functionality
    int m_selection;
    bool m_pinnedSelection;
    std::set<int> m_selectionSet; // n.b., m_selection_set stored as int for compatibility with
                                  // other map layers
    mutable PixelRef m_sBl;
    mutable PixelRef m_sTr;

    mutable int m_curmergeline;
    mutable Region4f m_selBounds;

    int m_viewingDeprecated;
    int m_drawStep;

    mutable bool m_finished = false;
    mutable PixelRef m_bl;
    mutable PixelRef m_cur; // cursor for points
    mutable PixelRef m_rc;  // cursor for grid lines
    mutable PixelRef m_prc; // cursor for point lines
    mutable PixelRef m_tr;

  protected:
    // which attribute is currently displayed:
    mutable int m_displayedAttribute;

  public: // ctor
    PointMapDX(std::unique_ptr<PointMap> &&map)
        : AttributeMapDX(std::move(map)), m_selection(NO_SELECTION), m_pinnedSelection(false),
          m_sBl(NoPixel), m_sTr(NoPixel), m_curmergeline(-1), m_viewingDeprecated(-1),
          m_drawStep(1), m_displayedAttribute(-2){
                             // -2 follows axial map convention, where -1 is the reference number
                             // screen

                         };
    ~PointMapDX() override {}
    PointMapDX() = delete;
    PointMapDX(const PointMapDX &other) = delete;
    PointMapDX(PointMapDX &&other) = default;
    PointMapDX &operator=(PointMapDX &&other) = default;

  public: // methods
    PointMap &getInternalMap() override { return *static_cast<PointMap *>(m_map.get()); }
    const PointMap &getInternalMap() const override {
        return *static_cast<PointMap *>(m_map.get());
    }

    double getDisplayMinValue() const {
        return (m_displayedAttribute != -1)
                   ? getInternalMap().getDisplayMinValue(static_cast<size_t>(m_displayedAttribute))
                   : 0;
    }

    double getDisplayMaxValue() const {
        return (m_displayedAttribute != -1)
                   ? getInternalMap().getDisplayMaxValue(static_cast<size_t>(m_displayedAttribute))
                   : getInternalMap().pixelate(getInternalMap().getRegion().topRight).x;
    }

    const DisplayParams &getDisplayParams() const {
        return getInternalMap().getDisplayParams(static_cast<size_t>(m_displayedAttribute));
    }
    // make a local copy of the display params for access speed:
    void setDisplayParams(const DisplayParams &dp, bool applyToAll = false) {
        getInternalMap().setDisplayParams(dp, static_cast<size_t>(m_displayedAttribute),
                                          applyToAll);
    }

    void setDisplayedAttribute(int col);
    void setDisplayedAttribute(const std::string &col);
    // use set displayed attribute instead unless you are deliberately changing the column order:
    void overrideDisplayedAttribute(int attribute) { m_displayedAttribute = attribute; }
    // now, there is a slightly odd thing here: the displayed attribute can go out of step with the
    // underlying attribute data if there is a delete of an attribute in idepthmap.h, so it just
    // needs checking before returning!
    int getDisplayedAttribute() const {
        if (m_displayedAttribute == getInternalMap().getAttributeTableHandle().getDisplayColIndex())
            return m_displayedAttribute;
        if (getInternalMap().getAttributeTableHandle().getDisplayColIndex() != -2) {
            m_displayedAttribute = getInternalMap().getAttributeTableHandle().getDisplayColIndex();
        }
        return m_displayedAttribute;
    }

    float getDisplayedSelectedAvg() {
        return (getInternalMap().getDisplayedSelectedAvg(static_cast<size_t>(m_displayedAttribute),
                                                         m_selectionSet));
    }

    bool write(std::ostream &stream);
    bool read(std::istream &stream);
    void copy(const PointMapDX &sourcemap, bool copypoints, bool copyattributes);

    double getLocationValue(const Point2f &point);

    bool isSelected() const // does a selection exist
    {
        return m_selection != NO_SELECTION;
    }
    bool clearSel();                               // clear the current selection
    bool setCurSel(Region4f &r, bool add = false); // set current selection
    bool setCurSel(const std::vector<int> &selset, bool add = false);
    // Note: passed by ref, use with care in multi-threaded app
    std::set<int> &getSelSet() { return m_selectionSet; }
    const std::set<int> &getSelSet() const { return m_selectionSet; }

    size_t getSelCount() { return m_selectionSet.size(); }
    const Region4f &getSelBounds() const { return m_selBounds; }

    bool clearPoints() {
        bool result = false;
        if (m_selection == NO_SELECTION) {
            result = getInternalMap().clearAllPoints();
        } else if (m_selection & SINGLE_SELECTION) {
            result = getInternalMap().clearPointsInRange(m_sBl, m_sTr, m_selectionSet);
        } else { // COMPOUND_SELECTION (note, need to test bitwise now)
            result = getInternalMap().clearPointsInRange(
                PixelRef(0, 0),
                PixelRef(static_cast<short>(getInternalMap().getRows()),
                         static_cast<short>(getInternalMap().getCols())),
                m_selectionSet);
        }
        m_selectionSet.clear();
        m_selection = NO_SELECTION;
        return result;
    }

    bool refInSelectedSet(const PixelRef &ref) const;

    void setScreenPixel(double mUnit);
    void makeViewportPoints(const Region4f &viewport) const;
    bool findNextPoint() const;
    Point2f getNextPointLocation() const { return getInternalMap().getPoint(m_cur).getLocation(); }
    bool findNextRow() const;
    Line4f getNextRow() const;
    bool findNextPointRow() const;
    Line4f getNextPointRow() const;
    bool findNextCol() const;
    Line4f getNextCol() const;
    bool findNextPointCol() const;
    Line4f getNextPointCol() const;
    bool findNextMergeLine() const;
    Line4f getNextMergeLine() const;
    bool getPointSelected() const;
    PafColor getPointColor(PixelRef pixelRef) const;
    PafColor getCurrentPointColor() const;

    size_t tagState(bool settag) {
        m_selectionSet.clear();
        m_selection = NO_SELECTION;
        return getInternalMap().tagState(settag);
    }

    bool binDisplay(Communicator *) { return getInternalMap().binDisplay(nullptr, m_selectionSet); }

    // Merge connections... very fiddly indeed... using a simple method for now...
    // ...and even that's too complicated... so I'll settle for a very, very simple
    // merge points (just a reference to another point --- yes, that simple!)

    // the first point should have been selected, the second is chosen now:

    auto mergePoints(const Point2f &p) {
        if (!m_selectionSet.size()) {
            return false;
        }
        auto pointsMerged = getInternalMap().mergePoints(p, m_selBounds, m_selectionSet);

        clearSel();
        return pointsMerged;
    }

    auto unmergePoints() {
        if (!m_selectionSet.size()) {
            return false;
        }
        auto pointsUnmerged = getInternalMap().unmergePoints(m_selectionSet);
        clearSel();
        return pointsUnmerged;
    }

    // Simple wrappers
    auto getName() { return getInternalMap().getName(); }
    auto getSpacing() const { return getInternalMap().getSpacing(); }
    auto getCols() const { return getInternalMap().getCols(); }
    auto getRows() const { return getInternalMap().getRows(); }
    auto getFilledPointCount() const { return getInternalMap().getFilledPointCount(); }
    auto fillPoint(const Point2f &p, bool add = true) { return getInternalMap().fillPoint(p, add); }
    auto depixelate(const PixelRef &p, double scalefactor = 1.0) const {
        return getInternalMap().depixelate(p, scalefactor);
    }
    auto pixelate(const Point2f &p, bool constrain = true, int scalefactor = 1) const {
        return getInternalMap().pixelate(p, constrain, scalefactor);
    }
    auto includes(const PixelRef pix) const { return getInternalMap().includes(pix); }
    auto &getPoint(const PixelRef &p) const { return getInternalMap().getPoint(p); }
    auto &getPoint(const PixelRef &p) { return getInternalMap().getPoint(p); }
};
