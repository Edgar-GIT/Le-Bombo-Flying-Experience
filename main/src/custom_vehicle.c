#include "custom_vehicle.h"

#include <stdio.h>
#include <string.h>

// max exported custom objects supported at runtime
#define MAX_CUSTOM_OBJECTS 512
// max exported shotpoints supported at runtime
#define MAX_CUSTOM_SHOTPOINTS 256

typedef enum {
    PRIMITIVE_CUBE = 0,
    PRIMITIVE_SPHERE,
    PRIMITIVE_CYLINDER,
    PRIMITIVE_CONE,
    PRIMITIVE_PLANE,
    PRIMITIVE_RECT2D,
    PRIMITIVE_CIRCLE2D,
    PRIMITIVE_TRIANGLE2D,
    PRIMITIVE_RING2D,
    PRIMITIVE_POLY2D,
    PRIMITIVE_LINE2D
} CustomPrimitiveKind;

typedef struct {
    int kind;
    bool visible;
    bool is2D;
    Vector3 position;
    Vector3 rotation;
    Vector3 scale;
    Color color;
} CustomObject;

typedef struct {
    int objectIndex;
    bool enabled;
    Vector3 localPos;
    Color blastColor;
    float blastSize;
} CustomShotpoint;

typedef struct {
    bool loaded;
    char name[64];
    float forwardYawDeg;
    int objectCount;
    int shotpointCount;
    CustomObject objects[MAX_CUSTOM_OBJECTS];
    CustomShotpoint shotpoints[MAX_CUSTOM_SHOTPOINTS];
} CustomVehicleData;

static CustomVehicleData gCustom = { 0 };

// reset runtime custom vehicle cache
void CustomVehicleReset(void) {
    memset(&gCustom, 0, sizeof(gCustom));
    strcpy(gCustom.name, "Custom Vehicle");
    gCustom.forwardYawDeg = 0.0f;
}

// build matrix in translate rotate scale order
static Matrix BuildObjectMatrix(const CustomObject* obj) {
    Matrix mat = MatrixIdentity();
    mat = MatrixMultiply(mat, MatrixTranslate(obj->position.x, obj->position.y, obj->position.z));
    mat = MatrixMultiply(mat, MatrixRotateX(obj->rotation.x * DEG2RAD));
    mat = MatrixMultiply(mat, MatrixRotateY(obj->rotation.y * DEG2RAD));
    mat = MatrixMultiply(mat, MatrixRotateZ(obj->rotation.z * DEG2RAD));
    mat = MatrixMultiply(mat, MatrixScale(obj->scale.x, obj->scale.y, obj->scale.z));
    return mat;
}

// build rotation only matrix for direction vectors
static Matrix BuildObjectRotationMatrix(const CustomObject* obj) {
    Matrix mat = MatrixIdentity();
    mat = MatrixMultiply(mat, MatrixRotateX(obj->rotation.x * DEG2RAD));
    mat = MatrixMultiply(mat, MatrixRotateY(obj->rotation.y * DEG2RAD));
    mat = MatrixMultiply(mat, MatrixRotateZ(obj->rotation.z * DEG2RAD));
    return mat;
}

// parse one exported object line
static bool ParseObjectLine(const char* line, CustomObject* outObj) {
    int kind = 0;
    int visible = 1;
    int is2D = 0;
    float px = 0.0f, py = 0.0f, pz = 0.0f;
    float rx = 0.0f, ry = 0.0f, rz = 0.0f;
    float sx = 1.0f, sy = 1.0f, sz = 1.0f;
    int cr = 255, cg = 255, cb = 255, ca = 255;

    int read = sscanf(line,
                      "obj=%d,%d,%d,%f,%f,%f,%f,%f,%f,%f,%f,%f,%d,%d,%d,%d",
                      &kind, &visible, &is2D,
                      &px, &py, &pz,
                      &rx, &ry, &rz,
                      &sx, &sy, &sz,
                      &cr, &cg, &cb, &ca);
    if (read != 16) return false;

    outObj->kind = kind;
    outObj->visible = (visible != 0);
    outObj->is2D = (is2D != 0);
    outObj->position = (Vector3){ px, py, pz };
    outObj->rotation = (Vector3){ rx, ry, rz };
    outObj->scale = (Vector3){ sx, sy, sz };
    outObj->color = (Color){
        (unsigned char)(cr < 0 ? 0 : (cr > 255 ? 255 : cr)),
        (unsigned char)(cg < 0 ? 0 : (cg > 255 ? 255 : cg)),
        (unsigned char)(cb < 0 ? 0 : (cb > 255 ? 255 : cb)),
        (unsigned char)(ca < 0 ? 0 : (ca > 255 ? 255 : ca))
    };
    return true;
}

