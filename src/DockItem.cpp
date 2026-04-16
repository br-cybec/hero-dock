#include "DockItem.h"
#include "DockWindow.h"
#include <QPainter>
#include <QPainterPath>
#include <QConicalGradient>
#include <QRadialGradient>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QProcess>
#include <QFontMetrics>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QDBusInterface>
#include <cmath>

static bool isHoriz(DockWindow* d) {
    return d && (d->config().posicion==DockPosition::Abajo||d->config().posicion==DockPosition::Arriba);
}

// ─── Constructor ─────────────────────────────────────────────────────────

static void initAnims(DockItem* s,
    QPropertyAnimation*& aE, QPropertyAnimation*& aR,
    QPropertyAnimation*& aB, QPropertyAnimation*& aAro,
    QTimer*& tEtiq, QTimer*& tR)
{
    aE = new QPropertyAnimation(s,"escala",s);   aE->setDuration(150); aE->setEasingCurve(QEasingCurve::OutCubic);
    aR = new QPropertyAnimation(s,"rebote",s);   aR->setDuration(180); aR->setEasingCurve(QEasingCurve::OutQuad);
    aB = new QPropertyAnimation(s,"brillo",s);   aB->setDuration(300); aB->setEasingCurve(QEasingCurve::OutCubic);
    aAro=new QPropertyAnimation(s,"aroBateria",s);aAro->setDuration(800);aAro->setEasingCurve(QEasingCurve::OutCubic);
    tEtiq=new QTimer(s); tEtiq->setSingleShot(true); tEtiq->setInterval(550);
    tR=new QTimer(s);
}

DockItem::DockItem(const InfoApp& app, DockWindow* dock, QWidget* parent)
    : QWidget(parent), m_app(app), m_tipoEsp(TipoEspecial::Ninguno), m_dock(dock)
{
    setAttribute(Qt::WA_TranslucentBackground); setMouseTracking(true);
    initAnims(this,m_animEscala,m_animRebote,m_animBrillo,m_animAroBat,m_timerEtiq,m_timerRebote);
    connect(m_timerEtiq,   &QTimer::timeout, [this]{ m_mostrarEtiqueta=true; update(); });
    connect(m_timerRebote, &QTimer::timeout, this, &DockItem::animarRebote);
}

DockItem::DockItem(TipoEspecial tipo, DockWindow* dock, QWidget* parent)
    : QWidget(parent), m_tipoEsp(tipo), m_dock(dock)
{
    setAttribute(Qt::WA_TranslucentBackground); setMouseTracking(true);
    initAnims(this,m_animEscala,m_animRebote,m_animBrillo,m_animAroBat,m_timerEtiq,m_timerRebote);
    connect(m_timerEtiq,   &QTimer::timeout, [this]{ m_mostrarEtiqueta=true; update(); });
    connect(m_timerRebote, &QTimer::timeout, this, &DockItem::animarRebote);

    if (tipo==TipoEspecial::Bateria || tipo==TipoEspecial::Red || tipo==TipoEspecial::Reloj) {
        m_timerSistema = new QTimer(this);
        m_timerSistema->setInterval(tipo==TipoEspecial::Reloj ? 1000 : 10000);
        connect(m_timerSistema, &QTimer::timeout, this, &DockItem::actualizarDatosSistema);
        m_timerSistema->start();
        actualizarDatosSistema();
    }

    if (tipo==TipoEspecial::Bateria) {
        // Animación de pulso de carga
        m_animPulsoCarga = new QPropertyAnimation(this, "aroBateria", this);
        // Se reutiliza aroBateria como nivel animado
    }
}

// ─── Size ─────────────────────────────────────────────────────────────────

int DockItem::tamanoActual() const {
    return m_tamMag>0 ? m_tamMag : (m_dock ? m_dock->config().tamanoIcono : 52);
}
QSize DockItem::tamano() const {
    int base = m_dock ? m_dock->config().tamanoIcono : 52;
    bool h   = isHoriz(m_dock);
    switch (m_tipoEsp) {
    case TipoEspecial::Separador:
        return h ? QSize(14,base+22) : QSize(base+22,14);
    case TipoEspecial::Reloj:
        return h ? QSize(76,base+22) : QSize(base+22,76);
    case TipoEspecial::Bateria:
        return h ? QSize(base+22,base+22) : QSize(base+22,base+22);
    case TipoEspecial::Red:
        return h ? QSize(base+22,base+22) : QSize(base+22,base+22);
    case TipoEspecial::Notificaciones:
        return QSize(tamanoActual()+22, tamanoActual()+22);
    default: break;
    }
    int sz = tamanoActual()+22;
    return QSize(sz,sz);
}

