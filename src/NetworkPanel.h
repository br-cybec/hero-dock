#pragma once
#include <QWidget>
#include <QPropertyAnimation>
#include <QTimer>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QList>
#include <QProcess>

struct RedInfo {
    QString ssid;
    QString interfaz;
    int     senal     = 0;
    bool    conectada = false;
    bool    segura    = false;
    bool    esWifi    = true;
};

// ─── Tarjeta individual de red ───────────────────────────────────────────
class NetworkCard : public QWidget {
    Q_OBJECT
public:
    explicit NetworkCard(const RedInfo& red, QWidget* parent = nullptr);
    const RedInfo& red() const { return m_red; }
signals:
    void clicado(const RedInfo& red);
protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*)      override;
    void mousePressEvent(QMouseEvent*) override;
private:
    void dibujarSenal(QPainter& p, int x, int y);
    void dibujarEthernet(QPainter& p, int x, int y);
    RedInfo m_red;
    bool    m_hover = false;
};

// ─── Panel flotante de redes ─────────────────────────────────────────────
class NetworkPanel : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal opacidad READ opacidad WRITE setOpacidad)
public:
    explicit NetworkPanel(QWidget* parent = nullptr);

    void toggle(const QPoint& anchorGlobal);
    void mostrar(const QPoint& anchorGlobal);
    void ocultar();
    bool estaVisible() const { return m_visible; }

    qreal opacidad() const     { return m_opacidad; }
    void  setOpacidad(qreal v) { m_opacidad = v; setWindowOpacity(v); }

signals:
    void redConectada(const QString& ssid);

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    bool eventFilter(QObject* o, QEvent* e) override;

private slots:
    void escanear();
    void conectar(const RedInfo& red);

private:
    void construirUI();
    void actualizarLista(const QList<RedInfo>& redes);
    QList<RedInfo> obtenerRedes();
    QList<RedInfo> obtenerRedesWifi();
    QList<RedInfo> obtenerRedesEthernet();

    QWidget*            m_listaWidget;
    QVBoxLayout*        m_listaLayout;
    QScrollArea*        m_scroll;
    QPushButton*        m_btnEscanear;
    QLabel*             m_lblEstado;
    QTimer*             m_timerEscaneo;
    QPropertyAnimation* m_anim;
    qreal               m_opacidad = 0.0;
    bool                m_visible  = false;
};
