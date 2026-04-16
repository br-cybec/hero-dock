#include "DockWindow.h"
#include "DockItem.h"
#include "ApplicationScanner.h"
#include "SettingsDialog.h"
#include "AppMenu.h"
#include "SystemPanel.h"
#include "NetworkPanel.h"
#include "NotificationManager.h"
#include "DeviceManager.h"
#include "DockDeviceItem.h"
#include "x11util.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QScreen>
#include <QApplication>
#include <QMouseEvent>
#include <QMenu>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTimer>
#include <cmath>

static const QString ESTILO_MENU = R"(
QMenu {
    background-color: rgba(18,20,30,242);
    border: 1px solid rgba(80,140,255,60);
    border-radius: 12px;
    padding: 6px;
    color: #e8eaf0;
    font-family: "Noto Sans";
    font-size: 13px;
}
QMenu::item { padding: 8px 24px; border-radius: 6px; }
QMenu::item:selected { background: rgba(80,130,255,45); color: white; }
QMenu::item:disabled { color: rgba(160,170,200,100); }
QMenu::separator { height:1px; background:rgba(100,120,200,30); margin:5px 12px; }
)";

// ═══════════════════════════════════════════════════════════════════════════
// CONSTRUCTOR
// ═══════════════════════════════════════════════════════════════════════════

DockWindow::DockWindow(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                   Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setMouseTracking(true);

    QString cfgPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                      + "/qdock/qdock.conf";
    QDir().mkpath(QFileInfo(cfgPath).dir().absolutePath());
    m_settings = new QSettings(cfgPath, QSettings::IniFormat, this);
    m_scanner  = new ApplicationScanner(this);

    // Paneles flotantes
    m_notifMgr = new NotificationManager(this);
    m_netPanel = new NetworkPanel(nullptr);
    m_sysPanel = new SystemPanel(this, nullptr);
    m_sysPanel->setNotifManager(m_notifMgr);
    m_sysPanel->setNetworkPanel(m_netPanel);
    connect(m_sysPanel, &SystemPanel::pedirPreferencias,
            this, &DockWindow::abrirPreferencias);

    // Cuando cambien notificaciones, actualizar el panel
    connect(m_notifMgr, &NotificationManager::contadorCambiado,
            m_sysPanel, &SystemPanel::actualizarNotificaciones);

    // Device manager
    m_devMgr = new DeviceManager(this);
    connect(m_devMgr, &DeviceManager::dispositivoAgregado,
            this, &DockWindow::onDispositivoAgregado);
    connect(m_devMgr, &DeviceManager::dispositivoQuitado,
            this, &DockWindow::onDispositivoQuitado);
    connect(m_devMgr, &DeviceManager::dispositivoActualizado,
            this, [this](const DispositivoInfo& info){
        for (DockDeviceItem* it : m_deviceItems)
            if (it->id() == info.id) { it->actualizarInfo(info); break; }
    });

    // Timers
    m_timerWM = new QTimer(this);
    m_timerWM->setInterval(800);
    connect(m_timerWM, &QTimer::timeout, this, &DockWindow::actualizarVentanas);

    m_timerAutoOcultar = new QTimer(this);
    m_timerAutoOcultar->setSingleShot(true);
    m_timerAutoOcultar->setInterval(600);
    connect(m_timerAutoOcultar, &QTimer::timeout, this, &DockWindow::ocultarDock);

    m_timerResetMag = new QTimer(this);
    m_timerResetMag->setSingleShot(true);
    m_timerResetMag->setInterval(80);
    connect(m_timerResetMag, &QTimer::timeout, this, &DockWindow::resetMagnificacion);

    m_timerPantalla = new QTimer(this);
    m_timerPantalla->setInterval(2000);
    connect(m_timerPantalla, &QTimer::timeout, this, &DockWindow::posicionarDock);
    m_timerPantalla->start();

    m_animSlide = new QPropertyAnimation(this, "posOffset", this);
    m_animSlide->setDuration(230);
    m_animSlide->setEasingCurve(QEasingCurve::InOutCubic);

    cargarConfig();
    cargarAppsAncladas();
    agregarItemsEspeciales();
    reconstruirDock();
    m_timerWM->start();
}

DockWindow::~DockWindow() {
    guardarConfig(); guardarAppsAncladas();
    x11_clear_strut((unsigned long)winId());
}

