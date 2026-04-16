#include "SystemPanel.h"
#include "DockWindow.h"
#include "NotificationManager.h"
#include "NetworkPanel.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QConicalGradient>
#include <QScreen>
#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QFont>
#include <QFontMetrics>
#include <QScrollArea>
#include <QGroupBox>
#include <functional>
#include <cmath>

// ─── Estilo base del panel ────────────────────────────────────────────────
static const QString CSS_PANEL = R"(
QWidget { background: transparent; color: #e8eaf4; }
QLabel  { color: #c8cce0; font-family: 'Noto Sans'; font-size: 12px; background: transparent; }
QLabel#valor { color: #e8eaf4; font-size: 13px; font-weight: bold; }
QSlider::groove:horizontal {
    height: 4px; background: rgba(255,255,255,22); border-radius: 2px;
}
QSlider::handle:horizontal {
    background: white; width: 14px; height: 14px;
    border-radius: 7px; margin: -5px 0;
}
QSlider::sub-page:horizontal { background: #4a9eff; border-radius: 2px; }
QPushButton#control {
    background: rgba(255,255,255,12);
    border: 1px solid rgba(255,255,255,25);
    border-radius: 12px;
    color: #d8dcf0;
    font-family: 'Noto Sans';
    font-size: 12px;
    padding: 8px 14px;
    text-align: left;
}
QPushButton#control:hover {
    background: rgba(255,255,255,22);
    border-color: rgba(255,255,255,50);
    color: white;
}
QPushButton#control:pressed { background: rgba(255,255,255,10); }
QPushButton#power {
    background: rgba(200,60,60,30);
    border: 1px solid rgba(200,60,60,60);
    border-radius: 10px;
    color: #ff8888;
    font-family: 'Noto Sans';
    font-size: 12px;
    padding: 7px 16px;
}
QPushButton#power:hover {
    background: rgba(220,70,70,60);
    color: white;
}
QPushButton#sysaction {
    background: rgba(255,255,255,10);
    border: 1px solid rgba(255,255,255,20);
    border-radius: 10px;
    color: #c0c8e0;
    font-family: 'Noto Sans';
    font-size: 11px;
    padding: 7px 0;
    min-width: 70px;
}
QPushButton#sysaction:hover {
    background: rgba(255,255,255,22);
    color: white;
}
)";

// ═══════════════════════════════════════════════════════════════════════════
// SystemPanel
// ═══════════════════════════════════════════════════════════════════════════

SystemPanel::SystemPanel(DockWindow* dock, QWidget* parent)
    : QWidget(parent), m_dock(dock)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                   Qt::WindowStaysOnTopHint | Qt::Tool |
                   Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setStyleSheet(CSS_PANEL);
    setFixedWidth(290);

    m_animOpacidad = new QPropertyAnimation(this, "opacidad", this);
    m_animOpacidad->setDuration(220);
    m_animOpacidad->setEasingCurve(QEasingCurve::OutCubic);

    m_animSlide = new QPropertyAnimation(this, "slideY", this);
    m_animSlide->setDuration(240);
    m_animSlide->setEasingCurve(QEasingCurve::OutCubic);

    // Timer para ignorar clics externos al abrir el panel
    m_timerIgnorarClics = new QTimer(this);
    m_timerIgnorarClics->setSingleShot(true);
    m_timerIgnorarClics->setInterval(200);
    connect(m_timerIgnorarClics, &QTimer::timeout, this, [this]{
        m_ignorarClicsTemporales = false;
    });

    // Actualizar datos del sistema cada 5s
    m_timerUpdate = new QTimer(this);
    m_timerUpdate->setInterval(5000);
    connect(m_timerUpdate, &QTimer::timeout, this, [this]{
        // Re-leer batería
        auto readSys = [](const QString& suf) -> QString {
            for (auto bat : {"BAT0","BAT1","BAT2"}) {
                QFile f(QString("/sys/class/power_supply/%1/%2").arg(bat,suf));
                if (f.open(QIODevice::ReadOnly)) return QString::fromUtf8(f.readAll()).trimmed();
            }
            return {};
        };
        QString cap = readSys("capacity");
        QString st  = readSys("status");
        if (!cap.isEmpty()) {
            m_nivelBateria = cap.toInt();
            m_cargando = (st=="Charging"||st=="Full");
            actualizarBateria(m_nivelBateria, m_cargando);
        }
    });

    construirUI();
    qApp->installEventFilter(this);
    hide();
}