// ─── Windows ──────────────────────────────────────────────────────────────

void DockItem::agregarVentana(quint64 id) {
    if (!m_app.ventanas.contains(id)) m_app.ventanas.append(id);
    m_app.estaEjecutando=true; update();
}
void DockItem::quitarVentana(quint64 id) {
    m_app.ventanas.removeAll(id);
    m_app.estaEjecutando=!m_app.ventanas.isEmpty(); update();
}

// ─── System data ──────────────────────────────────────────────────────────

void DockItem::actualizarDatosSistema() {
    if (m_tipoEsp==TipoEspecial::Bateria) {
        int nivelAnterior = m_nivelBateria;

        // Buscar batería en BAT0, BAT1, etc.
        auto readBatFile = [](const QString& suffix) -> QString {
            for (const QString& bat : {"BAT0","BAT1","BAT2","battery","AC"}) {
                QFile f(QString("/sys/class/power_supply/%1/%2").arg(bat, suffix));
                if (f.open(QIODevice::ReadOnly)) {
                    QString val = QString::fromUtf8(f.readAll()).trimmed();
                    f.close();
                    if (!val.isEmpty()) return val;
                }
            }
            return {};
        };

        QString cap = readBatFile("capacity");
        if (!cap.isEmpty()) m_nivelBateria = cap.toInt();

        QString st = readBatFile("status");
        if (!st.isEmpty())
            m_cargando = (st == "Charging" || st == "Full");

        // Inicializar o animar el aro
        if (m_nivelBateria >= 0) {
            qreal nivelFrac = (qreal)m_nivelBateria / 100.0;
            if (nivelAnterior < 0) {
                // Primera lectura: sin animación
                m_aroBateria = nivelFrac;
            } else if (m_nivelBateria != nivelAnterior) {
                // Cambio de nivel: animar
                m_animAroBat->stop();
                m_animAroBat->setStartValue(m_aroBateria);
                m_animAroBat->setEndValue(nivelFrac);
                m_animAroBat->start();
            }
        }
    }
    if (m_tipoEsp==TipoEspecial::Red) {
        m_redConectada=false;
        QDir netDir("/sys/class/net");
        for (const QString& iface : netDir.entryList(QDir::Dirs|QDir::NoDotAndDotDot)) {
            if (iface=="lo") continue;
            QFile op(QString("/sys/class/net/%1/operstate").arg(iface));
            if (op.open(QIODevice::ReadOnly)) {
                if (op.readAll().trimmed()=="up") { m_redConectada=true; m_nombreRed=iface; break; }
                op.close();
            }
        }
    }
    update();
}

// ─── Launch / Animate ─────────────────────────────────────────────────────

void DockItem::lanzar() {
    if (m_app.comandoExec.isEmpty()) return;
    QProcess::startDetached("/bin/sh", {"-c", m_app.comandoExec});
    if (m_dock && m_dock->config().rebotarAlAbrir) { m_contRebote=0; m_timerRebote->start(85); }
}

void DockItem::animarRebote() {
    if (m_contRebote>=6) { m_timerRebote->stop(); setRebote(0); return; }
    bool top = m_dock && m_dock->config().posicion==DockPosition::Arriba;
    qreal amp=(m_contRebote%2==0)?(top?10.0:-10.0):0.0;
    m_animRebote->stop();
    m_animRebote->setStartValue(m_rebote);
    m_animRebote->setEndValue(amp);
    m_animRebote->start();
    m_contRebote++;
}

void DockItem::animarPulso() {
    m_animBrillo->stop();
    m_animBrillo->setStartValue(0.8); m_animBrillo->setEndValue(0.0); m_animBrillo->setDuration(350);
    m_animBrillo->start();
}

// ═══════════════════════════════════════════════════════════════════════════
// PAINT helpers
// ═══════════════════════════════════════════════════════════════════════════

void DockItem::dibujarIconoRedondeado(QPainter& p, const QRect& rc, const QIcon& icon, int sz) {
    if (icon.isNull()) return;
    QPixmap px=icon.pixmap(sz,sz); if (px.isNull()) return;
    QPainterPath clip; clip.addRoundedRect(rc, sz*0.225, sz*0.225);
    p.save(); p.setClipPath(clip); p.drawPixmap(rc,px); p.restore();
}

