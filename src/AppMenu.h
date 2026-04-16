#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QScrollArea>
#include <QGridLayout>
#include <QPropertyAnimation>
#include <QList>
#include "ApplicationScanner.h"

class AppMenuButton;

// ─── Menú flotante de aplicaciones (estilo Deepin/GNOME Activities) ─────────
class AppMenu : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal opacidad READ opacidad WRITE setOpacidad)

public:
    explicit AppMenu(const QList<InfoApp>& apps, QWidget* parent = nullptr);

    void toggleVisible();
    void mostrar();
    void ocultar();

    qreal opacidad() const { return m_opacidad; }
    void  setOpacidad(qreal v);

signals:
    void appLanzada(const InfoApp& app);

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void mousePressEvent(QMouseEvent*) override;

private slots:
    void filtrar(const QString& texto);

private:
    void construirGrid(const QList<InfoApp>& apps);
    void actualizarGrid(const QList<InfoApp>& apps);

    QList<InfoApp>   m_todasApps;
    QLineEdit*       m_buscador;
    QWidget*         m_grid;
    QGridLayout*     m_gridLayout;
    QScrollArea*     m_scroll;
    QPropertyAnimation* m_anim;
    qreal m_opacidad = 0.0;

    QList<AppMenuButton*> m_botones;
};

// ─── Botón individual dentro del menú ───────────────────────────────────────
class AppMenuButton : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal escala READ escala WRITE setEscala)
public:
    explicit AppMenuButton(const InfoApp& app, QWidget* parent = nullptr);
    const InfoApp& app() const { return m_app; }
    void filtrar(bool visible) { setVisible(visible); }

    qreal escala() const { return m_escala; }
    void  setEscala(qreal v) { m_escala = v; update(); }

signals:
    void clicado(const InfoApp& app);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    InfoApp  m_app;
    bool     m_hover   = false;
    qreal    m_escala  = 1.0;
    QPropertyAnimation* m_anim;
};