// ─── UI ───────────────────────────────────────────────────────────────────

void SystemPanel::construirUI() {
    auto* outerL = new QVBoxLayout(this);
    outerL->setContentsMargins(10, 10, 10, 10);
    outerL->setSpacing(0);

    auto* inner = new QWidget;
    inner->setAttribute(Qt::WA_TranslucentBackground);
    auto* L = new QVBoxLayout(inner);
    L->setContentsMargins(16, 16, 16, 16);
    L->setSpacing(14);

    // ─ Estado del sistema (red, batería, volumen) ─────────────────────
    L->addWidget(crearSeccionEstado());

    // ─ Separador visual ──────────────────────────────────────────────
    auto* sep1 = new QWidget; sep1->setFixedHeight(1);
    sep1->setStyleSheet("background: rgba(255,255,255,18); border-radius: 1px;");
    L->addWidget(sep1);

    // ─ Acciones rápidas ──────────────────────────────────────────────
    L->addWidget(crearSeccionAcciones());

    // ─ Separador ─────────────────────────────────────────────────────
    auto* sep2 = new QWidget; sep2->setFixedHeight(1);
    sep2->setStyleSheet("background: rgba(255,255,255,18); border-radius: 1px;");
    L->addWidget(sep2);

    // ─ Botones de apagado ────────────────────────────────────────────
    L->addWidget(crearSeccionApagado());

    outerL->addWidget(inner);

    // Calcular altura total dinámica
    int h = 16+16 + 220 + 1+14 + 160 + 1+14 + 180 + 14;
    setFixedHeight(h + 20);
}

QWidget* SystemPanel::crearSeccionEstado() {
    auto* w = new QWidget;
    w->setAttribute(Qt::WA_TranslucentBackground);
    auto* L = new QVBoxLayout(w);
    L->setContentsMargins(0,0,0,0); L->setSpacing(12);

    // Título de sección
    auto* tit = new QLabel("Estado del sistema");
    tit->setStyleSheet("color:#6878a0;font-size:10px;font-family:'Noto Sans';"
                       "text-transform:uppercase;letter-spacing:0.5px;");
    L->addWidget(tit);

    // ── Red ──────────────────────────────────────────────────────────
    {
        m_btnRed = new QPushButton;
        m_btnRed->setObjectName("control");
        m_btnRed->setFixedHeight(48);
        connect(m_btnRed, &QPushButton::clicked, this, [this]{
            if (m_netPanel) {
                QPoint anchor = m_btnRed->mapToGlobal(m_btnRed->rect().center());
                m_netPanel->toggle(anchor);
            }
        });
        L->addWidget(m_btnRed);
        actualizarRed(false, "");
    }

    // ── Batería ──────────────────────────────────────────────────────
    {
        m_btnBateria = new QPushButton;
        m_btnBateria->setObjectName("control");
        m_btnBateria->setFixedHeight(48);
        L->addWidget(m_btnBateria);
        actualizarBateria(-1, false);
    }

    // ── Volumen ──────────────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout; row->setSpacing(10);
        auto* lblV = new QLabel("🔊");
        lblV->setFixedWidth(22);
        lblV->setStyleSheet("font-size:15px;");
        m_sliderVolumen = new QSlider(Qt::Horizontal);
        m_sliderVolumen->setRange(0, 100);
        m_sliderVolumen->setValue(m_nivelVolumen);
        m_lblVolumen = new QLabel(QString("%1%").arg(m_nivelVolumen));
        m_lblVolumen->setObjectName("valor");
        m_lblVolumen->setFixedWidth(36);
        m_lblVolumen->setAlignment(Qt::AlignRight|Qt::AlignVCenter);

        connect(m_sliderVolumen, &QSlider::valueChanged, [this](int v){
            m_nivelVolumen = v;
            m_lblVolumen->setText(QString("%1%").arg(v));
            // Cambiar volumen via pactl (PulseAudio/PipeWire)
            QProcess::startDetached("pactl",
                {"set-sink-volume","@DEFAULT_SINK@",
                 QString("%1%").arg(v)});
        });
        row->addWidget(lblV); row->addWidget(m_sliderVolumen); row->addWidget(m_lblVolumen);
        auto* rowW = new QWidget; rowW->setAttribute(Qt::WA_TranslucentBackground);
        rowW->setLayout(row);
        L->addWidget(rowW);
    }

    // ── Notificaciones ───────────────────────────────────────────────
    {
        m_btnNotif = new QPushButton;
        m_btnNotif->setObjectName("control");
        m_btnNotif->setFixedHeight(44);
        connect(m_btnNotif, &QPushButton::clicked, this, [this]{
            if (m_notifMgr) m_notifMgr->marcarLeidas();
            actualizarNotificaciones(0);
        });
        L->addWidget(m_btnNotif);
        actualizarNotificaciones(0);
    }

    return w;
}

