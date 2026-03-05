#include "obj.h"

//nome dos veiculos
const char* GetVehicleName(VehicleType vehicle) {
    switch (vehicle) {
        case VEHICLE_HELICOPTER: return "Helicopter";
        case VEHICLE_JET:        return "Jet F-16";
        case VEHICLE_AIRPLANE:   return "Airplane";
        case VEHICLE_UFO:        return "UFO";
        case VEHICLE_DRONE:      return "Drone";
        case VEHICLE_HAWK:       return "Hawk";
        default:                 return "Airplane";
    }
}

//modelo aviao
void DrawAirplaneModel(float propellerAngle) {
    // Fuselagem traseira afina para -Z (cauda)
    DrawCylinderEx((Vector3){0,0, 1.5f}, (Vector3){0,0, 4.5f},
                   0.65f, 0.2f, 16, (Color){100,100,85,255});
    // Fuselagem central
    DrawCylinderEx((Vector3){0,0,-3.5f}, (Vector3){0,0, 1.5f},
                   0.7f, 0.65f, 16, (Color){120,120,100,255});
    // Capot do motor (na frente, -Z)
    DrawCylinderEx((Vector3){0,0,-3.6f}, (Vector3){0,0,-2.0f},
                   0.72f, 0.75f, 16, (Color){60,55,45,255});
    // Cone do nariz (extremidade -Z)
    DrawCylinderEx((Vector3){0,0,-5.0f}, (Vector3){0,0,-3.5f},
                   0.0f, 0.7f, 16, (Color){40,40,40,255});

    // Asas — centradas no corpo
    // Asa direita
    DrawCube((Vector3){ 2.2f,0.0f,-0.2f}, 3.2f,0.25f,2.5f,(Color){110,110,90,255});
    DrawCube((Vector3){ 4.5f,0.0f, 0.0f}, 2.5f,0.18f,2.0f,(Color){105,105,85,255});
    DrawCube((Vector3){ 6.2f,0.0f, 0.3f}, 1.4f,0.12f,1.3f,(Color){100,100,82,255});
    // Asa esquerda
    DrawCube((Vector3){-2.2f,0.0f,-0.2f},3.2f,0.25f,2.5f,(Color){110,110,90,255});
    DrawCube((Vector3){-4.5f,0.0f, 0.0f},2.5f,0.18f,2.0f,(Color){105,105,85,255});
    DrawCube((Vector3){-6.2f,0.0f, 0.3f},1.4f,0.12f,1.3f,(Color){100,100,82,255});

    // Roundels RAF direito
    DrawCylinder((Vector3){ 3.8f,-0.14f, 0.2f},0.7f,0.7f,0.02f,16,BLUE);
    DrawCylinder((Vector3){ 3.8f,-0.15f, 0.2f},0.45f,0.45f,0.02f,16,WHITE);
    DrawCylinder((Vector3){ 3.8f,-0.16f, 0.2f},0.25f,0.25f,0.02f,16,RED);
    // Roundels RAF esquerdo
    DrawCylinder((Vector3){-3.8f,-0.14f, 0.2f},0.7f,0.7f,0.02f,16,BLUE);
    DrawCylinder((Vector3){-3.8f,-0.15f, 0.2f},0.45f,0.45f,0.02f,16,WHITE);
    DrawCylinder((Vector3){-3.8f,-0.16f, 0.2f},0.25f,0.25f,0.02f,16,RED);

    // Cockpit bolha (entre -Z e centro)
    DrawCube((Vector3){0,0.55f,-1.0f},0.7f,0.5f,1.2f,(Color){100,180,220,180});
    DrawCubeWires((Vector3){0,0.55f,-1.0f},0.72f,0.52f,1.22f,(Color){80,70,55,255});

    // Cauda horizontal (+Z)
    DrawCube((Vector3){0,0.1f, 3.8f},3.6f,0.15f,0.9f,(Color){100,100,82,255});
    // Deriva vertical (+Z)
    DrawCube((Vector3){0,0.7f, 3.6f},0.15f,1.2f,1.1f,(Color){95,95,78,255});
    // Listras
    DrawCube((Vector3){0,0.0f, 3.3f},3.6f,0.12f,0.12f,YELLOW);
    DrawCube((Vector3){0,0.0f, 3.5f},3.6f,0.12f,0.12f,BLUE);

    // Canhoes nas asas (apontam para -Z = frente)
    DrawCylinderEx((Vector3){ 2.5f,-0.05f,-3.8f},(Vector3){ 2.5f,-0.05f,-2.0f},
                   0.08f,0.08f,8,(Color){30,30,30,255});
    DrawCylinderEx((Vector3){ 3.8f,-0.05f,-3.3f},(Vector3){ 3.8f,-0.05f,-1.5f},
                   0.07f,0.07f,8,(Color){30,30,30,255});
    DrawCylinderEx((Vector3){-2.5f,-0.05f,-3.8f},(Vector3){-2.5f,-0.05f,-2.0f},
                   0.08f,0.08f,8,(Color){30,30,30,255});
    DrawCylinderEx((Vector3){-3.8f,-0.05f,-3.3f},(Vector3){-3.8f,-0.05f,-1.5f},
                   0.07f,0.07f,8,(Color){30,30,30,255});

    // Helice de 3 pas (na frente, -Z)
    rlPushMatrix();
        rlTranslatef(0, 0, -5.1f);
        rlRotatef(propellerAngle, 0, 0, 1);
        DrawCube((Vector3){0,0,0},3.8f,0.12f,0.25f,(Color){25,25,25,255});
        rlPushMatrix();
            rlRotatef(120, 0, 0, 1);
            DrawCube((Vector3){0,0,0},3.8f,0.12f,0.25f,(Color){25,25,25,255});
        rlPopMatrix();
        rlPushMatrix();
            rlRotatef(240, 0, 0, 1);
            DrawCube((Vector3){0,0,0},3.8f,0.12f,0.25f,(Color){25,25,25,255});
        rlPopMatrix();
        DrawCube((Vector3){0,0,0},0.4f,0.4f,0.35f,(Color){50,50,50,255});
    rlPopMatrix();
}

