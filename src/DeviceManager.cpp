#include "DeviceManager.h"
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusMetaType>
#include <QDBusArgument>
#include <QDBusObjectPath>
#include <QDBusMessage>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QIcon>
#include <QDebug>
#include <QStorageInfo>
#include <algorithm>
#include <QRegularExpression>

// ─── Tipos DBus de UDisks2 ───────────────────────────────────────────────
typedef QMap<QString, QVariantMap> InterfaceMap;
typedef QMap<QDBusObjectPath, InterfaceMap> ManagedObjectsMap;

Q_DECLARE_METATYPE(InterfaceMap)
Q_DECLARE_METATYPE(ManagedObjectsMap)

// ─── Constructor ─────────────────────────────────────────────────────────

DeviceManager::DeviceManager(QObject* parent) : QObject(parent) {
    qDBusRegisterMetaType<InterfaceMap>();
    qDBusRegisterMetaType<ManagedObjectsMap>();

    m_udisks2Ok = iniciarUDisks2();

    if (m_udisks2Ok) {
        escanearDispositivos();
    } else {
        // Fallback: monitorear /proc/mounts cada 2s
        m_timerFallback = new QTimer(this);
        m_timerFallback->setInterval(2000);
        connect(m_timerFallback, &QTimer::timeout,
                this, &DeviceManager::escanearMountsFallback);
        m_timerFallback->start();
        escanearMountsFallback();
    }
}

// ─── Iniciar UDisks2 ─────────────────────────────────────────────────────

bool DeviceManager::iniciarUDisks2() {
    QDBusConnection sys = QDBusConnection::systemBus();
    if (!sys.isConnected()) return false;

    // Verificar que UDisks2 está disponible
    QDBusInterface mgr("org.freedesktop.UDisks2",
                       "/org/freedesktop/UDisks2",
                       "org.freedesktop.DBus.ObjectManager",
                       sys);
    if (!mgr.isValid()) return false;

    // Señal: InterfacesAdded — dispositivo conectado/montado
    sys.connect("org.freedesktop.UDisks2",
                "/org/freedesktop/UDisks2",
                "org.freedesktop.DBus.ObjectManager",
                "InterfacesAdded",
                this,
                SLOT(onInterfazAgregada(QDBusObjectPath,QVariantMap)));

    // Señal: InterfacesRemoved — dispositivo desconectado/desmontado
    sys.connect("org.freedesktop.UDisks2",
                "/org/freedesktop/UDisks2",
                "org.freedesktop.DBus.ObjectManager",
                "InterfacesRemoved",
                this,
                SLOT(onInterfazQuitada(QDBusObjectPath,QStringList)));

    return true;
}

// ─── Escaneo inicial de todos los dispositivos ───────────────────────────

void DeviceManager::escanearDispositivos() {
    QDBusConnection sys = QDBusConnection::systemBus();
    QDBusInterface mgr("org.freedesktop.UDisks2",
                       "/org/freedesktop/UDisks2",
                       "org.freedesktop.DBus.ObjectManager",
                       sys);

    QDBusMessage resp = mgr.call("GetManagedObjects");
    if (resp.type() != QDBusMessage::ReplyMessage) return;

    // Parsear todos los objetos
    QDBusArgument arg = resp.arguments().at(0).value<QDBusArgument>();
    ManagedObjectsMap objects;
    arg >> objects;

    for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
        QString path = it.key().path();
        // Solo nos interesan bloques (particiones) con punto de montaje
        if (!path.startsWith("/org/freedesktop/UDisks2/block_devices/")) continue;

        DispositivoInfo info = parsearBloque(path);
        if (!info.id.isEmpty()) {
            agregarOActualizar(info);
        }
    }
}

// ─── Parsear un bloque UDisks2 ───────────────────────────────────────────

