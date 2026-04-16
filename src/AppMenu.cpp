#include "AppMenu.h"
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QProcess>
#include <QGraphicsOpacityEffect>
#include <algorithm>

// ═══════════════════════════════════════════════════════════════════════════
// AppMenuButton
// ═══════════════════════════════════════════════════════════════════════════

AppMenuButton::AppMenuButton(const InfoApp& app, QWidget* parent)
    : QWidget(parent), m_app(app)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(110, 110);
    setCursor(Qt::PointingHandCursor);

    m_anim = new QPropertyAnimation(this, "escala", this);
    m_anim->setDuration(120);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
}

void AppMenuButton::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);

    int cx = width()/2, cy = 42;
    int sz = (int)(48 * m_escala);
    QRect rc(cx-sz/2, cy-sz/2, sz, sz);

    // Fondo hover
    if (m_hover) {
        QPainterPath bg;
        bg.addRoundedRect(QRectF(4, 4, width()-8, height()-8), 14, 14);
        p.fillPath(bg, QColor(255,255,255,22));
    }

    // Ícono
    if (!m_app.icono.isNull()) {
        QPixmap px = m_app.icono.pixmap(sz, sz);
        if (!px.isNull()) {
            // Sombra
            p.save(); p.setOpacity(0.15);
            p.drawPixmap(rc.translated(0,3), px);
            p.setOpacity(1.0); p.restore();

            // Ícono con clip redondeado
            QPainterPath clip; clip.addRoundedRect(rc, sz*0.22, sz*0.22);
            p.save(); p.setClipPath(clip); p.drawPixmap(rc, px); p.restore();
        }
    } else {
        // Placeholder
        QLinearGradient g(rc.topLeft(), rc.bottomRight());
        g.setColorAt(0, QColor(60,100,200));
        g.setColorAt(1, QColor(30,60,140));
        QPainterPath ip; ip.addRoundedRect(rc, sz*0.22, sz*0.22);
        p.fillPath(ip, g);
        p.setPen(Qt::white); QFont f; f.setPixelSize(sz/3); f.setBold(true); p.setFont(f);
        p.drawText(rc, Qt::AlignCenter, m_app.nombre.left(1).toUpper());
    }

    // Nombre
    QFont f; f.setFamily("Noto Sans"); f.setPixelSize(11);
    p.setFont(f); p.setPen(QColor(230,232,240));
    QRect textR(2, cy+sz/2+8, width()-4, 30);
    p.drawText(textR, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
               m_app.nombre.length()>18 ? m_app.nombre.left(16)+"…" : m_app.nombre);
}

void AppMenuButton::mousePressEvent(QMouseEvent* e) {
    if (e->button()==Qt::LeftButton) {
        m_anim->stop(); m_anim->setStartValue(m_escala);
        m_anim->setEndValue(0.88); m_anim->start();
    }
    QWidget::mousePressEvent(e);
}

void AppMenuButton::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button()==Qt::LeftButton) {
        m_anim->stop(); m_anim->setStartValue(m_escala);
        m_anim->setEndValue(1.0); m_anim->start();
        if (rect().contains(e->pos())) emit clicado(m_app);
    }
    QWidget::mouseReleaseEvent(e);
}

void AppMenuButton::enterEvent(QEnterEvent* e) {
    m_hover=true;
    m_anim->stop(); m_anim->setStartValue(m_escala);
    m_anim->setEndValue(1.08); m_anim->start();
    update(); QWidget::enterEvent(e);
}

void AppMenuButton::leaveEvent(QEvent* e) {
    m_hover=false;
    m_anim->stop(); m_anim->setStartValue(m_escala);
    m_anim->setEndValue(1.0); m_anim->start();
    update(); QWidget::leaveEvent(e);
}

// ═══════════════════════════════════════════════════════════════════════════
// AppMenu
// ═══════════════════════════════════════════════════════════════════════════

