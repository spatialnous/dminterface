// SPDX-FileCopyrightText: 2011-2012 Tasos Varoudis
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "salalib/genlib/comm.hpp"

// this is a simple version of the Communicator which can be used for
// an interface

class ICommunicator : public Communicator {
    friend class IComm; // IComm is found in idepthmap.h
                        //
  protected:
    mutable size_t m_numSteps;
    mutable size_t m_numRecords;
    mutable size_t m_step;
    mutable size_t m_record;
    //
  public:
    ICommunicator() : m_numSteps(0), m_numRecords(0), m_step(0), m_record(0) {
        m_deleteFlag = true;
    } // note: an ICommunicator lets IComm know that it should delete it
    ~ICommunicator() override {}
    void CommPostMessage(size_t m, size_t x) const override;

    void logError(const std::string &message) const override { std::cerr << message << std::endl; }
    void logWarning(const std::string &message) const override {
        std::cerr << message << std::endl;
    }
    void logInfo(const std::string &message) const override { std::cout << message << std::endl; }
};

inline void ICommunicator::CommPostMessage(size_t m, size_t x) const {
    switch (m) {
    case Communicator::NUM_STEPS:
        m_numSteps = x;
        break;
    case Communicator::CURRENT_STEP:
        m_step = x;
        break;
    case Communicator::NUM_RECORDS:
        m_numRecords = x;
        break;
    case Communicator::CURRENT_RECORD:
        m_record = x;
        break;
    default:
        break;
    }
}