DispositivoInfo DeviceManager::parsearBloque(const QString& objectPath) {
    QDBusConnection sys = QDBusConnection::systemBus();

    // Interfaz Block
    QDBusInterface block("org.freedesktop.UDisks2", objectPath,
                         "org.freedesktop.UDisks2.Block", sys);
    if (!block.isValid()) return {};

    QString device    = QString::fromUtf8(block.property("Device").toByteArray());
    QString idLabel   = block.property("IdLabel").toString();
    QString idType    = block.property("IdType").toString();
    QString drivePath = block.property("Drive").value<QDBusObjectPath>().path();
    qint64  size      = block.property("Size").toLongLong();
    bool    hintIgnore= block.property("HintIgnore").toBool();
    bool    hintAuto  = block.property("HintAuto").toBool();

    if (hintIgnore) return {};
    if (device.isEmpty() || device == "/dev/") return {};

    // Interfaz Filesystem (solo si está montado)
    QDBusInterface fs("org.freedesktop.UDisks2", objectPath,
                      "org.freedesktop.UDisks2.Filesystem", sys);
    if (!fs.isValid()) return {};  // No es filesystem, ignorar swap etc.

    // Puntos de montaje
    QVariant mpVar = fs.property("MountPoints");
    QStringList mountPoints;
    if (mpVar.isValid()) {
        // MountPoints es array of array of bytes
        QDBusArgument mpArg = mpVar.value<QDBusArgument>();
        mpArg.beginArray();
        while (!mpArg.atEnd()) {
            QByteArray ba;
            mpArg.beginArray();
            while (!mpArg.atEnd()) {
                uchar c; mpArg >> c;
                if (c != 0) ba.append((char)c);
            }
            mpArg.endArray();
            if (!ba.isEmpty()) mountPoints << QString::fromUtf8(ba);
        }
        mpArg.endArray();
    }

    if (mountPoints.isEmpty()) return {};  // No montado — no mostrar

    QString puntoMontaje = mountPoints.first();

    // Ignorar montajes del sistema
    QStringList sistemaMounts = {"/", "/boot", "/home", "/usr", "/var",
                                  "/tmp", "/run", "/proc", "/sys", "/dev"};
    for (const QString& sm : sistemaMounts) {
        if (puntoMontaje == sm || puntoMontaje.startsWith(sm+"/boot") ||
            puntoMontaje.startsWith("/run/") || puntoMontaje.startsWith("/sys/"))
            return {};
    }
    if (puntoMontaje.startsWith("/snap/")) return {};

    // Interfaz Drive para saber si es extraíble
    bool extraible = true;
    QString vendor, model;
    if (!drivePath.isEmpty() && drivePath != "/") {
        QDBusInterface drive("org.freedesktop.UDisks2", drivePath,
                             "org.freedesktop.UDisks2.Drive", sys);
        if (drive.isValid()) {
            extraible = drive.property("Removable").toBool();
            vendor    = drive.property("Vendor").toString().trimmed();
            model     = drive.property("Model").toString().trimmed();
            // Si no es extraíble y no es /media/ o /mnt/, ignorar
            if (!extraible &&
                !puntoMontaje.startsWith("/media/") &&
                !puntoMontaje.startsWith("/mnt/") &&
                !puntoMontaje.startsWith("/run/media/")) {
                return {};
            }
        }
    }

    DispositivoInfo info;
    info.id           = objectPath;
    info.dispositivo  = device;
    info.puntoMontaje = puntoMontaje;
    info.tipoFS       = idType;
    info.tamanoTotal  = size;
    info.extraible    = extraible;
    info.montado      = true;
    info.drivePath    = drivePath;

    // Etiqueta inteligente
    info.etiqueta = obtenerEtiqueta(objectPath, idLabel,
                                    vendor.isEmpty() ? model : vendor + " " + model);

    // Tipo de ícono
    info.tipoIcono = determinarTipoIcono(objectPath, device, extraible);
    info.icono     = cargarIconoDispositivo(info.tipoIcono);

    // Espacio usado
    QStorageInfo storage(puntoMontaje);
    if (storage.isValid()) {
        info.tamanoTotal = storage.bytesTotal();
        info.tamanoUsado = storage.bytesTotal() - storage.bytesAvailable();
    }

    return info;
}

// ─── Etiqueta legible del dispositivo ────────────────────────────────────

QString DeviceManager::obtenerEtiqueta(const QString& objectPath,
                                        const QString& label,
                                        const QString& modelo) {
    if (!label.isEmpty()) return label;
    if (!modelo.isEmpty()) {
        // Limpiar modelo genérico
        QString m = modelo;
        m.remove("USB Flash Drive");
        m.remove("Storage Device");
        m = m.simplified();
        if (!m.isEmpty() && m.length() > 2) return m;
    }
    // Usar nombre de la partición como último recurso
    QString dev = objectPath.section('/', -1); // e.g. sdb1
    return dev.isEmpty() ? "Dispositivo" : dev.toUpper();
}

// ─── Determinar tipo de ícono ─────────────────────────────────────────────