void DockItem::dibujarIndicadores(QPainter& p, const QRect& rc) {
    auto pos=m_dock->config().posicion;
    int n=qMin((int)m_app.ventanas.size(),3); if(n<1) n=1;
    QColor col=m_activa?QColor(100,200,255,240):QColor(255,255,255,160);
    int dot=4,gap=7,total=n*dot+(n-1)*(gap-dot);
    for (int i=0;i<n;i++) {
        int x,y;
        if      (pos==DockPosition::Abajo)     {x=rc.center().x()-total/2+i*gap;y=rc.bottom()+5;}
        else if (pos==DockPosition::Arriba)    {x=rc.center().x()-total/2+i*gap;y=rc.top()-9;}
        else if (pos==DockPosition::Izquierda) {x=rc.left()-9;y=rc.center().y()-total/2+i*gap;}
        else                                   {x=rc.right()+5;y=rc.center().y()-total/2+i*gap;}
        p.setBrush(QColor(col.red(),col.green(),col.blue(),60)); p.setPen(Qt::NoPen);
        p.drawEllipse(x-2,y-2,dot+4,dot+4);
        p.setBrush(col); p.drawEllipse(x,y,dot,dot);
    }
}

void DockItem::dibujarEtiqueta(QPainter& p, const QString& texto) {
    if (texto.isEmpty()) return;
    QString t=texto; if(t.length()>26) t=t.left(24)+"…";
    QFont f; f.setFamily("Noto Sans"); f.setPixelSize(13); f.setWeight(QFont::Medium);
    p.setFont(f);
    QFontMetrics fm(f);
    int tw=fm.horizontalAdvance(t)+20, th=fm.height()+12;
    auto pos=m_dock->config().posicion;
    QRect lr;
    if      (pos==DockPosition::Abajo)     lr=QRect(width()/2-tw/2,-th-10,tw,th);
    else if (pos==DockPosition::Arriba)    lr=QRect(width()/2-tw/2,height()+10,tw,th);
    else if (pos==DockPosition::Izquierda) lr=QRect(width()+10,height()/2-th/2,tw,th);
    else                                   lr=QRect(-tw-10,height()/2-th/2,tw,th);
    p.save();
    QPainterPath bg; bg.addRoundedRect(lr,8,8);
    p.fillPath(bg,QColor(15,15,20,225));
    p.setPen(QPen(QColor(255,255,255,35),1)); p.drawPath(bg);
    p.setPen(QColor(240,240,248)); p.drawText(lr,Qt::AlignCenter,t);
    p.restore();
}

// ─── Aro circular de batería ──────────────────────────────────────────────