// parse one exported shotpoint line
static bool ParseShotpointLine(const char* line, CustomShotpoint* outSp) {
    int objectIndex = -1;
    int enabled = 1;
    float lx = 0.0f, ly = 0.0f, lz = 0.0f;
    int cr = 240, cg = 90, cb = 90, ca = 255;
    float size = 0.16f;

    int read = sscanf(line,
                      "sp=%d,%d,%f,%f,%f,%d,%d,%d,%d,%f",
                      &objectIndex, &enabled,
                      &lx, &ly, &lz,
                      &cr, &cg, &cb, &ca,
                      &size);
    if (read != 10) return false;

    outSp->objectIndex = objectIndex;
    outSp->enabled = (enabled != 0);
    outSp->localPos = (Vector3){ lx, ly, lz };
    outSp->blastColor = (Color){
        (unsigned char)(cr < 0 ? 0 : (cr > 255 ? 255 : cr)),
        (unsigned char)(cg < 0 ? 0 : (cg > 255 ? 255 : cg)),
        (unsigned char)(cb < 0 ? 0 : (cb > 255 ? 255 : cb)),
        (unsigned char)(ca < 0 ? 0 : (ca > 255 ? 255 : ca))
    };
    if (size < 0.05f) size = 0.05f;
    if (size > 2.0f) size = 2.0f;
    outSp->blastSize = size;
    return true;
}

// parse exported .vehicle file
static bool LoadVehicleFile(const char* filePath) {
    FILE* fp = fopen(filePath, "r");
    if (fp == NULL) return false;

    CustomVehicleReset();
    char line[1024];
    bool headerOk = false;

    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        if (strncmp(line, "LBFE_VEHICLE_V1", 15) == 0) {
            headerOk = true;
        } else if (strncmp(line, "vehicle_name=", 13) == 0) {
            char* start = line + 13;
            size_t len = strlen(start);
            while (len > 0 && (start[len - 1] == '\n' || start[len - 1] == '\r')) {
                start[len - 1] = '\0';
                len--;
            }
            if (len > 0) {
                strncpy(gCustom.name, start, sizeof(gCustom.name) - 1);
                gCustom.name[sizeof(gCustom.name) - 1] = '\0';
            }
        } else if (strncmp(line, "forward_yaw_deg=", 16) == 0) {
            float yaw = 0.0f;
            if (sscanf(line + 16, "%f", &yaw) == 1) {
                if (yaw < -180.0f) yaw = -180.0f;
                if (yaw > 180.0f) yaw = 180.0f;
                gCustom.forwardYawDeg = yaw;
            }
        } else if (strncmp(line, "obj=", 4) == 0) {
            if (gCustom.objectCount < MAX_CUSTOM_OBJECTS) {
                CustomObject obj = { 0 };
                if (ParseObjectLine(line, &obj)) {
                    gCustom.objects[gCustom.objectCount++] = obj;
                }
            }
        } else if (strncmp(line, "sp=", 3) == 0) {
            if (gCustom.shotpointCount < MAX_CUSTOM_SHOTPOINTS) {
                CustomShotpoint sp = { 0 };
                if (ParseShotpointLine(line, &sp)) {
                    gCustom.shotpoints[gCustom.shotpointCount++] = sp;
                }
            }
        } else if (strncmp(line, "end", 3) == 0) {
            break;
        }
    }

    fclose(fp);

    if (!headerOk || gCustom.objectCount <= 0) {
        CustomVehicleReset();
        return false;
    }

    for (int i = 0; i < gCustom.shotpointCount; i++) {
        if (gCustom.shotpoints[i].objectIndex < 0 || gCustom.shotpoints[i].objectIndex >= gCustom.objectCount) {
            gCustom.shotpoints[i].enabled = false;
        }
    }

    gCustom.loaded = true;
    return true;
}