QWidget* SystemPanel::crearSeccionAcciones() {
    auto* w = new QWidget;
    w->setAttribute(Qt::WA_TranslucentBackground);
    auto* L = new QVBoxLayout(w);
    L->setContentsMargins(0,0,0,0); L->setSpacing(6);

    auto* tit = new QLabel("Acciones");
    tit->setStyleSheet("color:#6878a0;font-size:10px;font-family:'Noto Sans';"
                       "text-transform:uppercase;letter-spacing:0.5px;");
    L->addWidget(tit);

    // Fila 1
    auto* r1 = new QHBoxLayout; r1->setSpacing(6);

    auto* btnPref = new QPushButton("⚙  Preferencias");
    btnPref->setObjectName("control"); btnPref->setFixedHeight(36);
    connect(btnPref, &QPushButton::clicked, this, [this]{
        ocultar();
        emit pedirPreferencias();
    });
    r1->addWidget(btnPref);

    auto* btnApps = new QPushButton("🔍  Buscar apps");
    btnApps->setObjectName("control"); btnApps->setFixedHeight(36);
    connect(btnApps, &QPushButton::clicked, this, [this]{
        ocultar();
        // El AppMenu se abre desde DockWindow al recibir pedirPreferencias con flag especial
        // o podemos emitir una señal diferente — por ahora usa xdg-open
        QProcess::startDetached("bash",{"-c",
            "gnome-shell --replace &>/dev/null || krunner &>/dev/null || rofi -show drun &>/dev/null &"});
    });
    r1->addWidget(btnApps);

    auto* r1w = new QWidget; r1w->setAttribute(Qt::WA_TranslucentBackground); r1w->setLayout(r1);
    L->addWidget(r1w);

    // Fila 2: Más opciones
    auto* r2 = new QHBoxLayout; r2->setSpacing(6);

    auto* btnFiles = new QPushButton("📁  Archivos");
    btnFiles->setObjectName("control"); btnFiles->setFixedHeight(36);
    connect(btnFiles, &QPushButton::clicked, [this]{
        ocultar();
        QProcess::startDetached("xdg-open",{QDir::homePath()});
    });
    r2->addWidget(btnFiles);

    auto* btnTerm = new QPushButton("⬛  Terminal");
    btnTerm->setObjectName("control"); btnTerm->setFixedHeight(36);
    connect(btnTerm, &QPushButton::clicked, [this]{
        ocultar();
        QProcess::startDetached("bash",{"-c",
            "x-terminal-emulator || gnome-terminal || konsole || xterm"});
    });
    r2->addWidget(btnTerm);

    auto* r2w = new QWidget; r2w->setAttribute(Qt::WA_TranslucentBackground); r2w->setLayout(r2);
    L->addWidget(r2w);

    return w;
}