void DockItem::paintAroBateria(QPainter& p, int cx, int cy, int radio,
                                qreal porcentaje, bool cargando, qreal animOffset)
{
    // ── Pista del aro (fondo) ────────────────────────────────────────────
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255,255,255,28), 5.5, Qt::SolidLine, Qt::RoundCap));
    p.drawEllipse(QPointF(cx, cy), (qreal)radio, (qreal)radio);

    if (porcentaje < 0) {
        // Sin batería: mostrar signo de interrogación
        QFont f; f.setFamily("Noto Sans"); f.setPixelSize(radio*2/3); f.setBold(true);
        p.setFont(f); p.setPen(QColor(150,160,190));
        p.drawText(QRect(cx-radio, cy-radio, radio*2, radio*2), Qt::AlignCenter, "?");
        return;
    }

    // ── Color según nivel ────────────────────────────────────────────────
    QColor c1, c2;
    if (cargando) {
        // Azul cian animado cuando carga
        c1 = QColor(40, 160, 255);
        c2 = QColor(100, 220, 255);
    } else if (porcentaje > 0.60) {
        c1 = QColor(40, 200, 100);
        c2 = QColor(80, 240, 140);
    } else if (porcentaje > 0.30) {
        c1 = QColor(240, 170, 30);
        c2 = QColor(255, 210, 70);
    } else {
        // Rojo pulsante cuando está bajo
        c1 = QColor(220, 50, 40);
        c2 = QColor(255, 90, 70);
    }

    // ── Gradiente cónico ─────────────────────────────────────────────────
    // El gradiente gira suavemente cuando carga (animOffset 0..1)
    qreal startDeg = -90.0 + (cargando ? animOffset * 360.0 : 0.0);
    QConicalGradient grad(cx, cy, startDeg);
    grad.setColorAt(0.0, c1);
    grad.setColorAt(0.4, c2);
    grad.setColorAt(0.7, c1);
    grad.setColorAt(1.0, c1);

    // ── Arco de progreso ─────────────────────────────────────────────────
    // Qt: ángulo 0 = derecha (3 en punto), positivo = antihorario
    // Queremos empezar desde arriba (12 en punto = 90°) en sentido horario (negativo)
    QPen penArc(QBrush(grad), 5.5, Qt::SolidLine, Qt::RoundCap);
    p.setPen(penArc);

    int startAngle16 = 90 * 16;
    int spanAngle16  = -(int)(porcentaje * 360.0 * 16.0);
    // Mínimo visible: al menos 5° de arco para que se vea aunque sea 1%
    if (porcentaje > 0.001 && qAbs(spanAngle16) < 5*16)
        spanAngle16 = -5*16;

    QRectF arcRect(cx - radio, cy - radio, radio*2.0, radio*2.0);
    p.drawArc(arcRect, startAngle16, spanAngle16);

    // ── Glow en el extremo del arco ──────────────────────────────────────
    if (porcentaje > 0.01) {
        // Ángulo del extremo en radianes (sentido horario desde arriba)
        qreal endDeg = -90.0 + porcentaje * 360.0;
        qreal endRad = endDeg * M_PI / 180.0;
        qreal ex = cx + radio * cos(endRad);
        qreal ey = cy + radio * sin(endRad);

        QRadialGradient glow(ex, ey, radio * 0.28);
        glow.setColorAt(0, QColor(c2.red(), c2.green(), c2.blue(), 180));
        glow.setColorAt(1, QColor(0, 0, 0, 0));
        p.setBrush(glow); p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(ex, ey), radio * 0.28, radio * 0.28);
    }

    // ── Texto en el centro ───────────────────────────────────────────────
    QRect textRect(cx - radio + 4, cy - radio + 4, radio*2 - 8, radio*2 - 8);

    if (cargando) {
        // Ícono de rayo ⚡
        QFont fL; fL.setFamily("Noto Sans"); fL.setPixelSize(qMax(8, radio * 3/5));
        p.setFont(fL);
        p.setPen(QColor(80, 200, 255, 230));
        p.drawText(textRect, Qt::AlignCenter, "⚡");
    } else {
        // Porcentaje grande + símbolo % pequeño
        int pct = qRound(porcentaje * 100.0);
        QString numStr = QString::number(pct);

        // Tamaño de fuente adaptable al espacio
        int fontSize = (pct >= 100) ? qMax(7, radio * 3/8)
                     : (pct >= 10)  ? qMax(8, radio * 2/5)
                                    : qMax(9, radio * 9/16);
        QFont fNum; fNum.setFamily("Noto Sans"); fNum.setPixelSize(fontSize); fNum.setBold(true);
        p.setFont(fNum);
        p.setPen(porcentaje <= 0.15 ? QColor(255,120,100) : QColor(215, 220, 235));

        // Dibujar número centrado un poco arriba del centro
        QRect numRect(cx - radio + 4, cy - radio + 4, radio*2 - 8, radio*2 - 4);
        p.drawText(numRect, Qt::AlignHCenter | Qt::AlignVCenter, numStr);

        // Símbolo % muy pequeño debajo del número
        QFont fPct; fPct.setFamily("Noto Sans"); fPct.setPixelSize(qMax(6, radio/5));
        p.setFont(fPct);
        p.setPen(QColor(130, 140, 170));
        QRect pctRect(cx - radio, cy + radio/3, radio*2, radio/3 + 2);
        p.drawText(pctRect, Qt::AlignHCenter | Qt::AlignTop, "%");
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// PAINT — paintEvent
// ═══════════════════════════════════════════════════════════════════════════

void DockItem::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing|QPainter::SmoothPixmapTransform|QPainter::TextAntialiasing);

    switch (m_tipoEsp) {
    case TipoEspecial::Separador:     paintSeparador(p);     return;
    case TipoEspecial::BotonApps:     paintBotonApps(p);     break;
    case TipoEspecial::BotonMenu:     paintBotonMenu(p);     break;
    case TipoEspecial::Papelera:      paintPapelera(p);      break;
    case TipoEspecial::Reloj:         paintReloj(p);         break;
    case TipoEspecial::Bateria:       paintBateria(p);       break;
    case TipoEspecial::Red:           paintRed(p);           break;
    case TipoEspecial::Notificaciones:paintNotificaciones(p);break;
    default:                          paintApp(p);           break;
    }

    if (m_mostrarEtiqueta && m_hover && m_tipoEsp!=TipoEspecial::Separador
        && m_dock && m_dock->config().mostrarEtiquetas)
    {
        QString label;
        switch(m_tipoEsp) {
        case TipoEspecial::BotonApps:      label="Aplicaciones"; break;
        case TipoEspecial::Papelera:       label="Papelera"; break;
        case TipoEspecial::Bateria:        label=m_nivelBateria>=0
            ? QString("Batería: %1%%").arg(m_nivelBateria) : "Sin batería"; break;
        case TipoEspecial::Red:            label=m_redConectada?"Red: "+m_nombreRed:"Sin conexión"; break;
        case TipoEspecial::Notificaciones: label=m_notifCount>0
            ? QString("%1 notificaciones").arg(m_notifCount) : "Sin notificaciones"; break;
        default:                           label=m_app.nombre; break;
        }
        dibujarEtiqueta(p, label);
    }
}

