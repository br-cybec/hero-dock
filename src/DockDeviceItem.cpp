#include "DockDeviceItem.h"
#include "DockWindow.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QConicalGradient>
#include <QMouseEvent>
#include <QFontMetrics>
#include <functional>
#include <cmath>

DockDeviceItem::DockDeviceItem(const DispositivoInfo& info,
                               DockWindow* dock,
                               QWidget* parent)
    : QWidget(parent), m_info(info), m_dock(dock)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);

    m_animEscala  = new QPropertyAnimation(this, "escala",  this);
    m_animEscala->setDuration(150);
    m_animEscala->setEasingCurve(QEasingCurve::OutCubic);

    m_animAparece = new QPropertyAnimation(this, "aparece", this);
    m_animAparece->setDuration(350);
    m_animAparece->setEasingCurve(QEasingCurve::OutCubic);

    m_timerEtiq = new QTimer(this);
    m_timerEtiq->setSingleShot(true);
    m_timerEtiq->setInterval(600);
    connect(m_timerEtiq, &QTimer::timeout, [this]{
        m_mostrarEtiqueta = true; update();
    });
}

void DockDeviceItem::actualizarInfo(const DispositivoInfo& info) {
    m_info = info;
    update();
}

QSize DockDeviceItem::tamano() const {
    int base = m_dock ? m_dock->config().tamanoIcono : 52;
    int sz   = base + 22;
    return QSize(sz, sz);
}

// ─── Animaciones de entrada / salida ─────────────────────────────────────

void DockDeviceItem::animarEntrada() {
    setWindowOpacity(0.0);
    m_animAparece->stop();
    m_animAparece->setStartValue(0.0);
    m_animAparece->setEndValue(1.0);
    m_animAparece->start();

    // Rebote de entrada
    m_animEscala->stop();
    m_animEscala->setStartValue(0.6);
    m_animEscala->setEndValue(1.0);
    m_animEscala->setEasingCurve(QEasingCurve::OutBack);
    m_animEscala->setDuration(380);
    m_animEscala->start();
}

void DockDeviceItem::animarSalida(std::function<void()> onDone) {
    m_animAparece->stop();
    m_animAparece->setStartValue(m_aparece);
    m_animAparece->setEndValue(0.0);
    m_animAparece->setDuration(280);

    m_animEscala->stop();
    m_animEscala->setStartValue(m_escala);
    m_animEscala->setEndValue(0.5);
    m_animEscala->setDuration(280);
    m_animEscala->setEasingCurve(QEasingCurve::InCubic);

    connect(m_animAparece, &QPropertyAnimation::finished,
            this, [onDone]{ if (onDone) onDone(); });

    m_animAparece->start();
    m_animEscala->start();
}

// ─── Pintar ──────────────────────────────────────────────────────────────