// load newest .vehicle file from builds folder
bool CustomVehicleLoadFromBuilds(const char* buildsPath) {
    CustomVehicleReset();
    if (buildsPath == NULL || buildsPath[0] == '\0') return false;

    FilePathList files = LoadDirectoryFilesEx(buildsPath, ".vehicle", true);
    if (files.count <= 0 || files.paths == NULL) {
        UnloadDirectoryFiles(files);
        return false;
    }

    int bestIndex = -1;
    long bestTime = -1;
    for (unsigned int i = 0; i < files.count; i++) {
        long modTime = GetFileModTime(files.paths[i]);
        if (bestIndex < 0 || modTime > bestTime) {
            bestIndex = (int)i;
            bestTime = modTime;
        }
    }

    bool ok = false;
    if (bestIndex >= 0) {
        ok = LoadVehicleFile(files.paths[bestIndex]);
    }

    UnloadDirectoryFiles(files);
    return ok;
}

// return load state flag for custom vehicle slot
bool CustomVehicleIsLoaded(void) {
    return gCustom.loaded;
}

// return display name for custom vehicle
const char* CustomVehicleName(void) {
    return gCustom.name;
}

// draw one object primitive exported by game engine
static void DrawCustomPrimitive(const CustomObject* obj) {
    Color wire = ColorAlpha(BLACK, 0.72f);
    switch (obj->kind) {
        case PRIMITIVE_CUBE:
            DrawCubeV((Vector3){ 0, 0, 0 }, (Vector3){ 1, 1, 1 }, obj->color);
            DrawCubeWiresV((Vector3){ 0, 0, 0 }, (Vector3){ 1, 1, 1 }, wire);
            break;
        case PRIMITIVE_SPHERE:
            DrawSphere((Vector3){ 0, 0, 0 }, 0.58f, obj->color);
            DrawSphereWires((Vector3){ 0, 0, 0 }, 0.58f, 14, 12, wire);
            break;
        case PRIMITIVE_CYLINDER:
            DrawCylinder((Vector3){ 0, 0, 0 }, 0.45f, 0.45f, 1.2f, 24, obj->color);
            DrawCylinderWires((Vector3){ 0, 0, 0 }, 0.45f, 0.45f, 1.2f, 24, wire);
            break;
        case PRIMITIVE_CONE:
            DrawCylinder((Vector3){ 0, 0, 0 }, 0.0f, 0.58f, 1.2f, 24, obj->color);
            DrawCylinderWires((Vector3){ 0, 0, 0 }, 0.0f, 0.58f, 1.2f, 24, wire);
            break;
        case PRIMITIVE_PLANE:
            DrawPlane((Vector3){ 0, 0, 0 }, (Vector2){ 1.6f, 1.6f }, obj->color);
            break;
        case PRIMITIVE_RECT2D:
            DrawCubeV((Vector3){ 0, 0, 0 }, (Vector3){ 1.25f, 0.04f, 0.9f }, obj->color);
            DrawCubeWiresV((Vector3){ 0, 0, 0 }, (Vector3){ 1.25f, 0.04f, 0.9f }, wire);
            break;
        case PRIMITIVE_CIRCLE2D:
            DrawCylinder((Vector3){ 0, 0, 0 }, 0.55f, 0.55f, 0.04f, 32, obj->color);
            DrawCylinderWires((Vector3){ 0, 0, 0 }, 0.55f, 0.55f, 0.04f, 32, wire);
            break;
        case PRIMITIVE_TRIANGLE2D:
            DrawTriangle3D((Vector3){ -0.6f, 0.02f, 0.45f },
                           (Vector3){ 0.65f, 0.02f, 0.45f },
                           (Vector3){ 0.0f, 0.02f, -0.55f }, obj->color);
            break;
        case PRIMITIVE_RING2D:
            DrawCircle3D((Vector3){ 0, 0.04f, 0 }, 0.68f, (Vector3){ 1, 0, 0 }, 90.0f, obj->color);
            break;
        case PRIMITIVE_POLY2D: {
            const int sides = 6;
            const float r = 0.64f;
            for (int i = 0; i < sides; i++) {
                float a0 = (float)i / (float)sides * 2.0f * PI;
                float a1 = (float)(i + 1) / (float)sides * 2.0f * PI;
                DrawLine3D((Vector3){ cosf(a0) * r, 0.03f, sinf(a0) * r },
                           (Vector3){ cosf(a1) * r, 0.03f, sinf(a1) * r },
                           obj->color);
            }
            break;
        }
        case PRIMITIVE_LINE2D:
            DrawLine3D((Vector3){ -0.65f, 0.03f, 0.0f }, (Vector3){ 0.65f, 0.03f, 0.0f }, obj->color);
            break;
        default:
            DrawCube((Vector3){ 0, 0, 0 }, 1.0f, 1.0f, 1.0f, obj->color);
            DrawCubeWires((Vector3){ 0, 0, 0 }, 1.0f, 1.0f, 1.0f, wire);
            break;
    }
}