//modelo helicoptero
void DrawHelicopterModel(float rotorAngle) {
    // Corpo principal (nariz + fuselagem)
    DrawCylinderEx((Vector3){0,0.10f,-4.8f}, (Vector3){0,0.35f, 2.2f},
                   0.45f, 1.05f, 18, (Color){88,92,96,255});
    DrawCube((Vector3){0,0.75f,-1.6f}, 1.9f,1.1f,3.4f, (Color){84,88,94,255});
    DrawCube((Vector3){0,1.05f,-0.2f}, 1.5f,0.8f,2.5f, (Color){90,96,102,255});

    // Cockpit em vidro angular
    DrawCube((Vector3){0,1.25f,-2.65f}, 1.35f,0.95f,2.25f, (Color){125,175,210,155});
    DrawCubeWires((Vector3){0,1.25f,-2.65f}, 1.37f,0.97f,2.27f, (Color){65,65,70,255});
    DrawCube((Vector3){0,1.00f,-3.40f}, 1.25f,0.6f,1.35f, (Color){120,170,210,150});
    DrawCubeWires((Vector3){0,1.00f,-3.40f}, 1.27f,0.62f,1.37f, (Color){65,65,70,255});

    // Sensor/canhao frontal
    DrawSphere((Vector3){0,-0.20f,-4.95f}, 0.36f, (Color){34,34,34,255});
    DrawCylinderEx((Vector3){0,-0.35f,-4.75f}, (Vector3){0,-0.35f,-5.75f},
                   0.10f, 0.08f, 10, (Color){26,26,26,255});

    // Estabilizadores curtos / asas de ataque
    DrawCube((Vector3){ 2.45f,0.02f,-0.90f}, 3.8f,0.16f,1.2f, (Color){86,90,96,255});
    DrawCube((Vector3){-2.45f,0.02f,-0.90f}, 3.8f,0.16f,1.2f, (Color){86,90,96,255});

    // Pods laterais
    DrawCylinderEx((Vector3){ 3.35f,-0.42f,-0.90f}, (Vector3){ 5.05f,-0.42f,-0.90f},
                   0.24f, 0.20f, 12, (Color){42,44,48,255});
    DrawCylinderEx((Vector3){-3.35f,-0.42f,-0.90f}, (Vector3){-5.05f,-0.42f,-0.90f},
                   0.24f, 0.20f, 12, (Color){42,44,48,255});

    // Boom de cauda
    DrawCylinderEx((Vector3){0,0.55f,2.0f}, (Vector3){0,1.25f,8.2f},
                   0.28f, 0.18f, 14, (Color){78,82,88,255});
    DrawCube((Vector3){0,1.90f,8.0f}, 0.16f,1.6f,1.3f, (Color){80,84,90,255});
    DrawCube((Vector3){ 1.35f,1.00f,6.7f}, 2.55f,0.10f,0.8f, (Color){90,94,100,255});
    DrawCube((Vector3){-1.35f,1.00f,6.7f}, 2.55f,0.10f,0.8f, (Color){90,94,100,255});

    // Skids
    DrawCylinderEx((Vector3){ 0.90f,-1.20f,-2.7f}, (Vector3){ 1.05f,-1.20f, 2.5f},
                   0.08f,0.08f,8,(Color){28,28,32,255});
    DrawCylinderEx((Vector3){-0.90f,-1.20f,-2.7f}, (Vector3){-1.05f,-1.20f, 2.5f},
                   0.08f,0.08f,8,(Color){28,28,32,255});
    DrawCylinderEx((Vector3){ 0.85f,-0.70f,-2.0f}, (Vector3){ 0.95f,-1.20f,-2.0f},
                   0.05f,0.05f,8,(Color){30,30,34,255});
    DrawCylinderEx((Vector3){-0.85f,-0.70f,-2.0f}, (Vector3){-0.95f,-1.20f,-2.0f},
                   0.05f,0.05f,8,(Color){30,30,34,255});
    DrawCylinderEx((Vector3){ 0.95f,-0.70f, 1.8f}, (Vector3){ 1.05f,-1.20f, 1.8f},
                   0.05f,0.05f,8,(Color){30,30,34,255});
    DrawCylinderEx((Vector3){-0.95f,-0.70f, 1.8f}, (Vector3){-1.05f,-1.20f, 1.8f},
                   0.05f,0.05f,8,(Color){30,30,34,255});

    // Torre do rotor principal
    DrawCylinderEx((Vector3){0,1.55f,0.75f}, (Vector3){0,2.18f,0.85f},
                   0.55f,0.50f,16,(Color){68,72,76,255});
    DrawSphere((Vector3){0,2.20f,0.85f}, 0.23f, (Color){34,34,36,255});

    // Rotor principal (4 pás)
    rlPushMatrix();
        rlTranslatef(0, 2.22f, 0.85f);
        rlRotatef(rotorAngle, 0, 1, 0);
        for (int i = 0; i < 4; i++) {
            rlPushMatrix();
                rlRotatef(i * 90.0f, 0, 1, 0);
                DrawCube((Vector3){2.95f,0,0}, 5.9f,0.08f,0.30f, (Color){205,210,210,255});
            rlPopMatrix();
        }
    rlPopMatrix();

    // Rotor traseiro
    rlPushMatrix();
        rlTranslatef(0, 1.35f, 8.45f);
        rlRotatef(90, 0, 1, 0);
        rlRotatef(rotorAngle * 2.7f, 0, 0, 1);
        DrawCube((Vector3){0,0,0},1.7f,0.06f,0.12f,(Color){210,210,210,255});
        rlPushMatrix();
            rlRotatef(90, 0, 0, 1);
            DrawCube((Vector3){0,0,0},1.7f,0.06f,0.12f,(Color){210,210,210,255});
        rlPopMatrix();
    rlPopMatrix();
}

