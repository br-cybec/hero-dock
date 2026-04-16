#pragma once
#include <QWidget>
#include <QPropertyAnimation>
#include <QTimer>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QHBoxLayout>

// ─── Panel flotante del sistema ──────────────────────────────────────────
// Aparece al hacer clic en el botón de menú del dock.
// Contiene: notificaciones, red, batería, volumen, preferencias, apagado.

class DockWindow;
class NotificationManager;
class NetworkPanel;

class SystemPanel : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal opacidad READ opacidad WRITE setOpacidad)
    Q_PROPERTY(int   slideY   READ slideY   WRITE setSlideY)

public:
    explicit SystemPanel(DockWindow* dock, QWidget* parent = nullptr);

    void toggle(const QPoint& anchorGlobal);
    void mostrar(const QPoint& anchorGlobal);
    void ocultar();
    bool estaVisible() const { return m_visible; }

    void setNotifManager(NotificationManager* mgr) { m_notifMgr = mgr; }
    void setNetworkPanel(NetworkPanel* net)          { m_netPanel = net; }

    // Actualizar datos en tiempo real
    void actualizarBateria(int nivel, bool cargando);
    void actualizarRed(bool conectada, const QString& nombre);
    void actualizarVolumen(int nivel);
    void actualizarNotificaciones(int n);

    qreal opacidad() const { return m_opacidad; }
    void  setOpacidad(qreal v) { m_opacidad = v; setWindowOpacity(v); }
    int   slideY() const { return m_slideY; }
    void  setSlideY(int v);

signals:
    void pedirPreferencias();

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    bool eventFilter(QObject* o, QEvent* e) override;

private:
    void construirUI();

    // Secciones del panel
    QWidget* crearSeccionEstado();
    QWidget* crearSeccionVolumen();
    QWidget* crearSeccionAcciones();
    QWidget* crearSeccionApagado();

    // Botón redondeado de control
    QPushButton* crearBotonControl(const QString& icono,
                                   const QString& texto,
                                   const QColor& color,
                                   std::function<void()> accion);

    void reposicionar(const QPoint& anchor);

    DockWindow*          m_dock;
    NotificationManager* m_notifMgr = nullptr;
    NetworkPanel*        m_netPanel  = nullptr;

    // Widgets de estado
    QLabel*     m_lblBateria;
    QLabel*     m_lblRed;
    QLabel*     m_lblVolumen;
    QLabel*     m_lblNotif;
    QPushButton* m_btnBateria;
    QPushButton* m_btnRed;
    QPushButton* m_btnNotif;
    QSlider*    m_sliderVolumen;

    // Estado
    int     m_nivelBateria = -1;
    bool    m_cargando     = false;
    bool    m_redConectada = false;
    QString m_nombreRed;
    int     m_nivelVolumen = 70;
    int     m_notifCount   = 0;

    // Animaciones
    QPropertyAnimation* m_animOpacidad;
    QPropertyAnimation* m_animSlide;
    QTimer*             m_timerUpdate;
    QTimer*             m_timerIgnorarClics;

    qreal m_opacidad = 0.0;
    int   m_slideY   = 0;
    bool  m_visible  = false;
    bool  m_ignorarClicsTemporales = false;
    QPoint m_anchorPos;
};