void DockItem::paintApp(QPainter& p) {
    bool h=isHoriz(m_dock);
    int sz=(int)(tamanoActual()*m_escala);
    int cx=width()/2, cy=height()/2;
    if(h) cy+=(int)m_rebote; else cx+=(int)m_rebote;
    QRect rc(cx-sz/2, cy-sz/2, sz, sz);

    // Anillo activo
    if (m_activa) {
        p.save();
        QPen pen(QColor(80,180,255,130),2);
        p.setPen(pen); p.setBrush(Qt::NoBrush);
        QPainterPath ring; ring.addRoundedRect(rc.adjusted(-3,-3,3,3),sz*0.25+3,sz*0.25+3);
        p.drawPath(ring);
        p.restore();
    }

    // Hover
    if (m_hover&&!m_activa) {
        QPainterPath hl; hl.addRoundedRect(rc.adjusted(-2,-2,2,2),sz*0.23,sz*0.23);
        p.fillPath(hl,QColor(255,255,255,22));
    }

    // Brillo pulso
    if (m_brillo>0.01) {
        QPainterPath gl; gl.addRoundedRect(rc,sz*0.22,sz*0.22);
        p.fillPath(gl,QColor(255,255,255,(int)(m_brillo*60)));
    }

    // Sombra
    if (sz>20 && !m_app.icono.isNull()) {
        p.save(); p.setOpacity(0.18);
        p.drawPixmap(rc.translated(0,4), m_app.icono.pixmap(sz,sz));
        p.setOpacity(1.0); p.restore();
    }

    // Ícono
    if (!m_app.icono.isNull()) {
        dibujarIconoRedondeado(p,rc,m_app.icono,sz);
    } else {
        QLinearGradient g(rc.topLeft(),rc.bottomRight());
        g.setColorAt(0,QColor(60,90,160)); g.setColorAt(1,QColor(30,50,110));
        QPainterPath ip; ip.addRoundedRect(rc,sz*0.22,sz*0.22);
        p.fillPath(ip,g);
        p.setPen(QColor(255,255,255,200)); QFont f; f.setPixelSize(sz/3); f.setBold(true); p.setFont(f);
        p.drawText(rc,Qt::AlignCenter,m_app.nombre.left(1).toUpper());
    }

    if (m_dock&&m_dock->config().mostrarIndicadores&&m_app.estaEjecutando)
        dibujarIndicadores(p,rc);
}

void DockItem::paintBotonApps(QPainter& p) {
    int sz=(int)(tamanoActual()*m_escala);
    int cx=width()/2, cy=height()/2;
    QRect rc(cx-sz/2,cy-sz/2,sz,sz);
    QPainterPath path; path.addRoundedRect(rc,sz*0.22,sz*0.22);
    QLinearGradient grad(rc.topLeft(),rc.bottomRight());
    if (m_hover) { grad.setColorAt(0,QColor(70,130,240,235)); grad.setColorAt(0.5,QColor(90,80,225,235)); grad.setColorAt(1,QColor(50,160,220,235)); }
    else         { grad.setColorAt(0,QColor(50,100,200,215)); grad.setColorAt(0.5,QColor(70,60,190,215)); grad.setColorAt(1,QColor(40,130,200,215)); }
    p.fillPath(path,grad);
    p.setPen(QPen(QColor(255,255,255,60),1.2)); p.drawPath(path);
    int dot=sz/10, gap=sz/5;
    int startX=rc.x()+(sz-2*gap-dot)/2, startY=rc.y()+(sz-2*gap-dot)/2;
    p.setPen(Qt::NoPen);
    for(int r=0;r<3;r++) for(int c=0;c<3;c++) {
        p.setBrush(QColor(255,255,255,200));
        p.drawRoundedRect(startX+c*gap,startY+r*gap,dot,dot,dot*0.3,dot*0.3);
    }
}

