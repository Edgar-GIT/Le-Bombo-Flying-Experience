#include "../include/config.hpp"

const PrimitiveDef kPrimitiveDefs[] = {
    { PrimitiveKind::Cube, "Cube", false, "3D cube primitive" },
    { PrimitiveKind::Sphere, "Sphere", false, "3D sphere primitive" },
    { PrimitiveKind::Cylinder, "Cylinder", false, "3D cylinder primitive" },
    { PrimitiveKind::Cone, "Cone", false, "3D cone primitive" },
    { PrimitiveKind::Plane, "Plane", false, "3D plane primitive" },
    { PrimitiveKind::Rectangle2D, "Rectangle", true, "2D rectangle primitive" },
    { PrimitiveKind::Circle2D, "Circle", true, "2D circle primitive" },
    { PrimitiveKind::Triangle2D, "Triangle", true, "2D triangle primitive" },
    { PrimitiveKind::Ring2D, "Ring", true, "2D ring primitive" },
    { PrimitiveKind::Poly2D, "Polygon", true, "2D polygon primitive" },
    { PrimitiveKind::Line2D, "Line", true, "2D line primitive" },
};

const int kPrimitiveDefCount = (int)(sizeof(kPrimitiveDefs) / sizeof(kPrimitiveDefs[0]));

const EngineUserConfig kUserConfig = {
    .uiAnimationSpeed = 10.0f,

    .cameraOrbitSensitivity = 0.008f,
    .cameraPanSensitivity = 0.006f,
    .cameraZoomSensitivity = 0.06f,

    .gizmoMoveSensitivity = 1.0f,
    .gizmoRotateSensitivity = 28.0f,
    .gizmoScaleSensitivity = 0.30f,

    .previewMoveSpeed = 12.0f,
    .previewLookSensitivity = 0.0045f,
    .previewZoomSpeed = 4.5f,
    .previewPanSensitivity = 2.2f,

    .maxLogEntries = 240,
    .trimLogEntries = 40,
};