//modelo jato f16
void DrawJetModel(float detailAnimAngle) {
    Color bodyMain  = (Color){ 128, 134, 142, 255 };
    Color bodyDark  = (Color){ 90, 96, 104, 255 };
    Color bodyLight = (Color){ 158, 164, 172, 255 };
    Color glass     = (Color){ 95, 170, 210, 170 };

    // Nariz alongado + fuselagem
    DrawCylinderEx((Vector3){0, 0.0f, -8.5f}, (Vector3){0, 0.0f, -5.8f},
                   0.02f, 0.55f, 20, bodyMain);
    DrawCylinderEx((Vector3){0, 0.0f, -5.8f}, (Vector3){0, 0.0f, -0.8f},
                   0.55f, 0.92f, 22, bodyMain);
    DrawCylinderEx((Vector3){0, 0.0f, -0.8f}, (Vector3){0, 0.0f, 4.2f},
                   0.92f, 0.78f, 22, bodyLight);
    DrawCylinderEx((Vector3){0, 0.0f, 4.2f}, (Vector3){0, 0.2f, 6.6f},
                   0.78f, 0.16f, 16, bodyDark);

    // Intake ventral
    DrawCube((Vector3){0, -0.42f, -2.9f}, 1.05f, 0.42f, 1.55f, (Color){56, 60, 64, 255});
    DrawCubeWires((Vector3){0, -0.42f, -2.9f}, 1.07f, 0.44f, 1.57f, (Color){30, 32, 34, 255});

    // Cockpit bubble + moldura
    DrawCube((Vector3){0, 0.62f, -2.0f}, 0.92f, 0.52f, 2.35f, glass);
    DrawCubeWires((Vector3){0, 0.62f, -2.0f}, 0.96f, 0.56f, 2.39f, (Color){62, 68, 76, 255});
    DrawCube((Vector3){0, 0.27f, -1.35f}, 0.76f, 0.12f, 1.45f, bodyDark);

    // Asas principais (delta/trapezoidais)
    DrawCube((Vector3){ 2.45f, 0.0f, -0.75f}, 3.5f, 0.16f, 2.9f, bodyMain);
    DrawCube((Vector3){ 4.55f, 0.0f,  0.20f}, 2.7f, 0.10f, 2.0f, bodyDark);
    DrawCube((Vector3){-2.45f, 0.0f, -0.75f}, 3.5f, 0.16f, 2.9f, bodyMain);
    DrawCube((Vector3){-4.55f, 0.0f,  0.20f}, 2.7f, 0.10f, 2.0f, bodyDark);

    // LERX (transição da raiz da asa)
    DrawCube((Vector3){ 1.20f, 0.12f, -2.50f}, 1.7f, 0.12f, 1.8f, bodyLight);
    DrawCube((Vector3){-1.20f, 0.12f, -2.50f}, 1.7f, 0.12f, 1.8f, bodyLight);

    // Estabilizadores horizontais traseiros
    DrawCube((Vector3){ 1.85f, 0.14f, 5.3f}, 2.4f, 0.10f, 1.2f, bodyDark);
    DrawCube((Vector3){-1.85f, 0.14f, 5.3f}, 2.4f, 0.10f, 1.2f, bodyDark);

    // Deriva vertical unica
    DrawCube((Vector3){0, 1.08f, 4.9f}, 0.22f, 2.15f, 1.85f, bodyDark);
    DrawCube((Vector3){0, 1.40f, 5.45f}, 0.12f, 1.10f, 0.95f, bodyLight);

    // Trilhos/misseis nas pontas das asas
    DrawCylinderEx((Vector3){ 5.9f, -0.10f, -0.55f}, (Vector3){ 6.9f, -0.10f, -0.55f},
                   0.10f, 0.08f, 10, (Color){178, 182, 188, 255});
    DrawCylinderEx((Vector3){-5.9f, -0.10f, -0.55f}, (Vector3){-6.9f, -0.10f, -0.55f},
                   0.10f, 0.08f, 10, (Color){178, 182, 188, 255});

    // Bocal do motor com animaçao subtil
    float nozzleGlow = 0.25f + fabsf(sinf(detailAnimAngle * DEG2RAD)) * 0.35f;
    DrawCylinderEx((Vector3){0, 0.0f, 6.55f}, (Vector3){0, 0.0f, 7.25f},
                   0.46f, 0.38f, 18, (Color){45, 47, 52, 255});
    DrawCylinderEx((Vector3){0, 0.0f, 7.15f}, (Vector3){0, 0.0f, 7.45f},
                   0.24f, 0.12f, 14, Fade(ORANGE, nozzleGlow));
}