void DockItem::paintBotonMenu(QPainter& p) {
    int sz = (int)(tamanoActual() * m_escala);
    int cx = width()/2, cy = height()/2;
    QRect rc(cx-sz/2, cy-sz/2, sz, sz);

    // Fondo con gradiente violeta-azul profundo
    QPainterPath path; path.addRoundedRect(rc, sz*0.22, sz*0.22);
    QLinearGradient grad(rc.topLeft(), rc.bottomRight());
    if (m_hover) {
        grad.setColorAt(0, QColor(100, 70, 220, 235));
        grad.setColorAt(1, QColor(60, 110, 240, 235));
    } else {
        grad.setColorAt(0, QColor(75, 50, 190, 215));
        grad.setColorAt(1, QColor(45, 85, 210, 215));
    }
    p.fillPath(path, grad);
    p.setPen(QPen(QColor(180, 160, 255, 60), 1.2)); p.drawPath(path);

    // Brillo superior
    QPainterPath shine;
    shine.addRoundedRect(QRectF(rc.x()+1, rc.y()+1, rc.width()-2, rc.height()/2.2), sz*0.21, sz*0.21);
    QLinearGradient sg(rc.topLeft(), QPoint(rc.left(), rc.top()+rc.height()/2));
    sg.setColorAt(0, QColor(255,255,255,32)); sg.setColorAt(1, QColor(255,255,255,0));
    p.fillPath(shine, sg);

    // Icono: tres líneas horizontales (hamburger) estilizado
    int lw = sz*5/10, lh = 2, gap = sz/9;
    int lx = cx - lw/2;
    int ly = cy - gap - lh;  // línea superior
    p.setBrush(QColor(220, 225, 255, 220)); p.setPen(Qt::NoPen);

    for (int i = 0; i < 3; i++) {
        int lineW = (i==1) ? lw : lw*8/10;  // línea del medio más corta
        int lineX = (i==1) ? lx + (lw-lineW)/2 : lx;
        QRectF line(lineX, ly + i*(gap+lh), lineW, lh+1);
        QPainterPath lp; lp.addRoundedRect(line, 1, 1);
        p.drawPath(lp);
    }
}

void DockItem::paintPapelera(QPainter& p) {
    int sz=(int)(tamanoActual()*m_escala);
    int cx=width()/2, cy=height()/2;
    QRect rc(cx-sz/2,cy-sz/2,sz,sz);
    QPainterPath path; path.addRoundedRect(rc,sz*0.22,sz*0.22);
    p.fillPath(path, m_hover?QColor(80,80,90,210):QColor(55,55,65,185));
    p.setPen(QPen(QColor(255,255,255,40),1)); p.drawPath(path);
    QIcon ti=QIcon::fromTheme("user-trash-full"); if(ti.isNull()) ti=QIcon::fromTheme("user-trash");
    if (!ti.isNull()) { int is=sz*7/10; p.drawPixmap(QRect(cx-is/2,cy-is/2,is,is),ti.pixmap(is,is)); }
}

void DockItem::paintSeparador(QPainter& p) {
    bool h=isHoriz(m_dock);
    if(h) {
        QLinearGradient g(0,height()/2,width(),height()/2);
        g.setColorAt(0,QColor(255,255,255,0)); g.setColorAt(0.3,QColor(255,255,255,55));
        g.setColorAt(0.7,QColor(255,255,255,55)); g.setColorAt(1,QColor(255,255,255,0));
        p.setPen(QPen(QBrush(g),1.2)); p.drawLine(0,height()/2,width(),height()/2);
    } else {
        QLinearGradient g(width()/2,0,width()/2,height());
        g.setColorAt(0,QColor(255,255,255,0)); g.setColorAt(0.3,QColor(255,255,255,55));
        g.setColorAt(0.7,QColor(255,255,255,55)); g.setColorAt(1,QColor(255,255,255,0));
        p.setPen(QPen(QBrush(g),1.2)); p.drawLine(width()/2,0,width()/2,height());
    }
}

