#pragma once
#include <QWidget>
#include <QPropertyAnimation>
#include <QTimer>
#include <QIcon>
#include "DockConfig.h"
#include "ApplicationScanner.h"

class DockWindow;

enum class TipoEspecial {
    Ninguno,
    BotonApps,
    BotonMenu,
    Separador,
    Papelera,
    Reloj,
    Bateria,
    Red,
    Notificaciones
};

class DockItem : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal escala       READ escala       WRITE setEscala)
    Q_PROPERTY(qreal rebote       READ rebote       WRITE setRebote)
    Q_PROPERTY(qreal brillo       READ brillo       WRITE setBrillo)
    Q_PROPERTY(qreal aroBateria   READ aroBateria   WRITE setAroBateria)

public:
    explicit DockItem(const InfoApp& app, DockWindow* dock, QWidget* parent = nullptr);
    explicit DockItem(TipoEspecial tipo, DockWindow* dock, QWidget* parent = nullptr);

    InfoApp      infoApp()    const { return m_app; }
    TipoEspecial tipoEsp()    const { return m_tipoEsp; }
    bool         esApp()      const { return m_tipoEsp == TipoEspecial::Ninguno; }
    bool         estaAnclada()     const { return m_app.estaAnclada; }
    bool         estaEjecutando()  const { return m_app.estaEjecutando; }
    QList<quint64> ventanas() const { return m_app.ventanas; }
    int          tamanoActual()    const;
    QSize        tamano()          const;

    void setEjecutando(bool v)         { m_app.estaEjecutando = v; update(); }
    void agregarVentana(quint64 id);
    void quitarVentana(quint64 id);
    void setActiva(bool v)             { m_activa = v; update(); }
    void setAnclada(bool v)            { m_app.estaAnclada = v; }
    void setTamanoMagnificado(int sz)  { m_tamMag = sz; updateGeometry(); update(); }
    void setContadorNotif(int n)       { m_notifCount = n; update(); }
    void actualizarDatosSistema();

    // Animatable props
    qreal escala()     const { return m_escala;     } void setEscala(qreal v)     { m_escala=v;     update(); }
    qreal rebote()     const { return m_rebote;     } void setRebote(qreal v)     { m_rebote=v;     update(); }
    qreal brillo()     const { return m_brillo;     } void setBrillo(qreal v)     { m_brillo=v;     update(); }
    qreal aroBateria() const { return m_aroBateria; } void setAroBateria(qreal v) { m_aroBateria=v; update(); }

    void lanzar();
    void animarRebote();
    void animarPulso();

signals:
    void clicIzquierdo();
    void clicDerecho(QPoint pos);
    void clicMedio();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    void paintApp(QPainter& p);
    void paintBotonApps(QPainter& p);
    void paintBotonMenu(QPainter& p);
    void paintSeparador(QPainter& p);
    void paintPapelera(QPainter& p);
    void paintReloj(QPainter& p);
    void paintBateria(QPainter& p);
    void paintRed(QPainter& p);
    void paintNotificaciones(QPainter& p);

    void dibujarIndicadores(QPainter& p, const QRect& rc);
    void dibujarEtiqueta(QPainter& p, const QString& texto);
    void dibujarIconoRedondeado(QPainter& p, const QRect& rc, const QIcon& icon, int sz);

    // Aro circular de batería con animación
    void paintAroBateria(QPainter& p, int cx, int cy, int radio,
                         qreal porcentaje, bool cargando, qreal animOffset);

    InfoApp      m_app;
    TipoEspecial m_tipoEsp  = TipoEspecial::Ninguno;
    DockWindow*  m_dock;

    qreal  m_escala     = 1.0;
    qreal  m_rebote     = 0.0;
    qreal  m_brillo     = 0.0;
    qreal  m_aroBateria = 0.0;   // 0..1 animado al cambiar nivel
    bool   m_hover      = false;
    bool   m_presion    = false;
    bool   m_activa     = false;
    int    m_tamMag     = 0;
    bool   m_mostrarEtiqueta = false;
    int    m_notifCount = 0;

    // Datos sistema
    int     m_nivelBateria     = -1;
    int     m_nivelBateriaAnim = 0;   // nivel previo para animar
    bool    m_cargando         = false;
    bool    m_redConectada     = false;
    QString m_nombreRed;
    QTimer* m_timerSistema  = nullptr;
    QTimer* m_timerCarga    = nullptr;  // pulso de carga
    qreal   m_pulsoCarga    = 0.0;      // 0..1 animado

    // Animaciones
    QPropertyAnimation* m_animEscala     = nullptr;
    QPropertyAnimation* m_animRebote     = nullptr;
    QPropertyAnimation* m_animBrillo     = nullptr;
    QPropertyAnimation* m_animAroBat     = nullptr;
    QPropertyAnimation* m_animPulsoCarga = nullptr;
    QTimer*             m_timerEtiq      = nullptr;
    QTimer*             m_timerRebote    = nullptr;
    int                 m_contRebote     = 0;
};