void DrawUfoModel(float animAngle) {
    // corpo do ufo
    // Domo superior (vidro)
    DrawSphere((Vector3){0, 1.8f, 0}, 2.2f, (Color){100, 180, 220, 200});
    
    // Disco principal (metal escuro)
    for (float h = 0.0f; h <= 1.2f; h += 0.3f) {
        float radius = 3.2f - (h * 0.5f);  // Afina ligeiramente
        DrawCylinderEx(
            (Vector3){0, h, 0}, 
            (Vector3){0, h + 0.3f, 0},
            radius, radius - 0.2f, 32,
            (Color){45, 70, 85, 255}
        );
    }
    
    // Luzes circulares no disco
    for (int i = 0; i < 8; i++) {
        float angle = (i / 8.0f) * 6.28f;
        float x = cosf(angle) * 2.5f;
        float z = sinf(angle) * 2.5f;
        DrawSphere((Vector3){x, 0.6f, z}, 0.35f, YELLOW);
    }
    
    // Antena de topo
    DrawCylinderEx((Vector3){0, 2.0f, 0}, (Vector3){0, 3.2f, 0}, 
                   0.15f, 0.08f, 12, (Color){60, 60, 60, 255});
    DrawSphere((Vector3){0, 3.3f, 0}, 0.25f, (Color){255, 200, 100, 255});

    // circulos animados por baixo do UFO
    for (int i = 0; i < 6; i++) {
        float delay = (float)i * 0.35f;
        float angle = fmodf(animAngle - delay, 360.0f);

        if (angle < 180.0f) {
            float progress = angle / 180.0f;
            float size = 0.3f + progress * 4.5f;
            float alpha = 1.0f - (progress * progress);
            float yPos = -1.5f - (progress * 12.0f);

            DrawCircle3D(
                (Vector3){0, yPos, 0},
                size,
                (Vector3){1, 0, 0},
                90.0f,
                Fade(YELLOW, alpha * 0.7f)
            );
        }
    }
}