void DockItem::paintReloj(QPainter& p) {
    QDateTime now=QDateTime::currentDateTime();
    QString hora=now.toString("HH:mm"), fecha=now.toString("ddd d");
    int cy=height()/2;

    QFont fH; fH.setFamily("Noto Sans"); fH.setPixelSize(15); fH.setBold(true);
    p.setFont(fH); p.setPen(QColor(230,235,248));
    p.drawText(QRect(0,cy-18,width(),20),Qt::AlignHCenter,hora);

    QFont fD; fD.setFamily("Noto Sans"); fD.setPixelSize(10);
    p.setFont(fD); p.setPen(QColor(160,170,200));
    p.drawText(QRect(0,cy+4,width(),16),Qt::AlignHCenter,fecha);
}

void DockItem::paintBateria(QPainter& p) {
    int sz    = tamanoActual();
    int cx    = width()/2;
    int cy    = height()/2;
    int radio = qMax(10, sz/2 - 6);

    // Usar aroBateria (animado) si el nivel ya se conoce
    qreal nivel = (m_nivelBateria >= 0) ? m_aroBateria : -1.0;

    // Tiempo de animación para el pulso de carga
    static qreal cargaOffset = 0.0;
    if (m_cargando) {
        cargaOffset = fmod(cargaOffset + 0.004, 1.0);
        // Forzar repaint para la animación
        QTimer::singleShot(16, this, [this]{ if(m_cargando) update(); });
    }

    paintAroBateria(p, cx, cy, radio, nivel, m_cargando, cargaOffset);
}

void DockItem::paintRed(QPainter& p) {
    int sz=(int)(tamanoActual()*m_escala);
    int cx=width()/2, cy=height()/2;
    QRect rc(cx-sz/2,cy-sz/2,sz,sz);

    // Fondo con color indicador
    QPainterPath path; path.addRoundedRect(rc,sz*0.22,sz*0.22);
    QColor bgColor = m_redConectada
        ? (m_hover ? QColor(40,120,70,200) : QColor(30,100,60,180))
        : (m_hover ? QColor(100,30,30,200) : QColor(80,25,25,180));
    p.fillPath(path, bgColor);
    p.setPen(QPen(m_redConectada?QColor(60,200,100,80):QColor(200,60,60,80),1));
    p.drawPath(path);

    // Ícono
    QString iconName = m_redConectada ? "network-wireless" : "network-offline";
    QIcon icon = QIcon::fromTheme(iconName);
    if (icon.isNull()) icon = QIcon::fromTheme(m_redConectada?"network-wired":"network-error");
    if (!icon.isNull()) {
        int is=sz*6/10;
        p.drawPixmap(QRect(cx-is/2,cy-is/2-4,is,is),icon.pixmap(is,is));
    }

    // Estado textual abajo
    QFont f; f.setFamily("Noto Sans"); f.setPixelSize(9); f.setBold(true);
    p.setFont(f);
    p.setPen(m_redConectada?QColor(80,220,130):QColor(220,80,80));
    p.drawText(QRect(0,cy+sz/2-8,width(),16),Qt::AlignHCenter,
               m_redConectada?"en línea":"sin red");
}