// draw custom vehicle model from exported objects
void DrawCustomVehicleModel(float animAngle) {
    (void)animAngle;
    if (!gCustom.loaded || gCustom.objectCount <= 0) {
        DrawCube((Vector3){ 0.0f, 0.0f, 0.0f }, 2.6f, 0.7f, 4.8f, (Color){ 120, 120, 120, 255 });
        DrawCubeWires((Vector3){ 0.0f, 0.0f, 0.0f }, 2.6f, 0.7f, 4.8f, BLACK);
        return;
    }

    rlPushMatrix();
    rlRotatef(gCustom.forwardYawDeg, 0, 1, 0);
    for (int i = 0; i < gCustom.objectCount; i++) {
        const CustomObject* obj = &gCustom.objects[i];
        if (!obj->visible) continue;

        rlPushMatrix();
        rlTranslatef(obj->position.x, obj->position.y, obj->position.z);
        rlRotatef(obj->rotation.x, 1, 0, 0);
        rlRotatef(obj->rotation.y, 0, 1, 0);
        rlRotatef(obj->rotation.z, 0, 0, 1);
        rlScalef(obj->scale.x, obj->scale.y, obj->scale.z);
        DrawCustomPrimitive(obj);
        rlPopMatrix();
    }
    rlPopMatrix();
}

// return number of configured custom shotpoints
int CustomVehicleShotpointCount(void) {
    return gCustom.shotpointCount;
}

// resolve shotpoint world transform for gameplay lasers
bool CustomVehicleGetShotpointWorld(int shotpointIndex,
                                    Vector3 vehiclePos,
                                    Matrix vehicleRotation,
                                    Vector3* outWorldPos,
                                    Vector3* outWorldDir,
                                    Color* outColor,
                                    float* outRadius) {
    if (!gCustom.loaded) return false;
    if (shotpointIndex < 0 || shotpointIndex >= gCustom.shotpointCount) return false;

    const CustomShotpoint* sp = &gCustom.shotpoints[shotpointIndex];
    if (!sp->enabled) return false;
    if (sp->objectIndex < 0 || sp->objectIndex >= gCustom.objectCount) return false;

    const CustomObject* obj = &gCustom.objects[sp->objectIndex];
    Matrix objMat = BuildObjectMatrix(obj);
    Matrix objRotMat = BuildObjectRotationMatrix(obj);

    Matrix forwardRot = MatrixRotateY(gCustom.forwardYawDeg * DEG2RAD);
    Vector3 modelPos = Vector3Transform(sp->localPos, objMat);
    modelPos = Vector3Transform(modelPos, forwardRot);
    Vector3 modelDir = Vector3Transform((Vector3){ 0.0f, 0.0f, -1.0f }, objRotMat);
    modelDir = Vector3Transform(modelDir, forwardRot);
    if (Vector3LengthSqr(modelDir) < 0.0001f) modelDir = (Vector3){ 0.0f, 0.0f, -1.0f };
    modelDir = Vector3Normalize(modelDir);

    if (outWorldPos) {
        *outWorldPos = Vector3Add(vehiclePos, Vector3Transform(modelPos, vehicleRotation));
    }
    if (outWorldDir) {
        Vector3 worldDir = Vector3Transform(modelDir, vehicleRotation);
        if (Vector3LengthSqr(worldDir) < 0.0001f) worldDir = (Vector3){ 0.0f, 0.0f, -1.0f };
        *outWorldDir = Vector3Normalize(worldDir);
    }
    if (outColor) *outColor = sp->blastColor;
    if (outRadius) *outRadius = sp->blastSize;
    return true;
}
