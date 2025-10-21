#ifndef ROLE_H
#define ROLE_H
#include <QString>

enum class Role : int {
    Viewer      = 0,
    Operator    = 1,
    Analyst     = 2,
    Admin       = 3,
    SuperAdmin  = 4
};

inline QString roleToString(Role r) {
    switch (r) {
    case Role::Viewer:     return QStringLiteral("Viewer");
    case Role::Operator:   return QStringLiteral("Operator");
    case Role::Analyst:    return QStringLiteral("Analyst");
    case Role::Admin:      return QStringLiteral("Admin");
    case Role::SuperAdmin: return QStringLiteral("SuperAdmin");
    }
    return QStringLiteral("Unknown");
}

inline Role roleFromInt(int v){
    if (v < 0) v = 0; if (v > 4) v = 4;
    return static_cast<Role>(v);
}

#endif
