#pragma once
#include <QObject>
#include <QWidget>
#include <QPropertyAnimation>
#include <QTimer>
#include <QList>
#include <QDBusAbstractAdaptor>
#include <QDateTime>
#include <QIcon>

struct Notificacion {
    uint    id;
    QString appName;
    QString titulo;
    QString cuerpo;
    QIcon   icono;
    int     timeout;   // ms, -1 = persistente
    QDateTime cuando;
};

// ─── Badge animado del dock (muestra N cuando hay notificaciones) ─────────
class NotifBadge : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal escala READ escala WRITE setEscala)
    Q_PROPERTY(qreal opacidad READ opacidad WRITE setOpacidad)
public:
    explicit NotifBadge(QWidget* parent = nullptr);
    void setCount(int n);
    int  count() const { return m_count; }

    qreal escala()  const { return m_escala; }
    qreal opacidad()const { return m_opacidad; }
    void setEscala(qreal v)  { m_escala=v; update(); }
    void setOpacidad(qreal v){ m_opacidad=v; update(); }

protected:
    void paintEvent(QPaintEvent*) override;
private:
    int   m_count   = 0;
    qreal m_escala  = 1.0;
    qreal m_opacidad= 0.0;
    QPropertyAnimation* m_animPop;
    QPropertyAnimation* m_animFade;
};

// ─── Popup flotante de notificación ─────────────────────────────────────
class NotifPopup : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal opacidad READ opacidad WRITE setOpacidad)
    Q_PROPERTY(int   offsetY  READ offsetY  WRITE setOffsetY)
public:
    explicit NotifPopup(const Notificacion& notif, QWidget* parent = nullptr);

    void mostrar();
    void cerrar();

    qreal opacidad() const { return m_opacidad; }
    void  setOpacidad(qreal v) { m_opacidad=v; setWindowOpacity(v); }
    int   offsetY() const  { return m_offsetY; }
    void  setOffsetY(int v){ m_offsetY=v; reposicionar(); }

    void setBasePos(QPoint p) { m_basePos=p; reposicionar(); }

signals:
    void cerrada(uint id);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;

private:
    void reposicionar();
    Notificacion        m_notif;
    qreal               m_opacidad = 0.0;
    int                 m_offsetY  = 0;
    QPoint              m_basePos;
    QPropertyAnimation* m_animFade;
    QPropertyAnimation* m_animSlide;
    QTimer*             m_timerAuto;
};

// ─── Manager principal (recibe notificaciones DBus y las muestra) ────────
class NotificationManager : public QObject {
    Q_OBJECT
public:
    explicit NotificationManager(QObject* parent = nullptr);

    // Llamado por el adaptador DBus
    uint recibirNotificacion(const QString& appName, uint replacesId,
                             const QString& appIcon, const QString& summary,
                             const QString& body,
                             const QStringList& actions,
                             const QVariantMap& hints,
                             int expireTimeout);
    void cerrarNotificacion(uint id);

    // Widget badge para embeber en el dock
    NotifBadge* badge() const { return m_badge; }

    QList<Notificacion> historial() const { return m_historial; }
    int  totalNoLeidas() const { return m_noLeidas; }
    void marcarLeidas();

signals:
    void nuevaNotificacion(const Notificacion& n);
    void contadorCambiado(int total);

private:
    void mostrarPopup(const Notificacion& n);
    void reposicionarPopups();

    uint                    m_nextId   = 1;
    int                     m_noLeidas = 0;
    QList<Notificacion>     m_historial;
    QList<NotifPopup*>      m_popupsActivos;
    NotifBadge*             m_badge;
};

// ─── Adaptador DBus (implementa org.freedesktop.Notifications) ──────────
class NotifDBusAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")
public:
    explicit NotifDBusAdaptor(NotificationManager* mgr);

public slots:
    // DBus methods
    QStringList GetCapabilities();
    uint Notify(const QString& appName, uint replacesId,
                const QString& appIcon, const QString& summary,
                const QString& body, const QStringList& actions,
                const QVariantMap& hints, int expireTimeout);
    void CloseNotification(uint id);
    void GetServerInformation(QString& name, QString& vendor,
                              QString& version, QString& specVersion);
signals:
    void NotificationClosed(uint id, uint reason);
    void ActionInvoked(uint id, const QString& actionKey);

private:
    NotificationManager* m_mgr;
};
