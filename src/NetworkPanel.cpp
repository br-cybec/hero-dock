#include "NetworkPanel.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QScreen>
#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QFont>
#include <algorithm>
#include <cmath>

// ═══════════════════════════════════════════════════════════════════════════
// NetworkCard
// ═══════════════════════════════════════════════════════════════════════════

NetworkCard::NetworkCard(const RedInfo& red, QWidget* parent)
    : QWidget(parent), m_red(red)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedHeight(58);
    setCursor(Qt::PointingHandCursor);
}

void NetworkCard::dibujarSenal(QPainter& p, int x, int y) {
    p.setPen(Qt::NoPen);
    int niveles = m_red.senal>=75?4 : m_red.senal>=50?3 : m_red.senal>=25?2 : 1;
    for (int i=0; i<4; i++) {
        QColor c = i<niveles
            ? (m_red.conectada?QColor(80,160,255,220):QColor(160,170,200,220))
            : QColor(80,90,120,100);
        p.setBrush(c);
        int sz=(i+1)*5+2;
        QRectF arc(x+10-sz, y+18-sz, sz*2, sz*2);
        p.drawChord(arc, 30*16, 120*16);
    }
    p.setBrush(m_red.conectada?QColor(80,160,255):QColor(160,170,200));
    p.drawEllipse(x+9, y+16, 3, 3);
}

void NetworkCard::dibujarEthernet(QPainter& p, int x, int y) {
    p.setPen(QPen(m_red.conectada?QColor(80,200,120):QColor(140,150,180),2));
    int cx=x+11, cy=y+10;
    p.drawRect(cx-7,cy-4,14,10);
    p.drawLine(cx-5,cy-4,cx-5,cy-6); p.drawLine(cx-2,cy-4,cx-2,cy-7);
    p.drawLine(cx+1,cy-4,cx+1,cy-7); p.drawLine(cx+4,cy-4,cx+4,cy-6);
}

void NetworkCard::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing|QPainter::TextAntialiasing);
    QRect rc=rect().adjusted(1,2,-1,-2);
    QPainterPath bg; bg.addRoundedRect(rc,10,10);
    QColor bgColor = m_red.conectada ? QColor(60,120,255,55)
                    : m_hover        ? QColor(255,255,255,18)
                                     : QColor(255,255,255,8);
    p.fillPath(bg, bgColor);
    if (m_red.conectada) { p.setPen(QPen(QColor(80,150,255,80),1)); p.drawPath(bg); }

    int padX=14;
    if (m_red.esWifi) dibujarSenal(p,padX,rc.height()/2-10);
    else              dibujarEthernet(p,padX,rc.height()/2-10);

    int iconRight=padX+30;
    QFont fName; fName.setFamily("Noto Sans"); fName.setPixelSize(13); fName.setBold(m_red.conectada);
    p.setFont(fName);
    p.setPen(m_red.conectada?QColor(150,200,255):QColor(220,225,240));
    QString nombre = m_red.ssid.isEmpty()?(m_red.esWifi?"Red oculta":m_red.interfaz):m_red.ssid;
    p.drawText(QRect(iconRight+10,8,width()-iconRight-50,20),Qt::AlignLeft|Qt::AlignVCenter,nombre);

    QFont fSub; fSub.setFamily("Noto Sans"); fSub.setPixelSize(10);
    p.setFont(fSub);
    p.setPen(m_red.conectada?QColor(100,180,255,200):QColor(140,150,180));
    QString sub = m_red.conectada ? "Conectada"
                : m_red.esWifi   ? QString("Senal: %1%").arg(m_red.senal)
                                  : "Desconectada";
    p.drawText(QRect(iconRight+10,28,width()-iconRight-50,16),Qt::AlignLeft|Qt::AlignVCenter,sub);

    if (m_red.segura && m_red.esWifi) {
        QFont fl; fl.setPixelSize(11); p.setFont(fl); p.setPen(QColor(180,190,220));
        p.drawText(QRect(width()-34,0,20,height()),Qt::AlignCenter,QString::fromUtf8("\xF0\x9F\x94\x92"));
    }
    if (!m_red.conectada) {
        p.setPen(QColor(80,130,255,m_hover?200:100));
        QFont fa; fa.setPixelSize(18); p.setFont(fa);
        p.drawText(QRect(width()-28,0,22,height()),Qt::AlignCenter,"›");
    }
}

