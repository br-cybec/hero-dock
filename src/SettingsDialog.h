#pragma once
#include <QDialog>
#include <QComboBox>
#include <QSlider>
#include <QCheckBox>
#include <QSpinBox>
#include <QListWidget>
#include <QLabel>
#include "DockConfig.h"
#include "ApplicationScanner.h"

class DockItem;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(const DockConfig& cfg,
                            const QList<InfoApp>& todasApps,
                            const QList<DockItem*>& itemsActuales,
                            QWidget* parent = nullptr);

    DockConfig     config()       const { return m_cfg; }
    QStringList    appsAncladas() const;

private slots:
    void aplicar();

private:
    void construirUI();
    void construirTabApariencia(QWidget* tab);
    void construirTabComportamiento(QWidget* tab);
    void construirTabApps(QWidget* tab);
    void cargarValores();
    void guardarValores();

    DockConfig       m_cfg;
    QList<InfoApp>   m_todasApps;
    QList<DockItem*> m_itemsActuales;

    // Apariencia
    QComboBox*  m_comboPosicion;
    QComboBox*  m_comboMonitor;
    QSlider*    m_sliderIcono;
    QLabel*     m_labelIcono;
    QCheckBox*  m_checkMag;
    QSlider*    m_sliderMag;
    QLabel*     m_labelMag;
    QCheckBox*  m_checkTranslucido;
    QSlider*    m_sliderOpacidad;
    QLabel*     m_labelOpacidad;
    QCheckBox*  m_checkIndicadores;
    QCheckBox*  m_checkEtiquetas;
    QSpinBox*   m_spinRadio;

    // Comportamiento
    QComboBox*  m_comboComportamiento;
    QCheckBox*  m_checkRebotar;

    // Apps
    QListWidget* m_listaAncladas;
    QListWidget* m_listaDisponibles;
};