void DrawDroneModel(float rotorAngle){
    Color bodyLight  = (Color){ 224, 226, 231, 255 };
    Color bodyMid    = (Color){ 188, 192, 200, 255 };
    Color bodyDark   = (Color){ 102, 108, 118, 255 };
    Color armColor   = (Color){ 206, 210, 216, 255 };
    Color rotorColor = (Color){ 235, 237, 239, 255 };
    Color accentRed  = (Color){ 210, 45, 52, 255 };
    Color lensBlue   = (Color){ 115, 172, 220, 255 };
    Color outline    = (Color){ 28, 30, 34, 255 };

    // corpo central
    DrawCube((Vector3){ 0.0f, 0.20f, -0.10f }, 3.20f, 0.72f, 4.30f, bodyLight);
    DrawCubeWires((Vector3){ 0.0f, 0.20f, -0.10f }, 3.24f, 0.76f, 4.34f, outline);
    DrawCube((Vector3){ 0.0f, 0.58f, -0.45f }, 2.05f, 0.32f, 2.55f, bodyMid);
    DrawCubeWires((Vector3){ 0.0f, 0.58f, -0.45f }, 2.09f, 0.36f, 2.59f, outline);
    DrawCube((Vector3){ 0.0f, 0.52f, -2.10f }, 1.18f, 0.18f, 1.10f, bodyDark);
    DrawSphere((Vector3){ 0.0f, 0.42f, -3.25f }, 0.66f, bodyMid);
    DrawSphereWires((Vector3){ 0.0f, 0.42f, -3.25f }, 0.67f, 10, 14, outline);
    DrawSphere((Vector3){ 0.0f, 0.75f, -1.30f }, 0.16f, Fade(WHITE, 0.8f));

    // camera frontal
    DrawSphere((Vector3){ 0.0f, -0.16f, -3.60f }, 0.55f, bodyDark);
    DrawSphereWires((Vector3){ 0.0f, -0.16f, -3.60f }, 0.56f, 10, 12, outline);
    DrawSphere((Vector3){ 0.0f, -0.16f, -3.78f }, 0.34f, (Color){ 230, 230, 230, 255 });
    DrawSphere((Vector3){ 0.0f, -0.16f, -3.92f }, 0.20f, (Color){ 38, 42, 48, 255 });
    DrawSphere((Vector3){ 0.0f, -0.16f, -3.98f }, 0.10f, lensBlue);

    // suportes diagonais dos motores
    float motorX[] = { -3.65f,  3.65f, -3.65f,  3.65f };
    float motorZ[] = { -3.55f, -3.55f,  3.55f,  3.55f };
    float armRotY[] = { 45.0f, -45.0f, -45.0f, 45.0f };

    for (int i = 0; i < 4; i++) {
        float armCenterX = motorX[i] * 0.52f;
        float armCenterZ = motorZ[i] * 0.52f;

        rlPushMatrix();
            rlTranslatef(armCenterX, 0.26f, armCenterZ);
            rlRotatef(armRotY[i], 0.0f, 1.0f, 0.0f);
            DrawCube((Vector3){ 0.0f, 0.0f, 0.0f }, 0.88f, 0.26f, 4.95f, armColor);
            DrawCubeWires((Vector3){ 0.0f, 0.0f, 0.0f }, 0.92f, 0.30f, 4.99f, outline);
        rlPopMatrix();

        // motor
        DrawCylinderEx((Vector3){ motorX[i], 0.05f, motorZ[i] },
                       (Vector3){ motorX[i], 0.94f, motorZ[i] },
                       0.45f, 0.34f, 14, bodyLight);
        DrawCylinderWires((Vector3){ motorX[i], 0.49f, motorZ[i] },
                          0.46f, 0.35f, 0.90f, 14, outline);
        DrawCylinderEx((Vector3){ motorX[i], 0.89f, motorZ[i] },
                       (Vector3){ motorX[i], 1.08f, motorZ[i] },
                       0.24f, 0.21f, 12, bodyDark);

        // perna
        DrawCube((Vector3){ motorX[i], -0.78f, motorZ[i] }, 0.60f, 1.32f, 0.72f, bodyLight);
        DrawCubeWires((Vector3){ motorX[i], -0.78f, motorZ[i] }, 0.64f, 1.36f, 0.76f, outline);
        DrawCube((Vector3){ motorX[i], -1.46f, motorZ[i] }, 0.32f, 0.15f, 0.36f, bodyDark);

        // helice
        rlPushMatrix();
            rlTranslatef(motorX[i], 1.12f, motorZ[i]);
            rlRotatef((i % 2 == 0) ? rotorAngle : -rotorAngle, 0.0f, 1.0f, 0.0f);
            DrawCylinder((Vector3){ 0.0f, 0.0f, 0.0f }, 1.55f, 1.55f, 0.025f, 18, Fade(WHITE, 0.14f));
            DrawCube((Vector3){ 0.0f, 0.0f, 0.0f }, 3.15f, 0.05f, 0.24f, rotorColor);
            DrawCube((Vector3){ 0.0f, 0.0f, 0.0f }, 0.24f, 0.05f, 3.15f, rotorColor);
            DrawCube((Vector3){ 1.48f, 0.0f, 0.0f }, 0.28f, 0.06f, 0.28f, accentRed);
            DrawCube((Vector3){-1.48f, 0.0f, 0.0f }, 0.28f, 0.06f, 0.28f, accentRed);
            DrawCube((Vector3){ 0.0f, 0.0f, 1.48f }, 0.28f, 0.06f, 0.28f, accentRed);
            DrawCube((Vector3){ 0.0f, 0.0f,-1.48f }, 0.28f, 0.06f, 0.28f, accentRed);
            DrawSphere((Vector3){ 0.0f, 0.0f, 0.0f }, 0.16f, outline);
        rlPopMatrix();
    }

    // canhoes frente/tras para o ataque duplo
    DrawCylinderEx((Vector3){ 1.02f, -0.04f, -3.10f }, (Vector3){ 1.02f, -0.04f, -4.15f },
                   0.08f, 0.06f, 8, outline);
    DrawCylinderEx((Vector3){-1.02f, -0.04f, -3.10f }, (Vector3){-1.02f, -0.04f, -4.15f },
                   0.08f, 0.06f, 8, outline);
    DrawCylinderEx((Vector3){ 1.02f, -0.04f,  3.10f }, (Vector3){ 1.02f, -0.04f,  4.15f },
                   0.08f, 0.06f, 8, outline);
    DrawCylinderEx((Vector3){-1.02f, -0.04f,  3.10f }, (Vector3){-1.02f, -0.04f,  4.15f },
                   0.08f, 0.06f, 8, outline);
}
void DrawHawkModel(float spinnerAngle) {

    Color skin      = (Color){  42,  52,  78, 255 };
    Color skinMid   = (Color){  55,  68,  98, 255 };
    Color skinLight = (Color){  72,  88, 120, 255 };
    Color skinDark  = (Color){  22,  28,  45, 255 };
    Color intakeBlk = (Color){   6,   8,  16, 255 };
    Color panelCol  = (Color){ 130, 155, 195, 200 };
    Color exhaustCol= (Color){  18,  22,  38, 255 };
    Color outline   = (Color){  14,  18,  32, 255 };

    float yBase = 0.0f;
    float yTop  = 0.18f;
    float hY    = yTop + 0.32f;
    float pY    = yTop + 0.005f;

    // macro inline para triângulo flat (dois lados visíveis)
    #define FTRI(ax,ay,az, bx,by,bz, cx,cy,cz, col) \
        rlBegin(RL_TRIANGLES); \
            rlColor4ub((col).r,(col).g,(col).b,(col).a); \
            rlVertex3f(ax,ay,az); rlVertex3f(bx,by,bz); rlVertex3f(cx,cy,cz); \
            rlVertex3f(cx,cy,cz); rlVertex3f(bx,by,bz); rlVertex3f(ax,ay,az); \
        rlEnd();

    // asa esquerda exterior
    FTRI(-5.8f,yBase,-1.6f, -13.5f,yBase,0.8f,  -13.5f,yBase,2.8f,  skin)
    FTRI(-5.8f,yBase,-1.6f, -13.5f,yBase,2.8f,   -7.2f,yBase,3.6f,  skin)
    FTRI(-5.8f,yBase,-1.6f,  -7.2f,yBase,3.6f,   -3.8f,yBase,2.4f,  skin)

    // asa direita exterior
    FTRI( 5.8f,yBase,-1.6f,  13.5f,yBase,0.8f,   13.5f,yBase,2.8f,  skin)
    FTRI( 5.8f,yBase,-1.6f,  13.5f,yBase,2.8f,    7.2f,yBase,3.6f,  skin)
    FTRI( 5.8f,yBase,-1.6f,   7.2f,yBase,3.6f,    3.8f,yBase,2.4f,  skin)

    // corpo central — nariz
    FTRI( 0.0f,yTop,-5.2f,  -5.8f,yTop,-1.6f,   5.8f,yTop,-1.6f,   skinMid)

    // corpo central — zona larga
    FTRI(-5.8f,yTop,-1.6f,   5.8f,yTop,-1.6f,   3.8f,yTop, 2.4f,   skinMid)
    FTRI(-5.8f,yTop,-1.6f,   3.8f,yTop, 2.4f,  -3.8f,yTop, 2.4f,   skinMid)

    // bordo de fuga W central
    FTRI(-3.8f,yTop, 2.4f,   3.8f,yTop, 2.4f,   0.0f,yTop, 3.4f,   skinMid)
    FTRI(-3.8f,yTop, 2.4f,   0.0f,yTop, 3.4f,  -1.4f,yTop, 4.2f,   skinMid)
    FTRI( 3.8f,yTop, 2.4f,   0.0f,yTop, 3.4f,   1.4f,yTop, 4.2f,   skinMid)

    // transição corpo → asa esquerda
    FTRI(-5.8f,yTop,-1.6f,  -5.8f,yBase,-1.6f,  -3.8f,yBase,2.4f,  skin)
    FTRI(-5.8f,yTop,-1.6f,  -3.8f,yBase, 2.4f,  -3.8f,yTop, 2.4f,  skin)

    // transição corpo → asa direita
    FTRI( 5.8f,yTop,-1.6f,   5.8f,yBase,-1.6f,   3.8f,yBase,2.4f,  skin)
    FTRI( 5.8f,yTop,-1.6f,   3.8f,yBase, 2.4f,   3.8f,yTop, 2.4f,  skin)

    #undef FTRI

    // dorso elevado
    DrawCube((Vector3){  0.0f, yTop+0.22f,  0.2f }, 4.2f, 0.28f, 3.8f, skinLight);
    DrawCube((Vector3){  0.0f, yTop+0.30f, -0.8f }, 2.8f, 0.18f, 2.2f, skinLight);
    DrawCube((Vector3){  0.0f, yTop+0.14f, -2.9f }, 1.8f, 0.12f, 1.8f, skinLight);

    // cockpit
    DrawCube((Vector3){ 0.0f, yTop+0.24f, -2.6f }, 0.9f, 0.10f, 0.9f, (Color){60,80,115,220});

    // intakes
    DrawCube((Vector3){  1.35f, hY-0.05f,  0.55f }, 1.55f, 0.28f, 1.45f, skinDark);
    DrawCube((Vector3){  1.35f, hY-0.08f,  0.55f }, 1.10f, 0.18f, 1.05f, intakeBlk);
    DrawCube((Vector3){  1.35f, hY+0.06f, -0.08f }, 1.50f, 0.06f, 0.18f, skinDark);
    DrawCube((Vector3){ -1.35f, hY-0.05f,  0.55f }, 1.55f, 0.28f, 1.45f, skinDark);
    DrawCube((Vector3){ -1.35f, hY-0.08f,  0.55f }, 1.10f, 0.18f, 1.05f, intakeBlk);
    DrawCube((Vector3){ -1.35f, hY+0.06f, -0.08f }, 1.50f, 0.06f, 0.18f, skinDark);

    // exaustores com glow animado
    float glow = 0.15f + fabsf(sinf(spinnerAngle * DEG2RAD * 0.8f)) * 0.35f;
    Color exhaustGlow = Fade((Color){255,140,40,255}, glow);
    DrawCube((Vector3){  0.85f, yTop+0.28f, 2.85f }, 0.75f, 0.18f, 0.80f, exhaustCol);
    DrawCube((Vector3){  0.85f, yTop+0.22f, 3.05f }, 0.50f, 0.10f, 0.28f, exhaustGlow);
    DrawCube((Vector3){ -0.85f, yTop+0.28f, 2.85f }, 0.75f, 0.18f, 0.80f, exhaustCol);
    DrawCube((Vector3){ -0.85f, yTop+0.22f, 3.05f }, 0.50f, 0.10f, 0.28f, exhaustGlow);

    // linhas de painel — weapon bay e centro
    DrawCube((Vector3){  0.0f,  pY+0.22f, -1.0f }, 0.04f, 0.025f, 5.5f, panelCol);
    DrawCube((Vector3){  0.95f, pY+0.22f,  0.3f }, 0.04f, 0.025f, 3.2f, panelCol);
    DrawCube((Vector3){ -0.95f, pY+0.22f,  0.3f }, 0.04f, 0.025f, 3.2f, panelCol);
    DrawCube((Vector3){  0.0f,  pY+0.22f, -1.2f }, 1.94f, 0.025f, 0.04f, panelCol);
    DrawCube((Vector3){  0.0f,  pY+0.22f,  1.8f }, 1.94f, 0.025f, 0.04f, panelCol);

    // linhas de painel — contorno dos intakes
    DrawCube((Vector3){  1.35f, pY+0.42f,  0.55f }, 1.80f, 0.025f, 0.04f, panelCol);
    DrawCube((Vector3){ -1.35f, pY+0.42f,  0.55f }, 1.80f, 0.025f, 0.04f, panelCol);
    DrawCube((Vector3){  1.35f, pY+0.42f, -0.15f }, 0.04f, 0.025f, 1.40f, panelCol);
    DrawCube((Vector3){  1.35f, pY+0.42f,  1.25f }, 0.04f, 0.025f, 1.40f, panelCol);
    DrawCube((Vector3){ -1.35f, pY+0.42f, -0.15f }, 0.04f, 0.025f, 1.40f, panelCol);
    DrawCube((Vector3){ -1.35f, pY+0.42f,  1.25f }, 0.04f, 0.025f, 1.40f, panelCol);

    // linhas de painel — asas
    rlPushMatrix(); rlTranslatef( 5.5f,pY,-0.3f); rlRotatef(-37.0f,0,1,0);
        DrawCube((Vector3){0,0,0}, 4.8f, 0.025f, 0.04f, panelCol); rlPopMatrix();
    rlPushMatrix(); rlTranslatef( 9.8f,pY, 1.2f); rlRotatef(-20.0f,0,1,0);
        DrawCube((Vector3){0,0,0}, 4.2f, 0.025f, 0.04f, panelCol); rlPopMatrix();
    rlPushMatrix(); rlTranslatef(-5.5f,pY,-0.3f); rlRotatef( 37.0f,0,1,0);
        DrawCube((Vector3){0,0,0}, 4.8f, 0.025f, 0.04f, panelCol); rlPopMatrix();
    rlPushMatrix(); rlTranslatef(-9.8f,pY, 1.2f); rlRotatef( 20.0f,0,1,0);
        DrawCube((Vector3){0,0,0}, 4.2f, 0.025f, 0.04f, panelCol); rlPopMatrix();

    // bordos de ataque
    rlPushMatrix(); rlTranslatef( 3.8f,0,-2.0f); rlRotatef(-53.0f,0,1,0);
        DrawCube((Vector3){0,0.12f,0}, 7.5f, 0.08f, 0.18f, skinDark); rlPopMatrix();
    rlPushMatrix(); rlTranslatef( 9.6f,0, 0.4f); rlRotatef(-22.0f,0,1,0);
        DrawCube((Vector3){0,0.08f,0}, 5.5f, 0.06f, 0.14f, skinDark); rlPopMatrix();
    rlPushMatrix(); rlTranslatef(-3.8f,0,-2.0f); rlRotatef( 53.0f,0,1,0);
        DrawCube((Vector3){0,0.12f,0}, 7.5f, 0.08f, 0.18f, skinDark); rlPopMatrix();
    rlPushMatrix(); rlTranslatef(-9.6f,0, 0.4f); rlRotatef( 22.0f,0,1,0);
        DrawCube((Vector3){0,0.08f,0}, 5.5f, 0.06f, 0.14f, skinDark); rlPopMatrix();

    // trem de aterragem
    DrawCube((Vector3){ 0.0f, -0.35f, -1.2f }, 0.18f, 0.70f, 0.18f, outline);
    DrawCube((Vector3){ 0.0f, -0.72f, -1.2f }, 0.55f, 0.08f, 0.22f, outline);

    // sombra ventral
    DrawCube((Vector3){ 0.0f, -0.06f, -0.5f }, 6.5f, 0.06f, 6.5f, (Color){18,22,35,180});
}


//desenha veiculo ativo
void DrawVehicleModel(VehicleType vehicle, float spinnerAngle) {
    switch (vehicle) {
        case VEHICLE_HELICOPTER: DrawHelicopterModel(spinnerAngle); break;
        case VEHICLE_JET:        DrawJetModel(spinnerAngle); break;
        case VEHICLE_AIRPLANE:   DrawAirplaneModel(spinnerAngle); break;
        case VEHICLE_UFO:        DrawUfoModel(spinnerAngle); break;
        case VEHICLE_DRONE:      DrawDroneModel(spinnerAngle); break;
        case VEHICLE_HAWK:       DrawHawkModel(spinnerAngle); break;
        default:                 DrawAirplaneModel(spinnerAngle); break;
    }
}