void DockDeviceItem::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing |
                     QPainter::SmoothPixmapTransform |
                     QPainter::TextAntialiasing);

    int  sz  = (int)(m_dock ? m_dock->config().tamanoIcono * m_escala : 52 * m_escala);
    int  cx  = width()  / 2;
    int  cy  = height() / 2;
    QRect rc(cx - sz/2, cy - sz/2, sz, sz);

    // ── Fondo del ícono ──────────────────────────────────────────────────
    QPainterPath bgPath;
    bgPath.addRoundedRect(rc, sz * 0.22, sz * 0.22);

    // Color de fondo según tipo de dispositivo
    QLinearGradient bgGrad(rc.topLeft(), rc.bottomRight());
    if (m_info.tipoIcono == "usb") {
        bgGrad.setColorAt(0, QColor(40, 100, 180, 220));
        bgGrad.setColorAt(1, QColor(20,  60, 140, 220));
    } else if (m_info.tipoIcono == "optical") {
        bgGrad.setColorAt(0, QColor(80,  60, 160, 220));
        bgGrad.setColorAt(1, QColor(50,  30, 120, 220));
    } else if (m_info.tipoIcono == "sdcard") {
        bgGrad.setColorAt(0, QColor(30, 130,  90, 220));
        bgGrad.setColorAt(1, QColor(20,  90,  60, 220));
    } else { // hdd externo
        bgGrad.setColorAt(0, QColor(60,  60,  80, 220));
        bgGrad.setColorAt(1, QColor(35,  35,  55, 220));
    }

    if (m_hover) {
        // Aclarar un poco en hover
        QColor h1 = bgGrad.stops().first().second.lighter(130);
        QColor h2 = bgGrad.stops().last().second.lighter(130);
        bgGrad.setColorAt(0, h1);
        bgGrad.setColorAt(1, h2);
    }

    p.fillPath(bgPath, bgGrad);

    // Borde sutil
    p.setPen(QPen(QColor(255, 255, 255, 45), 1.2));
    p.drawPath(bgPath);

    // Brillo interior superior (vidrio)
    QPainterPath shine;
    shine.addRoundedRect(QRectF(rc.x()+1, rc.y()+1, rc.width()-2, rc.height()/2.2),
                         sz*0.21, sz*0.21);
    QLinearGradient shineG(rc.topLeft(), QPoint(rc.left(), rc.top()+rc.height()/2));
    shineG.setColorAt(0, QColor(255,255,255,35));
    shineG.setColorAt(1, QColor(255,255,255,0));
    p.fillPath(shine, shineG);

    // ── Ícono del dispositivo ────────────────────────────────────────────
    if (!m_info.icono.isNull()) {
        int is = sz * 6 / 10;
        QRect ir(cx - is/2, cy - is/2 - 3, is, is);

        // Sombra del ícono
        p.save(); p.setOpacity(0.20);
        p.drawPixmap(ir.translated(0, 3), m_info.icono.pixmap(is, is));
        p.setOpacity(1.0); p.restore();

        p.drawPixmap(ir, m_info.icono.pixmap(is, is));
    } else {
        // Fallback: letra del tipo
        QFont f; f.setFamily("Noto Sans");
        f.setPixelSize(sz/3); f.setBold(true);
        p.setFont(f); p.setPen(QColor(255,255,255,200));
        QString letra = m_info.tipoIcono == "usb" ? "U"
                      : m_info.tipoIcono == "optical" ? "⊙"
                      : m_info.tipoIcono == "sdcard" ? "SD" : "⊟";
        p.drawText(rc, Qt::AlignCenter, letra);
    }

    // ── Barra de uso de espacio (esquina inferior del ícono) ─────────────
    if (m_info.tamanoTotal > 0) {
        dibujarBarra(p, rc);
    }

    // ── Etiqueta con nombre ──────────────────────────────────────────────
    if (m_mostrarEtiqueta && m_hover) {
        dibujarEtiqueta(p);
    }
}

void DockDeviceItem::dibujarBarra(QPainter& p, const QRect& rc) {
    // Mini barra de progreso al borde inferior del ícono
    qreal uso = (qreal)m_info.tamanoUsado / (qreal)m_info.tamanoTotal;
    uso = qBound(0.0, uso, 1.0);

    int bw = rc.width() - 8, bh = 4;
    int bx = rc.x() + 4, by = rc.bottom() - bh - 2;
    QRect bRect(bx, by, bw, bh);

    // Pista
    QPainterPath track; track.addRoundedRect(bRect, 2, 2);
    p.fillPath(track, QColor(0,0,0,80));

    // Relleno
    QColor fillColor = uso > 0.85 ? QColor(220,60,60,200)
                     : uso > 0.65 ? QColor(240,180,40,200)
                                  : QColor(60,180,120,200);
    int fw = qMax(4, (int)(bw * uso));
    QRect fill(bx, by, fw, bh);
    QPainterPath fillPath; fillPath.addRoundedRect(fill, 2, 2);
    p.fillPath(fillPath, fillColor);
}