void DockItem::paintNotificaciones(QPainter& p) {
    // Este ítem solo se muestra cuando m_notifCount > 0
    // (DockWindow lo oculta cuando es 0)
    int sz = (int)(tamanoActual() * m_escala);
    int cx = width()/2, cy = height()/2;
    QRect rc(cx - sz/2, cy - sz/2, sz, sz);

    // ── Fondo con gradiente azul-púrpura ────────────────────────────────
    QPainterPath path; path.addRoundedRect(rc, sz*0.22, sz*0.22);

    QLinearGradient bg(rc.topLeft(), rc.bottomRight());
    if (m_hover) {
        bg.setColorAt(0, QColor(80, 120, 240, 230));
        bg.setColorAt(1, QColor(120, 60, 220, 230));
    } else {
        bg.setColorAt(0, QColor(55, 90, 210, 215));
        bg.setColorAt(1, QColor(90, 45, 185, 215));
    }
    p.fillPath(path, bg);

    // Borde brillante
    p.setPen(QPen(QColor(140, 160, 255, 70), 1.2));
    p.drawPath(path);

    // Brillo interior superior
    QPainterPath shine;
    shine.addRoundedRect(QRectF(rc.x()+1, rc.y()+1, rc.width()-2, rc.height()/2.2),
                         sz*0.21, sz*0.21);
    QLinearGradient sg(rc.topLeft(), QPoint(rc.left(), rc.top()+rc.height()/2));
    sg.setColorAt(0, QColor(255,255,255,30)); sg.setColorAt(1, QColor(255,255,255,0));
    p.fillPath(shine, sg);

    // ── Ícono de campana ─────────────────────────────────────────────────
    int is = sz * 6 / 10;
    QIcon bellIcon = QIcon::fromTheme("notification-new",
                     QIcon::fromTheme("appointment-new",
                     QIcon::fromTheme("mail-unread")));
    if (!bellIcon.isNull()) {
        p.drawPixmap(QRect(cx-is/2, cy-is/2, is, is), bellIcon.pixmap(is, is));
    } else {
        // Dibujar campana vectorial simple
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(220, 228, 255, 230));

        // Cuerpo de campana
        int bw = sz*4/10, bh = sz*4/10;
        int bx = cx - bw/2, by = cy - bh/2 - 2;
        QPainterPath bell;
        bell.moveTo(cx, by);
        bell.cubicTo(cx + bw/2, by, cx + bw/2, by + bh*3/4, cx + bw/2, by + bh);
        bell.lineTo(cx - bw/2, by + bh);
        bell.cubicTo(cx - bw/2, by + bh*3/4, cx - bw/2, by, cx, by);
        p.drawPath(bell);

        // Base
        p.drawRoundedRect(cx - bw/2 - 1, by + bh, bw+2, 3, 1, 1);
        // Badajo
        p.drawEllipse(cx-3, by+bh+2, 6, 5);
    }

    // ── Badge de conteo (esquina superior derecha) ───────────────────────
    int bsz = sz >= 40 ? 18 : 15;
    int bx  = rc.right()  - bsz/2 + 2;
    int by  = rc.top()    - bsz/2 + 2;
    QRect brc(bx, by, bsz, bsz);

    // Glow rojo
    QRadialGradient glow(brc.center(), bsz*0.8);
    glow.setColorAt(0, QColor(255, 80, 60, 80));
    glow.setColorAt(1, QColor(255, 80, 60, 0));
    p.setBrush(glow); p.setPen(Qt::NoPen);
    p.drawEllipse(brc.adjusted(-3,-3,3,3));

    // Círculo rojo
    QPainterPath badge; badge.addEllipse(brc);
    QRadialGradient badgeGrad(brc.center(), bsz/2.0);
    badgeGrad.setColorAt(0, QColor(255, 75, 55));
    badgeGrad.setColorAt(1, QColor(210, 30, 20));
    p.fillPath(badge, badgeGrad);

    // Número
    QFont fN; fN.setFamily("Noto Sans");
    fN.setPixelSize(m_notifCount>9 ? qMax(7,bsz*5/9) : qMax(8,bsz*6/9));
    fN.setBold(true);
    p.setFont(fN); p.setPen(Qt::white);
    QString txt = m_notifCount > 99 ? "99+" : QString::number(m_notifCount);
    p.drawText(brc, Qt::AlignCenter, txt);
}

// ─── Events ───────────────────────────────────────────────────────────────

void DockItem::mousePressEvent(QMouseEvent* e) {
    if (e->button()==Qt::LeftButton) {
        m_presion=true;
        m_animEscala->stop(); m_animEscala->setStartValue(m_escala);
        m_animEscala->setEndValue(0.88); m_animEscala->start();
    } else if (e->button()==Qt::MiddleButton) emit clicMedio();
    QWidget::mousePressEvent(e);
}

void DockItem::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button()==Qt::LeftButton&&m_presion) {
        m_presion=false;
        m_animEscala->stop(); m_animEscala->setStartValue(m_escala);
        m_animEscala->setEndValue(m_hover?1.07:1.0); m_animEscala->start();
        if (rect().contains(e->pos())) { animarPulso(); emit clicIzquierdo(); }
    } else if (e->button()==Qt::RightButton) emit clicDerecho(e->globalPosition().toPoint());
    QWidget::mouseReleaseEvent(e);
}

void DockItem::enterEvent(QEnterEvent* e) {
    m_hover=true; m_timerEtiq->start();
    m_animEscala->stop(); m_animEscala->setStartValue(m_escala);
    m_animEscala->setEndValue(1.07); m_animEscala->start();
    update(); QWidget::enterEvent(e);
}

void DockItem::leaveEvent(QEvent* e) {
    m_hover=false; m_mostrarEtiqueta=false; m_timerEtiq->stop();
    m_animEscala->stop(); m_animEscala->setStartValue(m_escala);
    m_animEscala->setEndValue(1.0); m_animEscala->start();
    update(); QWidget::leaveEvent(e);
}
