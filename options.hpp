// SPDX-FileCopyrightText: 2011-2012 Tasos Varoudis
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "salalib/analysistype.hpp"
#include "salalib/radiustype.hpp"

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
    Options()
        : outputType(AnalysisType::ISOVIST), local(0), global(1), cliques(0), choice(false),
          fulloutput(false), radiusType(RadiusType::TOPOLOGICAL), radius(-1), radiusSet(),
          pointDepthSelection(0), tulipBins(1024), processInMemory(false), selOnly(false),
          gatesOnly(false), gatelayer(-1), weightedMeasureCol(-1), weightedMeasureCol2(-1),
          routeweightCol(-1), outputFile() {}
};
