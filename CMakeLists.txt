# SPDX-FileCopyrightText: 2024 Petros Koutsolampros
#
# SPDX-License-Identifier: GPL-3.0-or-later

target_sources(${dxproject}
    PRIVATE
        metagraphdx.cpp
        pointmapdx.cpp
        shapemapdx.cpp
        shapegraphdx.cpp
    PUBLIC
        metagraphdx.h
        pointmapdx.h
        shapemapdx.h
        shapegraphdx.h
        shapemapgroupdatadx.h
        attributemapdx.h
)

