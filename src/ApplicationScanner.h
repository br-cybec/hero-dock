#pragma once
#include <QObject>
#include <QString>
#include <QIcon>
#include <QList>
#include <QMap>

struct InfoApp {
    QString nombre;
    QString archivoDesktop;
    QString comandoExec;
    QString nombreIcono;
    QIcon   icono;
    QString wmClass;
    bool    estaAnclada  = false;
    bool    estaEjecutando = false;
    QList<quint64> ventanas;
};

class ApplicationScanner : public QObject {
    Q_OBJECT
public:
    explicit ApplicationScanner(QObject* parent = nullptr);
    void escanear();
    QList<InfoApp> todasLasApps() const { return m_apps; }
    InfoApp buscarPorWmClass(const QString& wm) const;
    InfoApp buscarPorDesktop(const QString& df) const;

signals:
    void appsActualizadas();

private:
    InfoApp parsearDesktop(const QString& path);
    QIcon   cargarIcono(const QString& nombre);
    QString limpiarExec(const QString& exec);

    QList<InfoApp>         m_apps;
    QMap<QString,InfoApp>  m_porWmClass;
    QMap<QString,InfoApp>  m_porDesktop;
};
