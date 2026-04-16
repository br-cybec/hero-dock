#pragma once
#include <QObject>
#include <QString>
#include <QIcon>
#include <QList>
#include <QTimer>
#include <QDBusObjectPath>
#include <QSet>

// ─── Información de un dispositivo montado ───────────────────────────────
struct DispositivoInfo {
    QString id;             // Identificador único (ruta DBus o dev)
    QString etiqueta;       // Nombre visible: "USB Kingston", "Disco 500GB"...
    QString puntoMontaje;   // /media/user/Kingston
    QString dispositivo;    // /dev/sdb1
    QString tipoFS;         // vfat, ntfs, ext4, exfat...
    QString tipoIcono;      // "usb", "hdd", "optical", "sdcard"
    QIcon   icono;
    qint64  tamanoTotal  = 0;   // bytes
    qint64  tamanoUsado  = 0;   // bytes
    bool    extraible    = true;
    bool    montado      = true;
    bool    esBus        = false;
    QString drivePath;      // ruta DBus del drive padre
};

// ─── Monitor de dispositivos usando UDisks2 via D-Bus ───────────────────
class DeviceManager : public QObject {
    Q_OBJECT
public:
    explicit DeviceManager(QObject* parent = nullptr);

    QList<DispositivoInfo> dispositivosMontados() const { return m_dispositivos; }
    bool desmontar(const QString& id);
    bool abrir(const QString& id);    // Abre en gestor de archivos

signals:
    void dispositivoAgregado(const DispositivoInfo& info);
    void dispositivoQuitado(const QString& id);
    void dispositivoActualizado(const DispositivoInfo& info);

private slots:
    void onInterfazAgregada(const QDBusObjectPath& path,
                            const QVariantMap& ifaces);
    void onInterfazQuitada(const QDBusObjectPath& path,
                           const QStringList& ifaces);
    void onPropertiesChanged(const QString& iface,
                             const QVariantMap& changed,
                             const QStringList& invalidated);
    void escanearDispositivos();

private:
    // UDisks2 helpers
    bool        iniciarUDisks2();
    DispositivoInfo parsearBloque(const QString& objectPath);
    QString     obtenerEtiqueta(const QString& objectPath,
                                const QString& label, const QString& dev);
    QString     determinarTipoIcono(const QString& objectPath,
                                    const QString& dev, bool extraible);
    QIcon       cargarIconoDispositivo(const QString& tipo);
    void        agregarOActualizar(const DispositivoInfo& info);
    void        quitarPorId(const QString& id);

    // Fallback: leer /proc/mounts
    void        escanearMountsFallback();
    QSet<QString> m_montajesConocidos;  // para el fallback

    QList<DispositivoInfo> m_dispositivos;
    QTimer*                m_timerFallback = nullptr;
    bool                   m_udisks2Ok     = false;
};