void NetworkCard::enterEvent(QEnterEvent* e) { m_hover=true;  update(); QWidget::enterEvent(e); }
void NetworkCard::leaveEvent(QEvent* e)      { m_hover=false; update(); QWidget::leaveEvent(e); }
void NetworkCard::mousePressEvent(QMouseEvent* e) {
    if (e->button()==Qt::LeftButton) emit clicado(m_red);
    QWidget::mousePressEvent(e);
}

// ═══════════════════════════════════════════════════════════════════════════
// NetworkPanel
// ═══════════════════════════════════════════════════════════════════════════

NetworkPanel::NetworkPanel(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::Window|Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint|Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setFixedWidth(320);

    m_anim = new QPropertyAnimation(this,"opacidad",this);
    m_anim->setDuration(200);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);

    m_timerEscaneo = new QTimer(this);
    m_timerEscaneo->setInterval(8000);
    connect(m_timerEscaneo,&QTimer::timeout,this,&NetworkPanel::escanear);

    construirUI();
    qApp->installEventFilter(this);
    hide();
}

void NetworkPanel::construirUI() {
    auto* outerL = new QVBoxLayout(this);
    outerL->setContentsMargins(10,10,10,10);
    outerL->setSpacing(0);

    auto* inner = new QWidget;
    inner->setAttribute(Qt::WA_TranslucentBackground);
    auto* innerL = new QVBoxLayout(inner);
    innerL->setContentsMargins(14,14,14,14);
    innerL->setSpacing(8);

    // Header
    auto* hRow = new QHBoxLayout;
    auto* lblTit = new QLabel("Conexiones de red");
    lblTit->setStyleSheet("color:#e8eaf4;font-family:'Noto Sans';font-size:15px;font-weight:bold;");
    m_btnEscanear = new QPushButton("Escanear");
    m_btnEscanear->setFixedHeight(28);
    m_btnEscanear->setStyleSheet(
        "QPushButton{background:rgba(80,130,255,50);border:1px solid rgba(80,130,255,90);"
        "border-radius:8px;color:#a0b8ff;font-family:'Noto Sans';font-size:11px;padding:3px 12px;}"
        "QPushButton:hover{background:rgba(80,130,255,80);color:white;}");
    connect(m_btnEscanear,&QPushButton::clicked,this,&NetworkPanel::escanear);
    hRow->addWidget(lblTit); hRow->addStretch(); hRow->addWidget(m_btnEscanear);
    innerL->addLayout(hRow);

    // Lista
    m_scroll = new QScrollArea;
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setStyleSheet("background:transparent;border:none;");
    m_scroll->verticalScrollBar()->setStyleSheet(
        "QScrollBar:vertical{width:5px;background:transparent;}"
        "QScrollBar::handle:vertical{background:rgba(255,255,255,35);border-radius:2px;min-height:20px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");

    m_listaWidget = new QWidget;
    m_listaWidget->setAttribute(Qt::WA_TranslucentBackground);
    m_listaLayout = new QVBoxLayout(m_listaWidget);
    m_listaLayout->setContentsMargins(0,0,0,0);
    m_listaLayout->setSpacing(4);
    m_listaLayout->addStretch();
    m_scroll->setWidget(m_listaWidget);
    innerL->addWidget(m_scroll);

    // Estado
    m_lblEstado = new QLabel("Buscando redes...");
    m_lblEstado->setAlignment(Qt::AlignCenter);
    m_lblEstado->setStyleSheet("color:#8890b0;font-family:'Noto Sans';font-size:11px;padding:4px 0;");
    innerL->addWidget(m_lblEstado);

    outerL->addWidget(inner);
}

// ── Obtener redes ─────────────────────────────────────────────────────────