QWidget* SystemPanel::crearSeccionApagado() {
    auto* w = new QWidget;
    w->setAttribute(Qt::WA_TranslucentBackground);
    auto* L = new QVBoxLayout(w);
    L->setContentsMargins(0,0,0,0); L->setSpacing(8);

    auto* tit = new QLabel("Sesión");
    tit->setStyleSheet("color:#6878a0;font-size:10px;font-family:'Noto Sans';"
                       "text-transform:uppercase;letter-spacing:0.5px;");
    L->addWidget(tit);

    struct BtnDef { QString icono; QString texto; QString cmd; bool esPower; };
    QList<BtnDef> btns = {
        {"🔒", "Bloquear",   "bash -c 'loginctl lock-session || xdg-screensaver lock || gnome-screensaver-command -l'", false},
        {"💤", "Suspender",  "systemctl suspend",   false},
        {"↩", "Cerrar",     "bash -c 'loginctl terminate-session $XDG_SESSION_ID || gnome-session-quit --logout'", false},
        {"↺", "Reiniciar",  "systemctl reboot",    true},
        {"⏻", "Apagar",     "systemctl poweroff",  true},
    };

    // Primera fila: 3 botones (Bloquear, Suspender, Cerrar)
    auto* fila1 = new QHBoxLayout; fila1->setSpacing(6);
    for (int i = 0; i < 3; ++i) {
        const BtnDef& bd = btns[i];
        auto* btn = new QPushButton(QString("%1\n%2").arg(bd.icono, bd.texto));
        btn->setObjectName("sysaction");
        btn->setFixedHeight(56);
        QString cmd = bd.cmd;
        connect(btn, &QPushButton::clicked, [this, cmd]{
            ocultar();
            QProcess::startDetached("bash", {"-c", cmd});
        });
        fila1->addWidget(btn);
    }
    auto* fila1W = new QWidget; fila1W->setAttribute(Qt::WA_TranslucentBackground);
    fila1W->setLayout(fila1);
    L->addWidget(fila1W);

    // Segunda fila: 2 botones (Reiniciar, Apagar) - más grandes
    auto* fila2 = new QHBoxLayout; fila2->setSpacing(6);
    for (int i = 3; i < 5; ++i) {
        const BtnDef& bd = btns[i];
        auto* btn = new QPushButton(QString("%1\n%2").arg(bd.icono, bd.texto));
        btn->setObjectName("power");
        btn->setFixedHeight(56);
        QString cmd = bd.cmd;
        connect(btn, &QPushButton::clicked, [this, cmd]{
            ocultar();
            QProcess::startDetached("bash", {"-c", cmd});
        });
        fila2->addWidget(btn);
    }
    auto* fila2W = new QWidget; fila2W->setAttribute(Qt::WA_TranslucentBackground);
    fila2W->setLayout(fila2);
    L->addWidget(fila2W);

    return w;
}

// ─── Actualizar estado en tiempo real ────────────────────────────────────

void SystemPanel::actualizarBateria(int nivel, bool cargando) {
    m_nivelBateria = nivel;
    m_cargando     = cargando;
    if (!m_btnBateria) return;

    QString icon, texto, sub;
    if (nivel < 0) {
        icon  = "🔌";
        texto = "Sin batería";
        sub   = "Corriente alterna";
    } else {
        icon  = nivel > 80 ? "🔋" : nivel > 30 ? "🔋" : "🪫";
        if (cargando) icon = "⚡";
        texto = QString("Batería: %1%").arg(nivel);
        sub   = cargando ? "Cargando…" :
                nivel > 80 ? "Carga completa" :
                nivel > 20 ? "Batería correcta" : "⚠ Batería baja";
    }

    m_btnBateria->setText(QString("%1  %2\n   %3").arg(icon, texto, sub));
}

void SystemPanel::actualizarRed(bool conectada, const QString& nombre) {
    m_redConectada = conectada;
    m_nombreRed    = nombre;
    if (!m_btnRed) return;

    QString icon  = conectada ? "📶" : "⚠";
    QString texto = conectada ? nombre.isEmpty() ? "Red conectada" : nombre : "Sin conexión";
    QString sub   = conectada ? "Clic para cambiar red" : "Clic para conectar";
    m_btnRed->setText(QString("%1  %2\n   %3").arg(icon, texto, sub));
}

void SystemPanel::actualizarVolumen(int nivel) {
    m_nivelVolumen = nivel;
    if (m_sliderVolumen) m_sliderVolumen->setValue(nivel);
    if (m_lblVolumen) m_lblVolumen->setText(QString("%1%").arg(nivel));
}

void SystemPanel::actualizarNotificaciones(int n) {
    m_notifCount = n;
    if (!m_btnNotif) return;
    if (n == 0) {
        m_btnNotif->setText("🔕  Sin notificaciones pendientes");
        m_btnNotif->setEnabled(false);
    } else {
        m_btnNotif->setText(QString("🔔  %1 notificación%2 sin leer  —  Limpiar")
                            .arg(n).arg(n>1?"es":""));
        m_btnNotif->setEnabled(true);
    }
}

// ─── Visibilidad y posición ──────────────────────────────────────────────

void SystemPanel::setSlideY(int v) {
    m_slideY = v;
    reposicionar(m_anchorPos);
}