AppMenu::AppMenu(const QList<InfoApp>& apps, QWidget* parent)
    : QWidget(parent), m_todasApps(apps)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);

    // Tamaño: cubre toda la pantalla
    QScreen* s = QApplication::primaryScreen();
    setGeometry(s->geometry());

    auto* mainL = new QVBoxLayout(this);
    mainL->setContentsMargins(0, 60, 0, 60);
    mainL->setSpacing(20);

    // Buscador centrado
    auto* searchRow = new QHBoxLayout;
    searchRow->addStretch();
    m_buscador = new QLineEdit;
    m_buscador->setPlaceholderText("🔍  Buscar aplicaciones…");
    m_buscador->setFixedWidth(420);
    m_buscador->setFixedHeight(46);
    m_buscador->setStyleSheet(R"(
        QLineEdit {
            background: rgba(255,255,255,14);
            border: 1.5px solid rgba(255,255,255,35);
            border-radius: 23px;
            padding: 0 22px;
            color: white;
            font-family: "Noto Sans";
            font-size: 14px;
            selection-background-color: rgba(80,140,255,120);
        }
        QLineEdit:focus {
            border-color: rgba(80,150,255,140);
            background: rgba(255,255,255,18);
        }
    )");
    searchRow->addWidget(m_buscador);
    searchRow->addStretch();
    mainL->addLayout(searchRow);
    connect(m_buscador, &QLineEdit::textChanged, this, &AppMenu::filtrar);

    // Área scroll con grid de apps
    m_scroll = new QScrollArea;
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setStyleSheet("background: transparent; border: none;");
    m_scroll->verticalScrollBar()->setStyleSheet(R"(
        QScrollBar:vertical { width:6px; background:transparent; margin:0; }
        QScrollBar::handle:vertical { background:rgba(255,255,255,40); border-radius:3px; min-height:30px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
    )");

    m_grid = new QWidget;
    m_grid->setAttribute(Qt::WA_TranslucentBackground);
    m_gridLayout = new QGridLayout(m_grid);
    m_gridLayout->setSpacing(6);
    m_gridLayout->setContentsMargins(60, 10, 60, 10);
    m_scroll->setWidget(m_grid);
    mainL->addWidget(m_scroll);

    construirGrid(apps);

    // Animación de opacidad
    m_anim = new QPropertyAnimation(this, "opacidad", this);
    m_anim->setDuration(220);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);

    hide();
}

void AppMenu::construirGrid(const QList<InfoApp>& apps) {
    // Calcular columnas según ancho de pantalla
    QScreen* s = QApplication::primaryScreen();
    int cols = qMax(4, (s->geometry().width()-120) / 118);

    int row=0, col=0;
    for (const InfoApp& app : apps) {
        if (app.nombre.isEmpty()) continue;
        auto* btn = new AppMenuButton(app, m_grid);
        connect(btn, &AppMenuButton::clicado, this, [this](const InfoApp& a){
            emit appLanzada(a);
            ocultar();
        });
        m_gridLayout->addWidget(btn, row, col);
        m_botones.append(btn);
        col++;
        if (col >= cols) { col=0; row++; }
    }
}

void AppMenu::filtrar(const QString& texto) {
    QString t = texto.toLower().trimmed();
    int row=0, col=0;
    // Limpiar layout
    while (m_gridLayout->count()) {
        auto* item = m_gridLayout->takeAt(0);
        if (item->widget()) item->widget()->setParent(nullptr);
        delete item;
    }

    QScreen* s = QApplication::primaryScreen();
    int cols = qMax(4, (s->geometry().width()-120) / 118);

    for (AppMenuButton* btn : m_botones) {
        bool visible = t.isEmpty() ||
                       btn->app().nombre.toLower().contains(t) ||
                       btn->app().comandoExec.toLower().contains(t);
        if (visible) {
            m_gridLayout->addWidget(btn, row, col);
            btn->show();
            col++; if (col>=cols) { col=0; row++; }
        } else {
            btn->hide();
        }
    }
}

void AppMenu::setOpacidad(qreal v) {
    m_opacidad = v;
    setWindowOpacity(v);
}

void AppMenu::toggleVisible() {
    isVisible() ? ocultar() : mostrar();
}

void AppMenu::mostrar() {
    show(); raise(); activateWindow();
    m_buscador->clear(); m_buscador->setFocus();
    filtrar("");
    m_anim->stop();
    m_anim->setStartValue(m_opacidad);
    m_anim->setEndValue(1.0);
    m_anim->start();
}

void AppMenu::ocultar() {
    m_anim->stop();
    m_anim->setStartValue(m_opacidad);
    m_anim->setEndValue(0.0);
    connect(m_anim, &QPropertyAnimation::finished, this, [this](){
        hide();
        disconnect(m_anim, &QPropertyAnimation::finished, this, nullptr);
    });
    m_anim->start();
}

void AppMenu::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    // Fondo oscuro translúcido con blur simulado (capas)
    p.fillRect(rect(), QColor(8, 10, 18, 215));
    // Viñeta radial
    QRadialGradient vignette(rect().center(), width()*0.7);
    vignette.setColorAt(0, QColor(0,0,0,0));
    vignette.setColorAt(1, QColor(0,0,0,80));
    p.fillRect(rect(), vignette);
}

void AppMenu::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) ocultar();
    else QWidget::keyPressEvent(e);
}

void AppMenu::mousePressEvent(QMouseEvent* e) {
    // Clic fuera del área de contenido cierra el menú
    QRect contenido = m_scroll->geometry().united(m_buscador->parentWidget()->geometry());
    if (!contenido.contains(e->pos())) ocultar();
    QWidget::mousePressEvent(e);
}
