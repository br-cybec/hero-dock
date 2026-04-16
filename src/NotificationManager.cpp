#include "NotificationManager.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QScreen>
#include <QApplication>
#include <QMouseEvent>
#include <QDBusConnection>
#include <QDBusError>
#include <QFont>
#include <QFontMetrics>
#include <QDateTime>
#include <QDebug>
#include <algorithm>

// ═══════════════════════════════════════════════════════════════════════════
// NotifBadge  — pequeño círculo con número sobre el ícono de notificaciones
// ═══════════════════════════════════════════════════════════════════════════

NotifBadge::NotifBadge(QWidget* parent) : QWidget(parent) {
    setFixedSize(18, 18);
    setAttribute(Qt::WA_TranslucentBackground);
    hide();

    m_animPop  = new QPropertyAnimation(this, "escala",   this);
    m_animFade = new QPropertyAnimation(this, "opacidad", this);

    m_animPop->setDuration(280);
    m_animPop->setEasingCurve(QEasingCurve::OutBack);
    m_animFade->setDuration(220);
    m_animFade->setEasingCurve(QEasingCurve::OutCubic);
}

void NotifBadge::setCount(int n) {
    bool cambia = (n > 0) != (m_count > 0);
    m_count = n;

    if (n > 0) {
        show();
        if (cambia) {
            m_animPop->stop();
            m_animPop->setStartValue(0.0);
            m_animPop->setEndValue(1.0);
            m_animPop->start();
            m_animFade->stop();
            m_animFade->setStartValue(0.0);
            m_animFade->setEndValue(1.0);
            m_animFade->start();
        } else {
            // Pulso al recibir nueva notificación
            auto* seq = new QPropertyAnimation(this, "escala", this);
            seq->setDuration(160);
            seq->setKeyValueAt(0,   1.0);
            seq->setKeyValueAt(0.5, 1.3);
            seq->setKeyValueAt(1,   1.0);
            seq->setEasingCurve(QEasingCurve::InOutQuad);
            seq->start(QAbstractAnimation::DeleteWhenStopped);
        }
    } else {
        m_animFade->stop();
        m_animFade->setStartValue(m_opacidad);
        m_animFade->setEndValue(0.0);
        connect(m_animFade, &QPropertyAnimation::finished, this, [this](){
            hide();
            disconnect(m_animFade, &QPropertyAnimation::finished, nullptr, nullptr);
        });
        m_animFade->start();
    }
    update();
}

void NotifBadge::paintEvent(QPaintEvent*) {
    if (m_count <= 0 || m_opacidad < 0.01) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(m_opacidad);

    int sz    = (int)(16 * m_escala);
    int cx    = width()/2, cy = height()/2;
    QRect rc(cx-sz/2, cy-sz/2, sz, sz);

    // Fondo rojo-naranja
    QRadialGradient rg(rc.center(), sz/2.0);
    rg.setColorAt(0, QColor(255,80,60));
    rg.setColorAt(1, QColor(220,40,30));
    QPainterPath circle; circle.addEllipse(rc);
    p.fillPath(circle, rg);

    // Número
    QString txt = m_count > 99 ? "99+" : QString::number(m_count);
    QFont f; f.setFamily("Noto Sans"); f.setPixelSize(sz > 14 ? 9 : 8); f.setBold(true);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(rc, Qt::AlignCenter, txt);
}

// ═══════════════════════════════════════════════════════════════════════════
// NotifPopup  — tarjeta flotante de notificación
// ═══════════════════════════════════════════════════════════════════════════

NotifPopup::NotifPopup(const Notificacion& notif, QWidget* parent)
    : QWidget(parent), m_notif(notif)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                   Qt::WindowStaysOnTopHint | Qt::Tool |
                   Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

    setFixedWidth(340);

    // Calcular altura necesaria según contenido del body
    QFont f; f.setFamily("Noto Sans"); f.setPixelSize(12);
    QFontMetrics fm(f);
    int bodyLines = qMin(3, (int)((notif.cuerpo.length() / 40) + 1));
    setFixedHeight(60 + bodyLines * 16 + 16);

    m_animFade  = new QPropertyAnimation(this, "opacidad",  this);
    m_animSlide = new QPropertyAnimation(this, "offsetY",   this);

    m_timerAuto = new QTimer(this);
    m_timerAuto->setSingleShot(true);
    int ms = (notif.timeout > 0) ? notif.timeout : 5000;
    m_timerAuto->setInterval(ms);
    connect(m_timerAuto, &QTimer::timeout, this, &NotifPopup::cerrar);
}

void NotifPopup::mostrar() {
    QScreen* s = QApplication::primaryScreen();
    QRect sg   = s->geometry();
    m_basePos  = QPoint(sg.x() + sg.width() - width() - 16,
                        sg.y() + 20);
    m_offsetY  = 0;
    reposicionar();
    show();

    // Slide desde arriba + fade
    m_animSlide->stop();
    m_animSlide->setStartValue(-height());
    m_animSlide->setEndValue(0);
    m_animSlide->setDuration(320);
    m_animSlide->setEasingCurve(QEasingCurve::OutCubic);
    m_animSlide->start();

    m_animFade->stop();
    m_animFade->setStartValue(0.0);
    m_animFade->setEndValue(1.0);
    m_animFade->setDuration(250);
    m_animFade->start();

    m_timerAuto->start();
}