QList<RedInfo> NetworkPanel::obtenerRedesWifi() {
    QList<RedInfo> lista;
    QProcess proc;
    proc.start("nmcli",{"-t","-f","SSID,SIGNAL,SECURITY,IN-USE","device","wifi","list"});
    proc.waitForFinished(3000);
    if (proc.exitCode()==0) {
        for (const QString& linea : QString::fromUtf8(proc.readAllStandardOutput()).split('\n')) {
            if (linea.trimmed().isEmpty()) continue;
            QStringList pp=linea.split(':');
            if (pp.size()<4) continue;
            RedInfo r;
            r.esWifi=true; r.ssid=pp[0].trimmed(); r.senal=pp[1].trimmed().toInt();
            r.segura=!pp[2].trimmed().isEmpty()&&pp[2].trimmed()!="--";
            r.conectada=pp[3].trimmed()=="*";
            if (!r.ssid.isEmpty()&&r.ssid!="--") lista.append(r);
        }
        return lista;
    }
    // Fallback /proc/net/wireless
    QFile f("/proc/net/wireless");
    if (f.open(QIODevice::ReadOnly)) {
        for (const QString& ln : QString::fromUtf8(f.readAll()).split('\n').mid(2)) {
            QStringList c=ln.simplified().split(' ');
            if (c.size()<4) continue;
            RedInfo r; r.esWifi=true; r.interfaz=c[0].replace(':', QChar());
            r.senal=qBound(0,(int)(c[2].remove('.').toDouble()/70.0*100),100);
            r.ssid=r.interfaz; lista.append(r);
        }
        f.close();
    }
    return lista;
}

QList<RedInfo> NetworkPanel::obtenerRedesEthernet() {
    QList<RedInfo> lista;
    QDir netDir("/sys/class/net");
    for (const QString& iface : netDir.entryList(QDir::Dirs|QDir::NoDotAndDotDot)) {
        if (iface=="lo") continue;
        QFile type(QString("/sys/class/net/%1/type").arg(iface));
        if (!type.open(QIODevice::ReadOnly)) continue;
        int t=type.readAll().trimmed().toInt(); type.close();
        if (t!=1) continue;
        if (QDir(QString("/sys/class/net/%1/wireless").arg(iface)).exists()) continue;
        QFile carrier(QString("/sys/class/net/%1/carrier").arg(iface));
        RedInfo r; r.interfaz=iface; r.esWifi=false; r.ssid="Ethernet: "+iface;
        if (carrier.open(QIODevice::ReadOnly)) { r.conectada=carrier.readAll().trimmed()=="1"; carrier.close(); }
        lista.append(r);
    }
    return lista;
}

QList<RedInfo> NetworkPanel::obtenerRedes() {
    QList<RedInfo> t; t+=obtenerRedesEthernet(); t+=obtenerRedesWifi(); return t;
}

void NetworkPanel::escanear() {
    m_lblEstado->setText("Escaneando...");
    m_btnEscanear->setEnabled(false);
    QTimer::singleShot(100,this,[this](){
        QList<RedInfo> redes=obtenerRedes();
        actualizarLista(redes);
        m_btnEscanear->setEnabled(true);
        if (redes.isEmpty()) m_lblEstado->setText("No se encontraron redes");
        else {
            int conn=0; for(auto&r:redes) if(r.conectada)conn++;
            m_lblEstado->setText(conn>0
                ? QString("Conectado  %1 redes").arg(redes.size())
                : QString("%1 redes disponibles").arg(redes.size()));
        }
    });
}

