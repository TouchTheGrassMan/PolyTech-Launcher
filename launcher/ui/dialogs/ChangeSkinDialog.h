#pragma once

#include <QDialog>
#include <QString>
#include <QVector>

class BaseInstance;
class QLabel;
class QListWidget;
class QRadioButton;
class QPushButton;
class QDragEnterEvent;
class QDropEvent;
class SkinPreview3D;

/**
 * Skin library dialog.
 *
 * The library of named skins is stored ONCE, launcher-wide, under
 *   <dataRoot>/polytechskins/skins/<id>.png  (+ library.json)
 * so you never re-upload skins per instance.
 *
 * "Applying" a skin materialises it into the CURRENT instance
 *   <gameRoot>/polytechskins/skin.png (+ model.txt)   <- what the mod reads
 * and records which library skin is active for that instance in active.txt.
 * So the pool of skins is shared; the active choice is per-instance.
 *
 * If the polytechskins mod isn't present in the instance's mods folder, a
 * warning is shown, but the library can still be managed.
 */
class ChangeSkinDialog : public QDialog {
    Q_OBJECT
   public:
    explicit ChangeSkinDialog(BaseInstance* instance, QWidget* parent = nullptr);

   protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

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

    // Global (launcher-wide) library
    QString libDir() const;       // <dataRoot>/polytechskins
    QString libSkinsDir() const;  // <dataRoot>/polytechskins/skins
    QString libPngFor(const QString& id) const;

    // Per-instance folder the mod reads from
    QString instSkinDir() const;  // <gameRoot>/polytechskins

    void loadLibrary();
    void saveLibrary();
    void loadActive();  // active skin id for THIS instance
    void saveActive();
    void materializeToInstance(const QString& id);  // copy skin -> instance skin.png + model.txt
    void checkMod();                                // warn if the mod isn't in this instance

    void refreshList();
    void updatePreview();
    int currentIndex() const;
    bool addSkinFromFile(const QString& file, const QString& name);

    BaseInstance* m_instance;
    QString m_gameRoot;
    QString m_dataRoot;
    QVector<SkinEntry> m_skins;  // global library
    QString m_activeId;          // active id for THIS instance
    bool m_updating = false;

    QListWidget* m_list;
    SkinPreview3D* m_preview;
    QLabel* m_hint;
    QLabel* m_modWarning;
    QRadioButton* m_classic;
    QRadioButton* m_slim;
    QPushButton* m_renameBtn;
    QPushButton* m_removeBtn;
    QPushButton* m_applyBtn;
};