void NotifPopup::cerrar() {
    m_timerAuto->stop();
    m_animFade->stop();
    m_animFade->setStartValue(m_opacidad);
    m_animFade->setEndValue(0.0);
    m_animFade->setDuration(300);
    disconnect(m_animFade, &QPropertyAnimation::finished, nullptr, nullptr);
    connect(m_animFade, &QPropertyAnimation::finished, this, [this](){
        emit cerrada(m_notif.id);
        deleteLater();
    });
    m_animFade->start();
}

void NotifPopup::reposicionar() {
    move(m_basePos.x(), m_basePos.y() + m_offsetY);
}

void NotifPopup::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

    QRect rc = rect().adjusted(2,2,-2,-2);
    QPainterPath path; path.addRoundedRect(rc, 14, 14);

    // Fondo con gradiente oscuro profundo
    QLinearGradient bg(rc.topLeft(), rc.bottomLeft());
    bg.setColorAt(0, QColor(28,30,42,248));
    bg.setColorAt(1, QColor(18,20,30,248));
    p.fillPath(path, bg);

    // Borde aurora izquierdo (tira de color)
    QPainterPath leftBar;
    leftBar.addRoundedRect(QRectF(rc.x(),rc.y()+8,4,rc.height()-16), 2, 2);
    QLinearGradient barGrad(rc.topLeft(), rc.bottomLeft());
    barGrad.setColorAt(0, QColor(80,160,255,200));
    barGrad.setColorAt(1, QColor(140,80,255,200));
    p.fillPath(leftBar, barGrad);

    // Borde exterior sutil
    p.setPen(QPen(QColor(80,140,255,55), 1));
    p.drawPath(path);

    int px = 16, py = 12;

    // Ícono de la app
    if (!m_notif.icono.isNull()) {
        QPixmap px2 = m_notif.icono.pixmap(32, 32);
        if (!px2.isNull()) {
            QPainterPath iconClip; iconClip.addRoundedRect(QRectF(px,py,32,32),7,7);
            p.save(); p.setClipPath(iconClip);
            p.drawPixmap(px, py, 32, 32, px2);
            p.restore();
        }
    } else {
        // Placeholder de color
        QPainterPath ic; ic.addRoundedRect(QRectF(px,py,32,32),7,7);
        QLinearGradient ig(QPoint(px,py), QPoint(px+32,py+32));
        ig.setColorAt(0,QColor(60,120,220)); ig.setColorAt(1,QColor(100,60,200));
        p.fillPath(ic, ig);
        QFont fL; fL.setFamily("Noto Sans"); fL.setPixelSize(14); fL.setBold(true);
        p.setFont(fL); p.setPen(Qt::white);
        p.drawText(QRect(px,py,32,32), Qt::AlignCenter,
                   m_notif.appName.left(1).toUpper());
    }

    int textX = px + 40;
    int textW = rc.width() - textX - 12;

    // App name + tiempo
    QFont fApp; fApp.setFamily("Noto Sans"); fApp.setPixelSize(10);
    p.setFont(fApp); p.setPen(QColor(100,120,180));
    QString ago = "";
    int secs = m_notif.cuando.secsTo(QDateTime::currentDateTime());
    if (secs < 60)       ago = "ahora";
    else if (secs < 3600) ago = QString("hace %1 min").arg(secs/60);
    else                  ago = QString("hace %1 h").arg(secs/3600);
    p.drawText(QRect(textX, py, textW/2, 14), Qt::AlignLeft, m_notif.appName);
    p.drawText(QRect(textX, py, textW, 14), Qt::AlignRight, ago);

    // Título
    QFont fTit; fTit.setFamily("Noto Sans"); fTit.setPixelSize(13); fTit.setBold(true);
    p.setFont(fTit); p.setPen(QColor(220,225,240));
    p.drawText(QRect(textX, py+16, textW, 18), Qt::AlignLeft|Qt::TextSingleLine,
               m_notif.titulo.length()>40 ? m_notif.titulo.left(38)+"…" : m_notif.titulo);

    // Cuerpo
    if (!m_notif.cuerpo.isEmpty()) {
        QFont fBody; fBody.setFamily("Noto Sans"); fBody.setPixelSize(11);
        p.setFont(fBody); p.setPen(QColor(150,158,185));
        p.drawText(QRect(textX, py+36, textW, height()-py-46),
                   Qt::AlignLeft|Qt::AlignTop|Qt::TextWordWrap,
                   m_notif.cuerpo.length()>120 ? m_notif.cuerpo.left(118)+"…" : m_notif.cuerpo);
    }

    // Botón X
    p.setPen(QColor(120,130,160));
    QFont fX; fX.setPixelSize(14); p.setFont(fX);
    p.drawText(QRect(rc.width()-26, py, 20, 20), Qt::AlignCenter, "×");
}

