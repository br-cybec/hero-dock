#pragma once
#include <QWidget>
#include <QList>
#include <QSettings>
#include <QTimer>
#include <QPropertyAnimation>
#include "DockConfig.h"
#include "ApplicationScanner.h"
#include "DeviceManager.h"

class DockItem;
class AppMenu;
class SystemPanel;
class DeviceManager;
class DockDeviceItem;
class NotificationManager;
class NetworkPanel;

class DockWindow : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int posOffset READ posOffset WRITE setPosOffset)

public:
    explicit DockWindow(QWidget* parent = nullptr);
    ~DockWindow();

    const DockConfig& config() const { return m_cfg; }
    void aplicarConfig(const DockConfig& cfg);

    void anclarApp(const QString& desktopFile);
    void desanclarApp(const QString& desktopFile);
    void reconstruirDock();

    int  posOffset() const { return m_posOffset; }
    void setPosOffset(int v);

    // Para que los ítems especiales accedan al panel
    SystemPanel* systemPanel() const { return m_sysPanel; }
    AppMenu*     appMenu()     const { return m_appMenu; }

public slots:
    void abrirPreferencias();

protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void showEvent(QShowEvent*) override;

private:
    void cargarConfig();
    void guardarConfig();
    void cargarAppsAncladas();
    void guardarAppsAncladas();
    void configurarWM();
    void establecerEstrut();
    void limpiarEstrut();

    // Layout: [BotonApps] [sep] [pinned+running apps] [sep] [devices] [sep] [Papelera] [Reloj] [BotonMenu]
    void layoutItems();
    void posicionarDock();
    void agregarItemsEspeciales();

    void actualizarVentanas();
    void actualizarMagnificacion(const QPoint& mouse);
    void resetMagnificacion();
    void mostrarDock();
    void ocultarDock();

    void menuContextualItem(DockItem* item, const QPoint& pos);
    void menuContextualDock(const QPoint& pos);
    void alHacerClicItem(DockItem* item);
    void alHacerClicDerechoItem(DockItem* item, const QPoint& pos);
    void conectarItem(DockItem* item);

    void onDispositivoAgregado(const DispositivoInfo& info);
    void onDispositivoQuitado(const QString& id);
    void menuContextualDispositivo(DockDeviceItem* item, const QPoint& pos);

    QRect    geometriaDock() const;
    QScreen* pantallaObjetivo() const;

    // Data
    DockConfig            m_cfg;
    QList<DockItem*>      m_items;    // Apps ancladas / ejecutando
    QList<DockItem*>      m_extras;   // Ítems especiales del dock
    QList<DockDeviceItem*> m_deviceItems;
    ApplicationScanner*   m_scanner;
    QSettings*            m_settings;

    // Paneles flotantes
    AppMenu*              m_appMenu   = nullptr;
    SystemPanel*          m_sysPanel  = nullptr;
    NetworkPanel*         m_netPanel  = nullptr;
    NotificationManager*  m_notifMgr  = nullptr;
    DeviceManager*        m_devMgr    = nullptr;

    // Auto-hide
    QTimer*               m_timerWM;
    QTimer*               m_timerAutoOcultar;
    QPropertyAnimation*   m_animSlide;
    bool                  m_oculto       = false;
    bool                  m_mouseAdentro = false;
    int                   m_posOffset    = 0;

    QTimer*               m_timerResetMag;
    QTimer*               m_timerPantalla;
};