void NetworkPanel::actualizarLista(const QList<RedInfo>& redes) {
    while (m_listaLayout->count()>1) {
        auto* item=m_listaLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    QList<RedInfo> ord=redes;
    std::sort(ord.begin(),ord.end(),[](const RedInfo&a,const RedInfo&b){
        if(a.conectada!=b.conectada)return a.conectada>b.conectada;
        return a.senal>b.senal;
    });
    for (const RedInfo& r : ord) {
        auto* card=new NetworkCard(r,m_listaWidget);
        connect(card,&NetworkCard::clicado,this,&NetworkPanel::conectar);
        m_listaLayout->insertWidget(m_listaLayout->count()-1,card);
    }
    int h=qMin(300,ord.size()*62+8);
    m_scroll->setFixedHeight(qMax(60,h));
    setFixedHeight(60+qMax(60,h)+40+20);
}

void NetworkPanel::conectar(const RedInfo& red) {
    if (red.conectada) return;
    m_lblEstado->setText(QString("Conectando a %1...").arg(red.ssid.isEmpty()?red.interfaz:red.ssid));
    if (red.esWifi && !red.ssid.isEmpty()) {
        QProcess* proc=new QProcess(this);
        proc->start("nmcli",{"device","wifi","connect",red.ssid});
        connect(proc,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                [this,red,proc](int code,QProcess::ExitStatus){
            if(code==0){m_lblEstado->setText("Conectado a "+red.ssid); emit redConectada(red.ssid); QTimer::singleShot(1500,this,&NetworkPanel::escanear);}
            else m_lblEstado->setText("No se pudo conectar");
            proc->deleteLater();
        });
    } else if (!red.esWifi) {
        QProcess::startDetached("nmcli",{"device","connect",red.interfaz});
        QTimer::singleShot(2000,this,&NetworkPanel::escanear);
    }
}

// ── Visibilidad ────────────────────────────────────────────────────────────

void NetworkPanel::toggle(const QPoint& anchor) { m_visible?ocultar():mostrar(anchor); }

void NetworkPanel::mostrar(const QPoint& anchor) {
    m_visible=true;
    QScreen* s=QApplication::primaryScreen(); QRect sg=s->geometry();
    int x=anchor.x()-width()/2;
    int y=anchor.y()-height()-14;
    if (y<sg.y()+10) y=anchor.y()+14;
    x=qBound(sg.x()+8,x,sg.x()+sg.width()-width()-8);
    move(x,y); show(); raise(); activateWindow();
    m_anim->stop(); m_anim->setStartValue(m_opacidad); m_anim->setEndValue(1.0); m_anim->start();
    escanear(); m_timerEscaneo->start();
}

void NetworkPanel::ocultar() {
    m_visible=false; m_timerEscaneo->stop();
    m_anim->stop(); m_anim->setStartValue(m_opacidad); m_anim->setEndValue(0.0);
    disconnect(m_anim,&QPropertyAnimation::finished,nullptr,nullptr);
    connect(m_anim,&QPropertyAnimation::finished,this,[this](){
        hide(); disconnect(m_anim,&QPropertyAnimation::finished,nullptr,nullptr);
    });
    m_anim->start();
}

void NetworkPanel::paintEvent(QPaintEvent*) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    QRect rc=rect().adjusted(10,10,-10,-10);
    QPainterPath path; path.addRoundedRect(rc,16,16);
    p.fillPath(path,QColor(18,20,30,242));
    QLinearGradient border(rc.topLeft(),rc.bottomRight());
    border.setColorAt(0,QColor(80,140,255,70)); border.setColorAt(0.5,QColor(140,80,255,50)); border.setColorAt(1,QColor(80,200,255,70));
    p.setPen(QPen(QBrush(border),1.3)); p.drawPath(path);
    QPainterPath shine; shine.addRoundedRect(QRectF(rc.x()+1,rc.y()+1,rc.width()-2,rc.height()/3.0),15,15);
    QLinearGradient sg(rc.topLeft(),QPoint(rc.left(),rc.top()+rc.height()/3));
    sg.setColorAt(0,QColor(255,255,255,22)); sg.setColorAt(1,QColor(255,255,255,0));
    p.fillPath(shine,sg);
}

void NetworkPanel::keyPressEvent(QKeyEvent* e) {
    if (e->key()==Qt::Key_Escape) ocultar(); else QWidget::keyPressEvent(e);
}

bool NetworkPanel::eventFilter(QObject*, QEvent* e) {
    if (m_visible && e->type()==QEvent::MouseButtonPress) {
        QMouseEvent* me=static_cast<QMouseEvent*>(e);
        if (!geometry().contains(me->globalPosition().toPoint())) ocultar();
    }
    return false;
}

#include "NetworkPanel.moc"
