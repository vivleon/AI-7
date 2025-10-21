cmake_minimum_required(VERSION 3.16)
project(VigilEdge VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Build options ────────────────────────────────────────────────────────────
option(ENABLE_AI_DIALOGS "Build with AI dialogs" ON)
option(WITH_ARGON2 "Use Argon2id for password hashing (fallback to PBKDF2 otherwise)" OFF)

find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Widgets Sql Network
    Qml Quick QuickControls2 Charts
    Positioning Location
    QuickWidgets
    PrintSupport
)

find_package(OpenSSL REQUIRED) # PBKDF2는 Qt로 구현하지만 OpenSSL 의존은 유지

# ── Sources ──────────────────────────────────────────────────────────────────
set(PROJECT_SOURCES
    main.cpp

    # UI / Dash
    MainDashboard.cpp
    MainDashboard.h
    MainDashboard.ui

    # 로그인 / 유저
    LoginDialog.cpp
    LoginDialog.h
    LoginDialog.ui
    UserMgmtDialog.cpp
    UserMgmtDialog.h

    # DB & Models
    DBBridge.cpp
    DBBridge.h
    SensorData.h
    LogTableModel.h
    LogTableModel.cpp
    CustomFilterProxyModel.h
    CustomFilterProxyModel.cpp
    Role.h
    Thresholds.h

    # 지도(QML)
    MapDialog.cpp
    MapDialog.h
    MapDialog.ui
    qml.qrc

    # 스레드/분석
    SensorManager.cpp
    SensorManager.h
    ThreadWorker.cpp
    ThreadWorker.h
    PatternAnalyzer.cpp
    PatternAnalyzer.h

    # Alert Rules
    AlertRulesDialog.cpp
    AlertRulesDialog.h

    # Language
    LanguageManager.h
    LanguageManager.cpp

    # 암호 유틸
    CryptoUtils.h
    CryptoUtils.cpp

    # 감사 로그
    AuditLogDialog.cpp
    AuditLogDialog.h

    # DB 커넥션 풀
    ConnectionPool.h
    ConnectionPool.cpp

    # ── 신규: 세션/중복로그인 방지 ────────────────────────────────────────────
    SessionService.h
    SessionService.cpp

    # ── 신규: 현재 접속중 사용자 팝업 ────────────────────────────────────────
    CurrentSessionsDialog.h
    CurrentSessionsDialog.cpp

    AlertInsightsWidget.h
    AlertInsightsWidget.cpp

    AIDialogs.cpp
    AIDialogs.h

    LocationPopupDialog.h
    LocationPopupDialog.cpp

)


qt_add_executable(VigilEdge
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
    LocationPopupDialog.h LocationPopupDialog.cpp
)

# 전역 매크로(위의 target_compile_definitions가 target 생성 전일 수 있어 여기서 보강)
if (ENABLE_AI_DIALOGS)
    target_compile_definitions(VigilEdge PRIVATE HAVE_AI_DIALOGS=1)
endif()

# Argon2(선택)
if (WITH_ARGON2)
    find_path(ARGON2_INCLUDE_DIR argon2.h)
    find_library(ARGON2_LIB argon2)
    if (ARGON2_INCLUDE_DIR AND ARGON2_LIB)
        target_compile_definitions(VigilEdge PRIVATE HAVE_ARGON2=1)
        target_include_directories(VigilEdge PRIVATE ${ARGON2_INCLUDE_DIR})
        target_link_libraries(VigilEdge PRIVATE ${ARGON2_LIB})
        message(STATUS "Argon2 enabled")
    else()
        message(FATAL_ERROR "WITH_ARGON2=ON but argon2 library not found")
    endif()
endif()

target_link_libraries(VigilEdge PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Charts
    Qt6::Positioning
    Qt6::Location
    Qt6::QuickWidgets
    Qt6::PrintSupport
    OpenSSL::SSL
    OpenSSL::Crypto
)

set_target_properties(VigilEdge PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

qt_finalize_executable(VigilEdge)