void NotifPopup::mousePressEvent(QMouseEvent* e) {
    cerrar();
    QWidget::mousePressEvent(e);
}

// ═══════════════════════════════════════════════════════════════════════════
// NotificationManager
// ═══════════════════════════════════════════════════════════════════════════

NotificationManager::NotificationManager(QObject* parent) : QObject(parent) {
    m_badge = new NotifBadge(nullptr);

    // Registrar servicio DBus
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService("org.freedesktop.Notifications")) {
        qWarning() << "No se pudo registrar org.freedesktop.Notifications:"
                   << bus.lastError().message();
    } else {
        new NotifDBusAdaptor(this);
        bus.registerObject("/org/freedesktop/Notifications", this);
    }
}

uint NotificationManager::recibirNotificacion(const QString& appName, uint replacesId,
                                               const QString& appIcon, const QString& summary,
                                               const QString& body,
                                               const QStringList&, const QVariantMap& hints,
                                               int expireTimeout)
{
    uint id = (replacesId > 0) ? replacesId : m_nextId++;

    Notificacion n;
    n.id      = id;
    n.appName = appName.isEmpty() ? "Sistema" : appName;
    n.titulo  = summary;
    n.cuerpo  = body;
    n.timeout = expireTimeout;
    n.cuando  = QDateTime::currentDateTime();

    // Cargar ícono
    if (!appIcon.isEmpty()) {
        n.icono = QIcon::fromTheme(appIcon);
        if (n.icono.isNull()) n.icono = QIcon(appIcon);
    }
    // Fallback desde hints
    if (n.icono.isNull()) {
        auto it = hints.find("image-path");
        if (it != hints.end()) n.icono = QIcon(it->toString());
    }

    // Reemplazar si existe
    for (int i=0; i<m_historial.size(); i++)
        if (m_historial[i].id == id) { m_historial[i]=n; goto done; }
    m_historial.prepend(n);
    if (m_historial.size() > 50) m_historial.removeLast();

done:
    m_noLeidas++;
    m_badge->setCount(m_noLeidas);
    mostrarPopup(n);
    emit nuevaNotificacion(n);
    emit contadorCambiado(m_noLeidas);
    return id;
}

void NotificationManager::cerrarNotificacion(uint id) {
    for (NotifPopup* pop : m_popupsActivos)
        if (pop->property("notifId").toUInt() == id)
            pop->cerrar();
}

void NotificationManager::mostrarPopup(const Notificacion& n) {
    auto* popup = new NotifPopup(n, nullptr);
    popup->setProperty("notifId", n.id);

    // Posición apilada: la más reciente arriba
    connect(popup, &NotifPopup::cerrada, this, [this, popup](uint) {
        m_popupsActivos.removeAll(popup);
        reposicionarPopups();
    });

    m_popupsActivos.prepend(popup);
    reposicionarPopups();
    popup->mostrar();
}

void NotificationManager::reposicionarPopups() {
    QScreen* s = QApplication::primaryScreen();
    QRect sg   = s->geometry();
    int y      = sg.y() + 20;

    for (NotifPopup* pop : m_popupsActivos) {
        QPoint base(sg.x() + sg.width() - pop->width() - 16, y);
        pop->setBasePos(base);
        y += pop->height() + 10;
        if (y + pop->height() > sg.y() + sg.height() - 100) break;
    }
}

void NotificationManager::marcarLeidas() {
    m_noLeidas = 0;
    m_badge->setCount(0);
    emit contadorCambiado(0);
}

// ═══════════════════════════════════════════════════════════════════════════
// NotifDBusAdaptor
// ═══════════════════════════════════════════════════════════════════════════

NotifDBusAdaptor::NotifDBusAdaptor(NotificationManager* mgr)
    : QDBusAbstractAdaptor(mgr), m_mgr(mgr)
{
    setAutoRelaySignals(true);
}

QStringList NotifDBusAdaptor::GetCapabilities() {
    return {"body", "body-markup", "icon-static", "persistence"};
}

uint NotifDBusAdaptor::Notify(const QString& appName, uint replacesId,
                               const QString& appIcon, const QString& summary,
                               const QString& body, const QStringList& actions,
                               const QVariantMap& hints, int expireTimeout) {
    return m_mgr->recibirNotificacion(appName, replacesId, appIcon,
                                      summary, body, actions, hints, expireTimeout);
}

void NotifDBusAdaptor::CloseNotification(uint id) {
    m_mgr->cerrarNotificacion(id);
    emit NotificationClosed(id, 3);
}

void NotifDBusAdaptor::GetServerInformation(QString& name, QString& vendor,
                                             QString& version, QString& specVersion) {
    name = "QDock"; vendor = "QDock"; version = "3.0"; specVersion = "1.2";
}
