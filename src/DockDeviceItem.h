#pragma once
#include <QWidget>
#include <QPropertyAnimation>
#include <QTimer>
#include <QIcon>
#include "DeviceManager.h"

class DockWindow;

// ─── Ícono de dispositivo en el dock ─────────────────────────────────────
class DockDeviceItem : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal escala  READ escala  WRITE setEscala)
    Q_PROPERTY(qreal aparece READ aparece WRITE setAparece)

public:
    explicit DockDeviceItem(const DispositivoInfo& info,
                            DockWindow* dock,
                            QWidget* parent = nullptr);

    const DispositivoInfo& info() const { return m_info; }
    QString id() const { return m_info.id; }
    void actualizarInfo(const DispositivoInfo& info);
    QSize tamano() const;

    qreal escala()  const { return m_escala;  }
    qreal aparece() const { return m_aparece; }
    void setEscala(qreal v)  { m_escala  = v; update(); }
    void setAparece(qreal v) { m_aparece = v; setWindowOpacity(v); update(); }

    // Animar entrada
    void animarEntrada();
    // Animar salida y luego deleteLater
    void animarSalida(std::function<void()> onDone);

signals:
    void clicIzquierdo(const QString& id);
    void clicDerecho(const QString& id, const QPoint& pos);
    void pedirDesmontar(const QString& id);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    void dibujarEtiqueta(QPainter& p);
    void dibujarBarra(QPainter& p, const QRect& rc);
    QString formatearTamano(qint64 bytes) const;

    DispositivoInfo m_info;
    DockWindow*     m_dock;

    qreal  m_escala  = 1.0;
    qreal  m_aparece = 0.0;
    bool   m_hover   = false;
    bool   m_presion = false;
    bool   m_mostrarEtiqueta = false;

    QPropertyAnimation* m_animEscala  = nullptr;
    QPropertyAnimation* m_animAparece = nullptr;
    QTimer*             m_timerEtiq   = nullptr;
};
