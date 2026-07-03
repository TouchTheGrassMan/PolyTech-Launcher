#pragma once

#include <QDialog>
#include <QString>
#include <QVector>

class BaseInstance;
class QLabel;
class QListWidget;
class QRadioButton;
class QPushButton;
class SkinPreview3D;

/**
 * Skin library dialog for an instance.
 *
 * The user keeps a named library of skins under
 *   <gameRoot>/polytechskins/skins/<id>.png
 * described by <gameRoot>/polytechskins/skins.json. Exactly one skin can be
 * "active": applying it copies its PNG to <gameRoot>/polytechskins/skin.png and
 * writes model.txt. The polytechskins mod still reads those two files, so it
 * needs no changes — the library is purely a launcher-side convenience.
 *
 * The preview is 2D for now; a 3D preview replaces it in a later step.
 */
class ChangeSkinDialog : public QDialog {
    Q_OBJECT
   public:
    explicit ChangeSkinDialog(BaseInstance* instance, QWidget* parent = nullptr);

   private slots:
    void addSkin();
    void renameSkin();
    void removeSkin();
    void applySkin();
    void onSelectionChanged();
    void onModelToggled();

   private:
    struct SkinEntry {
        QString id;
        QString name;
        QString model;  // "classic" or "slim"
    };

    QString baseDir() const;   // <gameRoot>/polytechskins
    QString skinsDir() const;  // <gameRoot>/polytechskins/skins
    QString pngPathFor(const QString& id) const;

    void loadLibrary();
    void saveLibrary();
    void materializeActive();  // copy active skin -> skin.png + model.txt
    void refreshList();
    void updatePreview();
    int currentIndex() const;

    BaseInstance* m_instance;
    QString m_gameRoot;
    QVector<SkinEntry> m_skins;
    QString m_activeId;
    bool m_updating = false;  // guards radio signals during UI refresh

    QListWidget* m_list;
    SkinPreview3D* m_preview;
    QLabel* m_hint;
    QRadioButton* m_classic;
    QRadioButton* m_slim;
    QPushButton* m_renameBtn;
    QPushButton* m_removeBtn;
    QPushButton* m_applyBtn;
};