QString DeviceManager::determinarTipoIcono(const QString& objectPath,
                                            const QString& dev, bool extraible) {
    QDBusConnection sys = QDBusConnection::systemBus();
    QString drivePath;

    QDBusInterface block("org.freedesktop.UDisks2", objectPath,
                         "org.freedesktop.UDisks2.Block", sys);
    if (block.isValid())
        drivePath = block.property("Drive").value<QDBusObjectPath>().path();

    if (!drivePath.isEmpty() && drivePath != "/") {
        QDBusInterface drive("org.freedesktop.UDisks2", drivePath,
                             "org.freedesktop.UDisks2.Drive", sys);
        if (drive.isValid()) {
            QString connIface = drive.property("ConnectionBus").toString();
            QString media     = drive.property("Media").toString();
            QString mediaComp = drive.property("MediaCompatibility").toStringList().join(",");
            bool    optical   = drive.property("Optical").toBool();

            if (optical || mediaComp.contains("optical"))
                return "optical";
            if (connIface == "usb")
                return "usb";
            if (connIface == "sdio" || dev.contains("mmcblk"))
                return "sdcard";
        }
    }

    if (dev.contains("mmcblk")) return "sdcard";
    if (!extraible)             return "hdd";
    return "usb";
}

// ─── Cargar ícono del sistema ─────────────────────────────────────────────

QIcon DeviceManager::cargarIconoDispositivo(const QString& tipo) {
    // Intentar varios nombres de iconos del tema freedesktop
    QStringList candidatos;
    if (tipo == "usb") {
        candidatos = {"drive-removable-media-usb",
                      "drive-removable-media-usb-pendrive",
                      "media-flash",
                      "drive-removable-media",
                      "media-removable"};
    } else if (tipo == "optical") {
        candidatos = {"media-optical",
                      "drive-optical",
                      "media-cdrom"};
    } else if (tipo == "sdcard") {
        candidatos = {"media-flash-sd-mmc",
                      "media-flash",
                      "drive-removable-media"};
    } else {
        candidatos = {"drive-harddisk-usb",
                      "drive-harddisk",
                      "drive-removable-media"};
    }

    for (const QString& name : candidatos) {
        QIcon icon = QIcon::fromTheme(name);
        if (!icon.isNull()) return icon;
    }
    // Fallback absoluto
    return QIcon::fromTheme("drive-removable-media",
           QIcon::fromTheme("folder"));
}

// ─── Gestión interna ──────────────────────────────────────────────────────

void DeviceManager::agregarOActualizar(const DispositivoInfo& info) {
    for (int i = 0; i < m_dispositivos.size(); i++) {
        if (m_dispositivos[i].id == info.id) {
            m_dispositivos[i] = info;
            emit dispositivoActualizado(info);
            return;
        }
    }
    m_dispositivos.append(info);
    emit dispositivoAgregado(info);
}

void DeviceManager::quitarPorId(const QString& id) {
    for (int i = 0; i < m_dispositivos.size(); i++) {
        if (m_dispositivos[i].id == id) {
            m_dispositivos.removeAt(i);
            emit dispositivoQuitado(id);
            return;
        }
    }
}

// ─── Slots D-Bus ─────────────────────────────────────────────────────────

void DeviceManager::onInterfazAgregada(const QDBusObjectPath& path,
                                        const QVariantMap&) {
    QString p = path.path();
    if (!p.startsWith("/org/freedesktop/UDisks2/block_devices/")) return;

    // Pequeño delay para que UDisks2 termine de poblar propiedades
    QTimer::singleShot(500, this, [this, p]() {
        DispositivoInfo info = parsearBloque(p);
        if (!info.id.isEmpty()) {
            agregarOActualizar(info);
        }
    });
}

void DeviceManager::onInterfazQuitada(const QDBusObjectPath& path,
                                       const QStringList& ifaces) {
    if (ifaces.contains("org.freedesktop.UDisks2.Filesystem") ||
        ifaces.contains("org.freedesktop.UDisks2.Block")) {
        quitarPorId(path.path());
    }
}

void DeviceManager::onPropertiesChanged(const QString&,
                                         const QVariantMap&,
                                         const QStringList&) {
    escanearDispositivos();
}

// ─── Desmontar ────────────────────────────────────────────────────────────

bool DeviceManager::desmontar(const QString& id) {
    if (m_udisks2Ok) {
        QDBusConnection sys = QDBusConnection::systemBus();
        QDBusInterface fs("org.freedesktop.UDisks2", id,
                          "org.freedesktop.UDisks2.Filesystem", sys);
        if (fs.isValid()) {
            QVariantMap opts;
            QDBusReply<void> reply = fs.call("Unmount", opts);
            if (reply.isValid()) {
                // También eyectar el drive si es extraíble
                QDBusInterface block("org.freedesktop.UDisks2", id,
                                     "org.freedesktop.UDisks2.Block", sys);
                if (block.isValid()) {
                    QString drivePath = block.property("Drive")
                                        .value<QDBusObjectPath>().path();
                    if (!drivePath.isEmpty() && drivePath != "/") {
                        QDBusInterface drive("org.freedesktop.UDisks2",
                                             drivePath,
                                             "org.freedesktop.UDisks2.Drive",
                                             sys);
                        if (drive.isValid())
                            drive.call("Eject", opts);
                    }
                }
                quitarPorId(id);
                return true;
            }
        }
    }
    // Fallback: umount
    for (const DispositivoInfo& d : m_dispositivos) {
        if (d.id == id) {
            int ret = QProcess::execute("umount", {d.puntoMontaje});
            if (ret == 0) { quitarPorId(id); return true; }
            break;
        }
    }
    return false;
}

