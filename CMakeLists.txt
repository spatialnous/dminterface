# SPDX-FileCopyrightText: 2024 Petros Koutsolampros
#
# SPDX-License-Identifier: GPL-3.0-or-later

target_sources(${dminterface}
    PRIVATE
        metagraphdm.cpp
        latticemapdm.cpp
        shapemapdm.cpp
        shapegraphdm.cpp
        options.hpp
    PUBLIC
        comm.hpp
        metagraphdm.hpp
        latticemapdm.hpp
        shapemapdm.hpp
        shapegraphdm.hpp
        shapemapgroupdatadm.hpp
        attributemapdm.hpp
)