void DockDeviceItem::dibujarEtiqueta(QPainter& p) {
    if (m_dock == nullptr) return;
    auto pos = m_dock->config().posicion;

    // Línea 1: nombre del dispositivo
    // Línea 2: espacio libre
    QString linea1 = m_info.etiqueta;
    if (linea1.length() > 22) linea1 = linea1.left(20) + "…";

    QString linea2;
    if (m_info.tamanoTotal > 0) {
        qint64 libre = m_info.tamanoTotal - m_info.tamanoUsado;
        linea2 = formatearTamano(libre) + " libres de " +
                 formatearTamano(m_info.tamanoTotal);
    }

    QFont f1; f1.setFamily("Noto Sans"); f1.setPixelSize(13); f1.setBold(true);
    QFont f2; f2.setFamily("Noto Sans"); f2.setPixelSize(10);

    QFontMetrics fm1(f1), fm2(f2);
    int tw = qMax(fm1.horizontalAdvance(linea1),
                  linea2.isEmpty() ? 0 : fm2.horizontalAdvance(linea2)) + 24;
    int th = linea2.isEmpty() ? fm1.height() + 10
                              : fm1.height() + fm2.height() + 14;

    QRect lr;
    if      (pos == DockPosition::Abajo)
        lr = QRect(width()/2 - tw/2, -th - 12, tw, th);
    else if (pos == DockPosition::Arriba)
        lr = QRect(width()/2 - tw/2, height() + 12, tw, th);
    else if (pos == DockPosition::Izquierda)
        lr = QRect(width() + 12, height()/2 - th/2, tw, th);
    else
        lr = QRect(-tw - 12, height()/2 - th/2, tw, th);

    p.save();
    QPainterPath bg; bg.addRoundedRect(lr, 10, 10);
    p.fillPath(bg, QColor(12, 14, 22, 232));
    p.setPen(QPen(QColor(80,140,255,55), 1));
    p.drawPath(bg);

    // Nombre
    p.setFont(f1);
    p.setPen(QColor(230, 235, 248));
    p.drawText(QRect(lr.x()+12, lr.y()+6, tw-24, fm1.height()),
               Qt::AlignLeft | Qt::AlignVCenter, linea1);

    // Espacio
    if (!linea2.isEmpty()) {
        p.setFont(f2);
        p.setPen(QColor(130, 145, 185));
        p.drawText(QRect(lr.x()+12, lr.y()+6+fm1.height()+2, tw-24, fm2.height()),
                   Qt::AlignLeft | Qt::AlignVCenter, linea2);
    }
    p.restore();
}

QString DockDeviceItem::formatearTamano(qint64 bytes) const {
    if (bytes <= 0) return "0 B";
    double v = bytes;
    QStringList units = {"B","KB","MB","GB","TB"};
    int idx = 0;
    while (v >= 1024.0 && idx < units.size()-1) { v /= 1024.0; idx++; }
    return QString("%1 %2").arg(v, 0, 'f', idx > 1 ? 1 : 0).arg(units[idx]);
}

// ─── Eventos ─────────────────────────────────────────────────────────────

void DockDeviceItem::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_presion = true;
        m_animEscala->stop();
        m_animEscala->setStartValue(m_escala);
        m_animEscala->setEndValue(0.88);
        m_animEscala->setDuration(120);
        m_animEscala->setEasingCurve(QEasingCurve::OutCubic);
        m_animEscala->start();
    }
    QWidget::mousePressEvent(e);
}

void DockDeviceItem::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && m_presion) {
        m_presion = false;
        m_animEscala->stop();
        m_animEscala->setStartValue(m_escala);
        m_animEscala->setEndValue(m_hover ? 1.07 : 1.0);
        m_animEscala->setDuration(150);
        m_animEscala->setEasingCurve(QEasingCurve::OutBack);
        m_animEscala->start();
        if (rect().contains(e->pos()))
            emit clicIzquierdo(m_info.id);
    } else if (e->button() == Qt::RightButton) {
        emit clicDerecho(m_info.id, e->globalPosition().toPoint());
    }
    QWidget::mouseReleaseEvent(e);
}

void DockDeviceItem::enterEvent(QEnterEvent* e) {
    m_hover = true;
    m_timerEtiq->start();
    m_animEscala->stop();
    m_animEscala->setStartValue(m_escala);
    m_animEscala->setEndValue(1.07);
    m_animEscala->setDuration(150);
    m_animEscala->setEasingCurve(QEasingCurve::OutCubic);
    m_animEscala->start();
    update();
    QWidget::enterEvent(e);
}

void DockDeviceItem::leaveEvent(QEvent* e) {
    m_hover = false;
    m_mostrarEtiqueta = false;
    m_timerEtiq->stop();
    m_animEscala->stop();
    m_animEscala->setStartValue(m_escala);
    m_animEscala->setEndValue(1.0);
    m_animEscala->setDuration(150);
    m_animEscala->start();
    update();
    QWidget::leaveEvent(e);
}