// ─── Abrir en gestor de archivos ─────────────────────────────────────────

bool DeviceManager::abrir(const QString& id) {
    for (const DispositivoInfo& d : m_dispositivos) {
        if (d.id == id && !d.puntoMontaje.isEmpty()) {
            // Intentar varios gestores de archivos en orden de preferencia
            QStringList gestores = {
                "xdg-open",       // Genérico (abre con el predeterminado)
                "nautilus",       // GNOME
                "nemo",           // Cinnamon
                "thunar",         // XFCE
                "dolphin",        // KDE
                "pcmanfm",        // LXDE
                "caja"            // MATE
            };
            for (const QString& g : gestores) {
                // xdg-open siempre debería funcionar
                if (g == "xdg-open") {
                    QProcess::startDetached("xdg-open", {d.puntoMontaje});
                    return true;
                }
                // Verificar si el gestor existe
                if (QProcess::execute("which", {g}) == 0) {
                    QProcess::startDetached(g, {d.puntoMontaje});
                    return true;
                }
            }
            // Último recurso: xdg-open igualmente
            QProcess::startDetached("xdg-open", {d.puntoMontaje});
            return true;
        }
    }
    return false;
}

// ─── Fallback: leer /proc/mounts ─────────────────────────────────────────

void DeviceManager::escanearMountsFallback() {
    QFile mounts("/proc/mounts");
    if (!mounts.open(QIODevice::ReadOnly)) return;

    QSet<QString> actuales;
    QStringList sistemaPrefijos = {"/proc", "/sys", "/dev", "/run",
                                    "/snap", "/boot"};

    for (const QByteArray& linea : mounts.readAll().split('\n')) {
        QStringList campos = QString::fromUtf8(linea).split(' ');
        if (campos.size() < 3) continue;

        QString dev   = campos[0];
        QString punto = campos[1];
        QString tipo  = campos[2];

        // Solo dispositivos de bloque reales
        if (!dev.startsWith("/dev/")) continue;

        // Ignorar montajes del sistema
        bool esSistema = false;
        for (const QString& p : sistemaPrefijos)
            if (punto.startsWith(p)) { esSistema = true; break; }
        if (esSistema) continue;
        if (punto == "/") continue;
        if (tipo == "tmpfs" || tipo == "overlay" || tipo == "squashfs") continue;

        actuales.insert(dev);

        if (!m_montajesConocidos.contains(dev)) {
            // Nuevo montaje detectado
            m_montajesConocidos.insert(dev);

            DispositivoInfo info;
            info.id           = dev;
            info.dispositivo  = dev;
            info.puntoMontaje = punto;
            info.tipoFS       = tipo;
            info.montado      = true;

            // Nombre: usar etiqueta del sistema de archivos si existe
            QFile labelFile(QString("/dev/disk/by-label/").section("",0,0));
            // Leer de /sys/block/*/removable
            QString devName = QFileInfo(dev).fileName(); // sdb1 -> sdb1
            QString diskName = devName.remove(QRegularExpression("\\d+$")); // sdb1 -> sdb
            QFile removable(QString("/sys/block/%1/removable").arg(diskName));
            info.extraible = true;
            if (removable.open(QIODevice::ReadOnly)) {
                info.extraible = removable.readAll().trimmed() == "1";
                removable.close();
            }

            // Etiqueta: usar punto de montaje
            info.etiqueta  = QDir(punto).dirName();
            if (info.etiqueta.isEmpty()) info.etiqueta = devName.toUpper();

            // Tipo ícono
            if (dev.contains("mmcblk")) info.tipoIcono = "sdcard";
            else if (info.extraible)    info.tipoIcono = "usb";
            else                        info.tipoIcono = "hdd";
            info.icono = cargarIconoDispositivo(info.tipoIcono);

            // Espacio
            QStorageInfo storage(punto);
            if (storage.isValid()) {
                info.tamanoTotal = storage.bytesTotal();
                info.tamanoUsado = storage.bytesTotal() - storage.bytesAvailable();
            }

            agregarOActualizar(info);
        }
    }
    mounts.close();

    // Detectar los que se desmontaron
    QSet<QString> quitados = m_montajesConocidos - actuales;
    for (const QString& dev : quitados) {
        m_montajesConocidos.remove(dev);
        quitarPorId(dev);
    }
}