// ═══════════════════════════════════════════════════════════════════════════
// ÍTEMS ESPECIALES — Layout del dock:
//   [BotonApps] | apps | devices | [Papelera][Reloj] | [BotonMenu]
// ═══════════════════════════════════════════════════════════════════════════

void DockWindow::agregarItemsEspeciales() {
    for (DockItem* it : m_extras) it->deleteLater();
    m_extras.clear();

    auto add = [&](TipoEspecial t) {
        auto* item = new DockItem(t, this, this);
        conectarItem(item);
        item->show();
        m_extras.append(item);
        return item;
    };

    // IZQUIERDA: Botón de apps (abre grid de aplicaciones)
    if (m_cfg.mostrarBotonApps) add(TipoEspecial::BotonApps);

    add(TipoEspecial::Separador);
    // [apps van aquí en el medio]
    // [devices van aquí]
    add(TipoEspecial::Separador);

    // DERECHA: Papelera, Reloj, Botón de menú del sistema
    if (m_cfg.mostrarPapelera) add(TipoEspecial::Papelera);
    if (m_cfg.mostrarReloj)    add(TipoEspecial::Reloj);
    if (m_cfg.mostrarBotonMenu) add(TipoEspecial::BotonMenu);

    // Crear AppMenu
    if (!m_appMenu) {
        m_appMenu = new AppMenu(m_scanner->todasLasApps(), nullptr);
        connect(m_appMenu, &AppMenu::appLanzada, [](const InfoApp& a){
            QProcess::startDetached("/bin/sh", {"-c", a.comandoExec});
        });
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// CONFIG
// ═══════════════════════════════════════════════════════════════════════════

void DockWindow::cargarConfig() {
    m_settings->beginGroup("Dock");
#define LOAD(k,d) m_settings->value(k,d)
    m_cfg.posicion           = (DockPosition)LOAD("posicion",0).toInt();
    m_cfg.comportamiento     = (DockComportamiento)LOAD("comportamiento",0).toInt();
    m_cfg.tamanoIcono        = LOAD("tamanoIcono",52).toInt();
    m_cfg.magnificacion      = LOAD("magnificacion",true).toBool();
    m_cfg.multiplicadorMag   = LOAD("multiplicadorMag",165).toInt();
    m_cfg.fondoTranslucido   = LOAD("fondoTranslucido",true).toBool();
    m_cfg.opacidadFondo      = LOAD("opacidadFondo",0.82).toDouble();
    m_cfg.mostrarIndicadores = LOAD("mostrarIndicadores",true).toBool();
    m_cfg.mostrarEtiquetas   = LOAD("mostrarEtiquetas",true).toBool();
    m_cfg.rebotarAlAbrir     = LOAD("rebotarAlAbrir",true).toBool();
    m_cfg.indiceMonitor      = LOAD("indiceMonitor",0).toInt();
    m_cfg.radioEsquina       = LOAD("radioEsquina",20).toInt();
    m_cfg.espaciadoItems     = LOAD("espaciadoItems",5).toInt();
    m_cfg.mostrarBotonMenu   = LOAD("mostrarBotonMenu",true).toBool();
    m_cfg.mostrarPapelera    = LOAD("mostrarPapelera",true).toBool();
    m_cfg.mostrarReloj       = LOAD("mostrarReloj",true).toBool();
    m_cfg.mostrarBotonApps   = LOAD("mostrarBotonApps",true).toBool();
#undef LOAD
    m_settings->endGroup();
}

void DockWindow::guardarConfig() {
    m_settings->beginGroup("Dock");
#define SAVE(k,v) m_settings->setValue(k,v)
    SAVE("posicion",(int)m_cfg.posicion); SAVE("comportamiento",(int)m_cfg.comportamiento);
    SAVE("tamanoIcono",m_cfg.tamanoIcono); SAVE("magnificacion",m_cfg.magnificacion);
    SAVE("multiplicadorMag",m_cfg.multiplicadorMag); SAVE("fondoTranslucido",m_cfg.fondoTranslucido);
    SAVE("opacidadFondo",m_cfg.opacidadFondo); SAVE("mostrarIndicadores",m_cfg.mostrarIndicadores);
    SAVE("mostrarEtiquetas",m_cfg.mostrarEtiquetas); SAVE("rebotarAlAbrir",m_cfg.rebotarAlAbrir);
    SAVE("indiceMonitor",m_cfg.indiceMonitor); SAVE("radioEsquina",m_cfg.radioEsquina);
    SAVE("espaciadoItems",m_cfg.espaciadoItems); SAVE("mostrarBotonMenu",m_cfg.mostrarBotonMenu);
    SAVE("mostrarPapelera",m_cfg.mostrarPapelera); SAVE("mostrarReloj",m_cfg.mostrarReloj);
    SAVE("mostrarBotonApps",m_cfg.mostrarBotonApps);
#undef SAVE
    m_settings->endGroup();
    m_settings->sync();
}

void DockWindow::cargarAppsAncladas() {
    QStringList defaults;
    for (const QString& c : QStringList{
        "/usr/share/applications/org.gnome.Nautilus.desktop",
        "/usr/share/applications/nautilus.desktop","/usr/share/applications/nemo.desktop",
        "/usr/share/applications/thunar.desktop","/usr/share/applications/firefox.desktop",
        "/usr/share/applications/firefox-esr.desktop",
        "/usr/share/applications/org.gnome.Terminal.desktop",
        "/usr/share/applications/gnome-terminal.desktop","/usr/share/applications/konsole.desktop",
        "/usr/share/applications/org.gnome.gedit.desktop","/usr/share/applications/gedit.desktop"
    }) { if (QFile::exists(c)) defaults<<c; if (defaults.size()>=5) break; }

    m_settings->beginGroup("AppsAncladas");
    QStringList ancladas = m_settings->value("lista", defaults).toStringList();
    m_settings->endGroup();

    for (const QString& df : ancladas) {
        InfoApp info = m_scanner->buscarPorDesktop(df);
        if (info.nombre.isEmpty()) {
            if (!QFile::exists(df)) continue;
            info.archivoDesktop=df; info.nombre=QFileInfo(df).baseName();
            info.icono=QIcon::fromTheme("application-x-executable");
        }
        info.estaAnclada = true;
        auto* item = new DockItem(info, this, this);
        conectarItem(item); item->show();
        m_items.append(item);
    }
}

void DockWindow::guardarAppsAncladas() {
    QStringList lista;
    for (DockItem* it : m_items)
        if (it->esApp() && it->estaAnclada()) lista << it->infoApp().archivoDesktop;
    m_settings->beginGroup("AppsAncladas");
    m_settings->setValue("lista", lista);
    m_settings->endGroup();
    m_settings->sync();
}

void DockWindow::aplicarConfig(const DockConfig& cfg) {
    m_cfg = cfg; guardarConfig();
    x11_clear_strut((unsigned long)winId());
    for (DockItem* it : m_items+m_extras) { it->setTamanoMagnificado(0); it->update(); }
    agregarItemsEspeciales();
    layoutItems(); posicionarDock(); establecerEstrut(); update();
}

// ═══════════════════════════════════════════════════════════════════════════
// ITEMS
// ═══════════════════════════════════════════════════════════════════════════

void DockWindow::conectarItem(DockItem* item) {
    connect(item, &DockItem::clicIzquierdo, this, [this,item](){ alHacerClicItem(item); });
    connect(item, &DockItem::clicDerecho,   this, [this,item](QPoint p){ alHacerClicDerechoItem(item,p); });
    connect(item, &DockItem::clicMedio,     this, [item](){ item->lanzar(); });
}

void DockWindow::reconstruirDock() {
    for (DockItem* it : m_items+m_extras) { disconnect(it,nullptr,this,nullptr); conectarItem(it); }
    layoutItems(); posicionarDock(); establecerEstrut();
}

void DockWindow::anclarApp(const QString& df) {
    for (DockItem* it : m_items)
        if (it->infoApp().archivoDesktop==df) { it->setAnclada(true); return; }
    InfoApp info=m_scanner->buscarPorDesktop(df);
    if (info.nombre.isEmpty()) return;
    info.estaAnclada=true;
    auto* item=new DockItem(info,this,this);
    conectarItem(item); item->show(); m_items.append(item);
    layoutItems(); posicionarDock(); guardarAppsAncladas();
}

void DockWindow::desanclarApp(const QString& df) {
    for (int i=0;i<m_items.size();i++) {
        DockItem* it=m_items[i];
        if (it->infoApp().archivoDesktop==df) {
            if (it->estaEjecutando()) it->setAnclada(false);
            else { m_items.removeAt(i); it->deleteLater(); layoutItems(); posicionarDock(); }
            guardarAppsAncladas(); return;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// LAYOUT — orden: [izq extras] [apps] [devices] [der extras]
// ═══════════════════════════════════════════════════════════════════════════

void DockWindow::layoutItems() {
    bool h     = (m_cfg.posicion==DockPosition::Abajo || m_cfg.posicion==DockPosition::Arriba);
    int  esp   = m_cfg.espaciadoItems;
    int  pad   = m_cfg.padding;
    int  grosor= m_cfg.tamanoIcono + 20 + pad*2;

    // Separar extras: hasta primer separador = izquierda, resto = derecha
    int splitIdx = 0;
    for (int i=1; i<m_extras.size(); i++) {
        if (m_extras[i]->tipoEsp()==TipoEspecial::Separador) { splitIdx=i+1; break; }
    }
    QList<DockItem*> izq = m_extras.mid(0, splitIdx);
    QList<DockItem*> der = m_extras.mid(splitIdx);

    QList<DockDeviceItem*> devVis;
    for (DockDeviceItem* d : m_deviceItems) if (d) devVis.append(d);

    auto vis = [](DockItem* it){ return it->isVisible(); };

    if (h) {
        int totalW = pad;
        for (DockItem* it : izq)     if(vis(it)) totalW += it->tamano().width()  + esp;
        for (DockItem* it : m_items) if(vis(it)) totalW += it->tamano().width()  + esp;
        for (DockDeviceItem* d : devVis)          totalW += d->tamano().width()   + esp;
        for (DockItem* it : der)     if(vis(it)) totalW += it->tamano().width()  + esp;
        if (totalW > pad) totalW -= esp;
        totalW += pad;
        resize(qMax(totalW, 60), grosor);

        int x = pad;
        auto plH = [&](DockItem* it){ if(!vis(it))return; it->setGeometry(x,0,it->tamano().width(),grosor); x+=it->tamano().width()+esp; };
        for (DockItem* it : izq)     plH(it);
        for (DockItem* it : m_items) plH(it);
        for (DockDeviceItem* d : devVis) { d->setParent(this); d->setGeometry(x,0,d->tamano().width(),grosor); d->show(); x+=d->tamano().width()+esp; }
        for (DockItem* it : der)     plH(it);
    } else {
        int totalH = pad;
        for (DockItem* it : izq)     if(vis(it)) totalH += it->tamano().height() + esp;
        for (DockItem* it : m_items) if(vis(it)) totalH += it->tamano().height() + esp;
        for (DockDeviceItem* d : devVis)          totalH += d->tamano().height()  + esp;
        for (DockItem* it : der)     if(vis(it)) totalH += it->tamano().height() + esp;
        if (totalH > pad) totalH -= esp;
        totalH += pad;
        resize(grosor, qMax(totalH, 60));

        int y = pad;
        auto plV = [&](DockItem* it){ if(!vis(it))return; it->setGeometry(0,y,grosor,it->tamano().height()); y+=it->tamano().height()+esp; };
        for (DockItem* it : izq)     plV(it);
        for (DockItem* it : m_items) plV(it);
        for (DockDeviceItem* d : devVis) { d->setParent(this); d->setGeometry(0,y,grosor,d->tamano().height()); d->show(); y+=d->tamano().height()+esp; }
        for (DockItem* it : der)     plV(it);
    }
}

QScreen* DockWindow::pantallaObjetivo() const {
    auto p=QApplication::screens();
    return p[qBound(0,m_cfg.indiceMonitor,p.size()-1)];
}

QRect DockWindow::geometriaDock() const {
    QScreen* s=pantallaObjetivo(); QRect sg=s->geometry();
    int grosor=m_cfg.tamanoIcono+20+m_cfg.padding*2;
    int mg=m_cfg.margen;
    switch(m_cfg.posicion){
    case DockPosition::Abajo:     return {sg.x()+(sg.width()-width())/2,   sg.y()+sg.height()-grosor-mg, width(),grosor};
    case DockPosition::Arriba:    return {sg.x()+(sg.width()-width())/2,   sg.y()+mg,                    width(),grosor};
    case DockPosition::Izquierda: return {sg.x()+mg,                       sg.y()+(sg.height()-height())/2, grosor,height()};
    case DockPosition::Derecha:   return {sg.x()+sg.width()-grosor-mg,     sg.y()+(sg.height()-height())/2, grosor,height()};
    }
    return {};
}

void DockWindow::posicionarDock() {
    layoutItems();
    move(geometriaDock().topLeft());
}

void DockWindow::setPosOffset(int v) {
    m_posOffset=v;
    QRect base=geometriaDock();
    switch(m_cfg.posicion){
    case DockPosition::Abajo:     move(base.x(),base.y()+v);  break;
    case DockPosition::Arriba:    move(base.x(),base.y()-v);  break;
    case DockPosition::Izquierda: move(base.x()-v,base.y());  break;
    case DockPosition::Derecha:   move(base.x()+v,base.y());  break;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// PAINT
// ═══════════════════════════════════════════════════════════════════════════

void DockWindow::paintEvent(QPaintEvent*) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    QRect bg=rect().adjusted(2,2,-2,-2);
    int r=m_cfg.radioEsquina;
    QPainterPath path; path.addRoundedRect(bg,r,r);
    bool h=(m_cfg.posicion==DockPosition::Abajo||m_cfg.posicion==DockPosition::Arriba);
    QLinearGradient grad;
    int alpha=(int)(m_cfg.opacidadFondo*255);
    if(h){grad=QLinearGradient(bg.left(),bg.top(),bg.left(),bg.bottom());grad.setColorAt(0,QColor(48,50,62,qMin(255,alpha+15)));grad.setColorAt(1,QColor(22,24,34,alpha));}
    else {grad=QLinearGradient(bg.left(),bg.top(),bg.right(),bg.top());  grad.setColorAt(0,QColor(48,50,62,qMin(255,alpha+15)));grad.setColorAt(1,QColor(22,24,34,alpha));}
    p.fillPath(path,grad);
    QLinearGradient border(bg.topLeft(),bg.bottomRight());
    border.setColorAt(0,QColor(100,160,255,55));border.setColorAt(0.4,QColor(180,100,255,35));
    border.setColorAt(0.8,QColor(80,200,255,45));border.setColorAt(1,QColor(100,160,255,55));
    p.setPen(QPen(QBrush(border),1.3)); p.drawPath(path);
    if(h){
        QPainterPath shine; shine.addRoundedRect(QRectF(bg.x()+1,bg.y()+1,bg.width()-2,bg.height()/2),r-1,r-1);
        QLinearGradient sg2(bg.topLeft(),QPoint(bg.left(),bg.top()+bg.height()/2));
        sg2.setColorAt(0,QColor(255,255,255,28));sg2.setColorAt(1,QColor(255,255,255,0));
        p.fillPath(shine,sg2);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// WM
// ═══════════════════════════════════════════════════════════════════════════

void DockWindow::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    QTimer::singleShot(150,this,[this]{ configurarWM(); establecerEstrut(); posicionarDock(); });
}
void DockWindow::configurarWM()  { x11_set_dock_type((unsigned long)winId()); }
void DockWindow::limpiarEstrut() { x11_clear_strut((unsigned long)winId()); }
void DockWindow::establecerEstrut() {
    unsigned long wid=(unsigned long)winId(); if(!wid) return;
    QScreen* s=pantallaObjetivo(); QRect sg=s->geometry();
    int grosor=m_cfg.tamanoIcono+20+m_cfg.padding*2+m_cfg.margen;
    x11_set_strut(wid,sg.x(),sg.y(),sg.width(),sg.height(),(int)m_cfg.posicion,grosor);
}

// ═══════════════════════════════════════════════════════════════════════════
// VENTANAS
// ═══════════════════════════════════════════════════════════════════════════

void DockWindow::actualizarVentanas() {
    static const int MAX=256;
    X11WindowInfo wins[MAX];
    int n=x11_get_client_list(wins,MAX);
    QSet<quint64> actuales;
    bool changed=false;

    for(int i=0;i<n;i++){
        quint64 wid=(quint64)wins[i].wid;
        if(wid==(quint64)winId()) continue;
        actuales.insert(wid);
        QString wmClass=QString::fromLocal8Bit(wins[i].wmClass).toLower();
        QString wmName =QString::fromLocal8Bit(wins[i].wmName).toLower();

        DockItem* match=nullptr;
        for(DockItem* it:m_items){
            if(!it->esApp()) continue;
            InfoApp inf=it->infoApp();
            QString iwm=inf.wmClass.toLower(),in2=inf.nombre.toLower();
            QString iex=QFileInfo(inf.comandoExec.split(' ').first()).baseName().toLower();
            if((!iwm.isEmpty()&&(wmClass==iwm||wmName==iwm))||wmClass==in2||wmClass==iex||wmName==in2||wmName==iex)
                {match=it;break;}
        }
        if(!match){
            InfoApp info=m_scanner->buscarPorWmClass(wmClass);
            if(info.nombre.isEmpty()) info=m_scanner->buscarPorWmClass(wmName);
            if(!info.nombre.isEmpty()){
                bool ya=false;
                for(DockItem* it:m_items) if(it->infoApp().archivoDesktop==info.archivoDesktop){ya=true;match=it;break;}
                if(!ya){auto* ni=new DockItem(info,this,this);conectarItem(ni);ni->show();m_items.append(ni);changed=true;match=ni;}
            }
        }
        if(match) match->agregarVentana(wid);
    }
    for(int i=m_items.size()-1;i>=0;i--){
        DockItem* it=m_items[i]; if(!it->esApp()) continue;
        for(quint64 wid:it->ventanas()) if(!actuales.contains(wid)) it->quitarVentana(wid);
        if(!it->estaAnclada()&&!it->estaEjecutando()){m_items.removeAt(i);it->deleteLater();changed=true;}
    }
    quint64 active=(quint64)x11_get_active_window();
    for(DockItem* it:m_items) it->setActiva(active&&it->ventanas().contains(active));
    if(changed){layoutItems();posicionarDock();}
}

// ═══════════════════════════════════════════════════════════════════════════
// MAGNIFICATION
// ═══════════════════════════════════════════════════════════════════════════

void DockWindow::actualizarMagnificacion(const QPoint& mouse) {
    if(!m_cfg.magnificacion) return;
    bool h=(m_cfg.posicion==DockPosition::Abajo||m_cfg.posicion==DockPosition::Arriba);
    int base=m_cfg.tamanoIcono,maxSz=base*m_cfg.multiplicadorMag/100,rango=base*2;
    auto applyMag=[&](DockItem* it){
        if(it->tipoEsp()==TipoEspecial::Separador||it->tipoEsp()==TipoEspecial::Reloj) return;
        QPoint c=it->mapToParent(it->rect().center());
        int dist=h?abs(mouse.x()-c.x()):abs(mouse.y()-c.y());
        int sz=base;
        if(dist<rango){double t=1.0-(double)dist/rango;t=t*t*(3-2*t);sz=base+(int)((maxSz-base)*t);}
        it->setTamanoMagnificado(sz);
    };
    for(DockItem* it:m_items+m_extras) applyMag(it);
    layoutItems();
}

void DockWindow::resetMagnificacion(){
    for(DockItem* it:m_items+m_extras) it->setTamanoMagnificado(0);
    layoutItems(); posicionarDock();
}

// ═══════════════════════════════════════════════════════════════════════════
// AUTO-HIDE
// ═══════════════════════════════════════════════════════════════════════════

void DockWindow::mostrarDock(){
    if(!m_oculto) return; m_oculto=false;
    m_animSlide->stop(); m_animSlide->setStartValue(m_posOffset); m_animSlide->setEndValue(0); m_animSlide->start();
}
void DockWindow::ocultarDock(){
    if(m_oculto||m_mouseAdentro) return; m_oculto=true;
    int grosor=m_cfg.tamanoIcono+20+m_cfg.padding*2;
    m_animSlide->stop(); m_animSlide->setStartValue(m_posOffset); m_animSlide->setEndValue(grosor-3); m_animSlide->start();
}
void DockWindow::enterEvent(QEnterEvent* e){m_mouseAdentro=true;m_timerAutoOcultar->stop();if(m_cfg.comportamiento!=DockComportamiento::SiempreVisible)mostrarDock();QWidget::enterEvent(e);}
void DockWindow::leaveEvent(QEvent* e){m_mouseAdentro=false;if(m_cfg.comportamiento!=DockComportamiento::SiempreVisible)m_timerAutoOcultar->start();m_timerResetMag->start();QWidget::leaveEvent(e);}
void DockWindow::mouseMoveEvent(QMouseEvent* e){actualizarMagnificacion(e->pos());QWidget::mouseMoveEvent(e);}
void DockWindow::mouseReleaseEvent(QMouseEvent* e){
    if(e->button()==Qt::RightButton){
        bool sobre=false;
        for(DockItem* it:m_items+m_extras) if(it->geometry().contains(e->pos())){sobre=true;break;}
        if(!sobre) menuContextualDock(e->globalPosition().toPoint());
    }
    QWidget::mouseReleaseEvent(e);
}

// ═══════════════════════════════════════════════════════════════════════════
// ITEM ACTIONS
// ═══════════════════════════════════════════════════════════════════════════

void DockWindow::alHacerClicItem(DockItem* item){
    switch(item->tipoEsp()){
    case TipoEspecial::BotonApps:
        if(m_appMenu) m_appMenu->toggleVisible();
        return;
    case TipoEspecial::BotonMenu: {
        QPoint anchor=item->mapToGlobal(item->rect().center());
        if(m_sysPanel) m_sysPanel->toggle(anchor);
        return;
    }
    case TipoEspecial::Papelera:
        QProcess::startDetached("xdg-open",{"trash://"});
        return;
    case TipoEspecial::Reloj:
        QProcess::startDetached("bash",{"-c","gnome-calendar||kcalc||orage&>/dev/null&"});
        return;
    default: break;
    }
    if(item->estaEjecutando()){
        auto wins=item->ventanas();
        if(!wins.isEmpty()) x11_activate_window((unsigned long)wins.first());
    } else {
        item->lanzar();
    }
}

void DockWindow::alHacerClicDerechoItem(DockItem* item,const QPoint& pos){
    menuContextualItem(item,pos);
}

// ═══════════════════════════════════════════════════════════════════════════
// MENUS CONTEXTUALES
// ═══════════════════════════════════════════════════════════════════════════

void DockWindow::menuContextualItem(DockItem* item,const QPoint& pos){
    QMenu menu; menu.setStyleSheet(ESTILO_MENU);
    auto* hdr=menu.addAction(item->esApp()?item->infoApp().nombre:"Panel del sistema");
    hdr->setEnabled(false); QFont f=hdr->font();f.setBold(true);f.setPixelSize(14);hdr->setFont(f);
    menu.addSeparator();

    if(item->tipoEsp()==TipoEspecial::BotonApps){
        menu.addAction("Abrir lanzador",[this]{if(m_appMenu)m_appMenu->mostrar();});
    } else if(item->tipoEsp()==TipoEspecial::BotonMenu){
        menu.addAction("Abrir panel del sistema",[this,pos]{if(m_sysPanel)m_sysPanel->mostrar(pos);});
    } else if(item->tipoEsp()==TipoEspecial::Papelera){
        menu.addAction("Abrir Papelera",[]{QProcess::startDetached("xdg-open",{"trash://"});});
        menu.addAction("Vaciar Papelera",[]{QProcess::startDetached("bash",{"-c","gio trash --empty"});});
    } else if(item->esApp()){
        if(item->estaAnclada())
            menu.addAction("✕  Quitar del Dock",[this,item]{desanclarApp(item->infoApp().archivoDesktop);});
        else
            menu.addAction("📌  Mantener en el Dock",[this,item]{anclarApp(item->infoApp().archivoDesktop);});

        if(!item->estaEjecutando()){
            menu.addAction("▶  Abrir",[item]{item->lanzar();});
        } else {
            menu.addAction("⊞  Nueva Ventana",[item]{item->lanzar();});
            auto wins=item->ventanas();
            if(wins.size()>1){
                menu.addSeparator();
                int n=1;
                for(quint64 wid:wins){
                    char titulo[512]={};
                    if(!x11_get_window_title((unsigned long)wid,titulo,512))snprintf(titulo,512,"Ventana %d",n);
                    QString t=QString::fromUtf8(titulo);if(t.length()>42)t=t.left(40)+"…";
                    menu.addAction(t,[wid]{x11_activate_window((unsigned long)wid);});n++;
                }
            }
            menu.addSeparator();
            menu.addAction("✕  Cerrar",[item]{for(quint64 wid:item->ventanas())x11_close_window((unsigned long)wid);});
        }
    }
    menu.addSeparator();
    menu.addAction("⚙  Preferencias del Dock…",[this]{abrirPreferencias();});
    menu.exec(pos);
}

void DockWindow::menuContextualDock(const QPoint& pos){
    QMenu menu; menu.setStyleSheet(ESTILO_MENU);
    menu.addAction("⚙  Preferencias del Dock…",[this]{abrirPreferencias();});
    auto* pm=menu.addMenu("Posición");pm->setStyleSheet(ESTILO_MENU);
    pm->addAction("⬇  Abajo",    [this]{DockConfig c=m_cfg;c.posicion=DockPosition::Abajo;    aplicarConfig(c);});
    pm->addAction("⬆  Arriba",   [this]{DockConfig c=m_cfg;c.posicion=DockPosition::Arriba;   aplicarConfig(c);});
    pm->addAction("⬅  Izquierda",[this]{DockConfig c=m_cfg;c.posicion=DockPosition::Izquierda;aplicarConfig(c);});
    pm->addAction("➡  Derecha",  [this]{DockConfig c=m_cfg;c.posicion=DockPosition::Derecha;  aplicarConfig(c);});
    menu.exec(pos);
}

void DockWindow::abrirPreferencias(){
    m_timerWM->stop();
    SettingsDialog dlg(m_cfg,m_scanner->todasLasApps(),m_items,this);
    if(dlg.exec()==QDialog::Accepted){
        aplicarConfig(dlg.config());
        QStringList nuevas=dlg.appsAncladas();
        for(DockItem* it:m_items){
            if(!it->esApp()||!it->estaAnclada()) continue;
            if(!nuevas.contains(it->infoApp().archivoDesktop)){
                it->setAnclada(false);
                if(!it->estaEjecutando()){m_items.removeAll(it);it->deleteLater();}
            }
        }
        for(const QString& df:nuevas){
            bool ya=false;
            for(DockItem* it:m_items) if(it->infoApp().archivoDesktop==df){ya=true;it->setAnclada(true);break;}
            if(!ya) anclarApp(df);
        }
        layoutItems(); posicionarDock(); guardarAppsAncladas();
    }
    m_timerWM->start();
}

// ═══════════════════════════════════════════════════════════════════════════
// DISPOSITIVOS
// ═══════════════════════════════════════════════════════════════════════════

void DockWindow::onDispositivoAgregado(const DispositivoInfo& info){
    for(DockDeviceItem* d:m_deviceItems) if(d->id()==info.id) return;
    auto* item=new DockDeviceItem(info,this,this);
    connect(item,&DockDeviceItem::clicIzquierdo,[this](const QString&id){if(m_devMgr)m_devMgr->abrir(id);});
    connect(item,&DockDeviceItem::clicDerecho,[this](const QString&id,const QPoint&pos){
        for(DockDeviceItem*d:m_deviceItems) if(d->id()==id){menuContextualDispositivo(d,pos);return;}});
    m_deviceItems.append(item);
    layoutItems(); posicionarDock(); establecerEstrut();
    QTimer::singleShot(50,item,[item]{item->animarEntrada();});
}

void DockWindow::onDispositivoQuitado(const QString& id){
    for(int i=0;i<m_deviceItems.size();i++){
        DockDeviceItem* item=m_deviceItems[i];
        if(item->id()==id){
            m_deviceItems.removeAt(i);
            item->animarSalida([this,item]{item->deleteLater();layoutItems();posicionarDock();establecerEstrut();});
            return;
        }
    }
}

void DockWindow::menuContextualDispositivo(DockDeviceItem* item,const QPoint& pos){
    const DispositivoInfo& info=item->info();
    QMenu menu; menu.setStyleSheet(ESTILO_MENU);
    auto* hdr=menu.addAction(info.etiqueta.isEmpty()?"Dispositivo":info.etiqueta);
    hdr->setEnabled(false); if(!info.icono.isNull())hdr->setIcon(info.icono);
    QFont f=hdr->font();f.setBold(true);f.setPixelSize(14);hdr->setFont(f);
    menu.addSeparator();
    menu.addAction("📂  Abrir en gestor de archivos",[this,id=info.id]{if(m_devMgr)m_devMgr->abrir(id);});
    menu.addAction("⬛  Abrir terminal aquí",[mp=info.puntoMontaje]{
        QProcess::startDetached("bash",{"-c",QString("cd '%1' && (x-terminal-emulator||gnome-terminal||konsole||xterm)&").arg(mp)});
    });
    menu.addSeparator();
    menu.addAction(info.extraible?"⏏  Expulsar":"⏏  Desmontar",[this,id=info.id]{if(m_devMgr)m_devMgr->desmontar(id);});
    menu.exec(pos);
}