void SystemPanel::reposicionar(const QPoint& anchor) {
    m_anchorPos = anchor;
    QScreen* s  = QApplication::primaryScreen();
    QRect sg    = s->geometry();
    auto pos    = m_dock ? m_dock->config().posicion : DockPosition::Abajo;

    int x = anchor.x() - width()/2;
    int y;

    switch (pos) {
    case DockPosition::Abajo:
        y = anchor.y() - height() - 14 + m_slideY;
        break;
    case DockPosition::Arriba:
        y = anchor.y() + 14 - m_slideY;
        break;
    case DockPosition::Izquierda:
        x = anchor.x() + 14;
        y = anchor.y() - height()/2;
        break;
    case DockPosition::Derecha:
        x = anchor.x() - width() - 14;
        y = anchor.y() - height()/2;
        break;
    }

    x = qBound(sg.x()+8, x, sg.x()+sg.width()-width()-8);
    y = qBound(sg.y()+8, y, sg.y()+sg.height()-height()-8);
    move(x, y);
}

void SystemPanel::toggle(const QPoint& anchor) {
    m_visible ? ocultar() : mostrar(anchor);
}

void SystemPanel::mostrar(const QPoint& anchor) {
    m_visible = true;
    m_ignorarClicsTemporales = true;  // Ignorar clics externos temporalmente
    m_timerIgnorarClics->start();
    m_anchorPos = anchor;
    reposicionar(anchor);
    show(); raise();

    // Leer volumen actual
    QProcess* proc = new QProcess(this);
    proc->start("bash", {"-c",
        "pactl get-sink-volume @DEFAULT_SINK@ | grep -oP '\\d+%' | head -1 | tr -d '%'"});
    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            [this,proc](int,QProcess::ExitStatus){
        QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        if (!out.isEmpty()) actualizarVolumen(out.toInt());
        proc->deleteLater();
    });
    proc->start();

    m_animOpacidad->stop();
    m_animOpacidad->setStartValue(m_opacidad);
    m_animOpacidad->setEndValue(1.0);
    m_animOpacidad->start();

    int slideStart = (m_dock && m_dock->config().posicion == DockPosition::Abajo) ? 20 : -20;
    m_animSlide->stop();
    m_animSlide->setStartValue(slideStart);
    m_animSlide->setEndValue(0);
    m_animSlide->start();

    m_timerUpdate->start();
}

void SystemPanel::ocultar() {
    m_visible = false;
    m_timerUpdate->stop();

    m_animOpacidad->stop();
    m_animOpacidad->setStartValue(m_opacidad);
    m_animOpacidad->setEndValue(0.0);

    disconnect(m_animOpacidad, &QPropertyAnimation::finished, nullptr, nullptr);
    connect(m_animOpacidad, &QPropertyAnimation::finished, this, [this]{
        hide();
        disconnect(m_animOpacidad, &QPropertyAnimation::finished, nullptr, nullptr);
    });
    m_animOpacidad->start();
}

// ─── Pintar ───────────────────────────────────────────────────────────────

void SystemPanel::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRect rc = rect().adjusted(10, 10, -10, -10);
    QPainterPath path; path.addRoundedRect(rc, 20, 20);

    // Fondo oscuro profundo
    QLinearGradient bg(rc.topLeft(), rc.bottomLeft());
    bg.setColorAt(0, QColor(22, 24, 36, 248));
    bg.setColorAt(1, QColor(15, 17, 26, 252));
    p.fillPath(path, bg);

    // Borde aurora
    QLinearGradient border(rc.topLeft(), rc.bottomRight());
    border.setColorAt(0.0, QColor(80,  140, 255, 65));
    border.setColorAt(0.4, QColor(150,  80, 255, 45));
    border.setColorAt(0.8, QColor( 80, 200, 255, 60));
    border.setColorAt(1.0, QColor( 80, 140, 255, 65));
    p.setPen(QPen(QBrush(border), 1.4));
    p.drawPath(path);

    // Brillo superior (vidrio)
    QPainterPath shine;
    shine.addRoundedRect(QRectF(rc.x()+1, rc.y()+1, rc.width()-2, rc.height()/3.5), 19, 19);
    QLinearGradient sg(rc.topLeft(), QPoint(rc.left(), rc.top()+rc.height()/3));
    sg.setColorAt(0, QColor(255,255,255,22));
    sg.setColorAt(1, QColor(255,255,255,0));
    p.fillPath(shine, sg);
}

// ─── Eventos ─────────────────────────────────────────────────────────────

void SystemPanel::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) ocultar();
    else QWidget::keyPressEvent(e);
}

bool SystemPanel::eventFilter(QObject*, QEvent* e) {
    if (m_visible && !m_ignorarClicsTemporales && e->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(e);
        if (!geometry().contains(me->globalPosition().toPoint()))
            ocultar();
    }
    return false;
}
