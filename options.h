// SPDX-FileCopyrightText: 2011-2012 Tasos Varoudis
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "salalib/analysistype.h"
#include "salalib/radiustype.h"

#include <set>
#include <string>

// Options for mean depth calculations
struct Options {
    // Output type, see above
    AnalysisType outputType;
    // Options for the summary type:
    int local;
    int global;
    int cliques;
    //
    bool choice;
    // include measures that can be derived: RA, RRA and total depth
    bool fulloutput;

    RadiusType radiusType;
    double radius; // <- n.b. for metric integ radius is floating point
    // radius has to go up to a list (phase out radius as is)
    std::set<double> radiusSet;
    //
    int pointDepthSelection;
    int tulipBins;
    bool processInMemory;
    bool selOnly;
    bool gatesOnly;
    // for pushing to a gates layer
    int gatelayer;
    // a column to weight measures by:
    int weightedMeasureCol;
    int weightedMeasureCol2; // EFEF
    int routeweightCol;      // EFEF
    std::string outputFile;  // To save an output graph (for example)
    // default values
    Options() {
        local = 0;
        global = 1;
        cliques = 0;
        choice = false;
        fulloutput = false;
        pointDepthSelection = 0;
        tulipBins = 1024;
        radius = -1;
        radiusType = RadiusType::TOPOLOGICAL;
        outputType = AnalysisType::ISOVIST;
        processInMemory = false;
        gatesOnly = false;
        selOnly = false;
        gatelayer = -1;
        weightedMeasureCol = -1;
    }
};
