#pragma once
#include <QString>
#include <QColor>

enum class DockPosition       { Abajo, Arriba, Izquierda, Derecha };
enum class DockComportamiento { SiempreVisible, AutoOcultar };

struct DockConfig {
    DockPosition        posicion           = DockPosition::Abajo;
    DockComportamiento  comportamiento     = DockComportamiento::SiempreVisible;
    int                 tamanoIcono        = 52;
    bool                magnificacion      = true;
    int                 multiplicadorMag   = 165;
    bool                fondoTranslucido   = true;
    double              opacidadFondo      = 0.82;
    bool                mostrarIndicadores = true;
    bool                mostrarEtiquetas   = true;
    bool                rebotarAlAbrir     = true;
    int                 indiceMonitor      = 0;
    int                 radioEsquina       = 20;
    int                 espaciadoItems     = 5;
    int                 padding            = 10;
    int                 margen             = 6;

    // Dock layout: solo estos elementos en el dock
    // [BotonMenu][sep][Apps ancladas + ejecutando][sep][Discos externos][sep][Papelera][Reloj]
    // Todo lo demás (red, batería, notif, volumen, apagado) va en el SystemPanel
    bool mostrarBotonMenu   = true;   // ← abre SystemPanel
    bool mostrarPapelera    = true;
    bool mostrarReloj       = true;
    bool mostrarBotonApps   = true;   // ← abre AppMenu (grid de apps)
};
